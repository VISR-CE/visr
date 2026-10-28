#include "host_hinge.h"
#include <TargetConditionals.h>
#include <pthread.h>

static struct host_hinge_snapshot snapshot;
static pthread_mutex_t snapshot_lock = PTHREAD_MUTEX_INITIALIZER;

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_TV && !TARGET_OS_VISION && \
    __IPHONE_OS_VERSION_MAX_ALLOWED >= 270100 && __has_include(<UIKit/UIHingeInteraction.h>)
#import <UIKit/UIKit.h>
#include "ios_host.h"

static __weak UIView *root_view;
static __weak UIHingeInteraction *hinge_interaction API_AVAILABLE(ios(27.1));
static int hinge_available;
static enum host_hinge_status hinge_status;
static double hinge_angle;

static void publish(void) API_AVAILABLE(ios(27.1));
static void publish(void)
{
    struct host_hinge_snapshot next = {0};
    UIView *root = root_view;
    if (hinge_available && root.window) {
        next.available = 1;
        next.status = hinge_status;
        next.angle = hinge_angle;
        for (UIViewReservedRegion *region in [root reservedRegionsOfKind:UIViewReservedRegionKind.divisionRegionKind]) {
            if (!region.isActive) continue;
            CGRect frame = region.frame;
            next.division_active = 1;
            next.division_x = frame.origin.x;
            next.division_y = frame.origin.y;
            next.division_width = frame.size.width;
            next.division_height = frame.size.height;
            break; /* iPhone Duo has one hinge. */
        }
    }
    pthread_mutex_lock(&snapshot_lock);
    int changed = next.available != snapshot.available || next.status != snapshot.status ||
        next.angle != snapshot.angle || next.division_active != snapshot.division_active ||
        next.division_x != snapshot.division_x || next.division_y != snapshot.division_y ||
        next.division_width != snapshot.division_width || next.division_height != snapshot.division_height;
    if (changed) {
        next.generation = snapshot.generation + 1;
        snapshot = next;
    }
    pthread_mutex_unlock(&snapshot_lock);
    if (changed)
        host_logf(HOST_LOG_INFO, "hinge: available=%d status=%d angle=%.6f rad division=%d frame=(%.3f,%.3f,%.3f,%.3f) pt",
            next.available, next.status, next.angle, next.division_active,
            next.division_x, next.division_y, next.division_width, next.division_height);
}

void host_hinge_attach(void *view_pointer)
{
    if (@available(iOS 27.1, *)) {
        UIView *view = (__bridge UIView *)view_pointer;
        if (view == root_view && hinge_interaction) return;
        UIHingeInteraction *previous = hinge_interaction;
        if (previous) [previous.view removeInteraction:previous];
        hinge_interaction = nil;
        root_view = view;
        hinge_available = 0;
        hinge_status = HOST_HINGE_UNKNOWN;
        hinge_angle = 0;
        publish();
        if (!view || NSProcessInfo.processInfo.isiOSAppOnMac) return;
        UIHingeInteraction *interaction = [[UIHingeInteraction alloc] initWithUpdateHandler:
            ^(UIHingeInteraction *sender, UIHingeInteractionUpdate *update) {
                /* Ignore a final update from the view retired during reattachment. */
                if (sender.view != root_view) return;
                UIHinge *hinge = update.hinge;
                hinge_available = hinge != nil;
                hinge_status = HOST_HINGE_UNKNOWN;
                if (hinge) {
                    switch (hinge.status) {
                    case UIHingeStatusClosed: hinge_status = HOST_HINGE_CLOSED; break;
                    case UIHingeStatusPartiallyOpen: hinge_status = HOST_HINGE_PARTIALLY_OPEN; break;
                    case UIHingeStatusFullyOpen: hinge_status = HOST_HINGE_FULLY_OPEN; break;
                    default: break;
                    }
                }
                hinge_angle = hinge ? hinge.angle : 0;
                publish();
            }];
        hinge_interaction = interaction;
        [view addInteraction:interaction];
    }
}

void host_hinge_poll(void)
{
    if (@available(iOS 27.1, *)) {
        if (NSThread.isMainThread) publish();
    }
}
#else
void host_hinge_attach(void *root_view) { (void)root_view; }
void host_hinge_poll(void) {}
#endif

void host_hinge_snapshot(struct host_hinge_snapshot *out)
{
    if (!out) return;
    pthread_mutex_lock(&snapshot_lock);
    *out = snapshot;
    pthread_mutex_unlock(&snapshot_lock);
}
