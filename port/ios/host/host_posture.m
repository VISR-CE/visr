#include "host_posture_layout.h"
#include <TargetConditionals.h>
#include <pthread.h>

static struct host_posture_snapshot snapshot;
static struct host_posture_layout layout;
static pthread_mutex_t snapshot_lock = PTHREAD_MUTEX_INITIALIZER;

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_TV && !TARGET_OS_VISION
#import <UIKit/UIKit.h>
#import <CoreMotion/CoreMotion.h>
#include "host_orientation.h"
#include "ios_host.h"

static __weak UIView *root_view;
static CMMotionManager *motion;
static struct host_posture_state classifier;

void host_posture_attach(void *view_pointer)
{
    root_view = (__bridge UIView *)view_pointer;
}

void host_posture_poll(void)
{
    if (!NSThread.isMainThread) return;
    UIView *root = root_view;
    UIWindowScene *scene = root.window.windowScene;
    BOOL enabled = host_ios_allows_portrait() && scene &&
        scene.activationState == UISceneActivationStateForegroundActive &&
        !NSProcessInfo.processInfo.isiOSAppOnMac;
    if (enabled) {
        if (!motion) {
            motion = [[CMMotionManager alloc] init];
            motion.deviceMotionUpdateInterval = 0.1;
        }
        if (motion.deviceMotionAvailable && !motion.deviceMotionActive)
            [motion startDeviceMotionUpdates];
    } else if (motion.deviceMotionActive) {
        [motion stopDeviceMotionUpdates];
    }

    struct host_posture_inputs in = {0};
    if (enabled) {
        host_hinge_snapshot(&in.hinge);
        in.view_width = root.bounds.size.width;
        in.view_height = root.bounds.size.height;
        CMDeviceMotion *sample = motion.deviceMotion;
        /* A suspended or failed sensor must become unknown rather than keep
           classifying a new fold with an old gravity vector. */
        double age = NSProcessInfo.processInfo.systemUptime - sample.timestamp;
        if (sample && age >= 0 && age < 0.5) {
            CMAcceleration gravity = sample.gravity;
            in.gravity_known = 1;
            in.gravity_z = gravity.z;
            /* CoreMotion uses portrait device axes; UIKit's interface
               orientation rotates those into the root view's screen axes. */
            switch (scene.effectiveGeometry.interfaceOrientation) {
            case UIInterfaceOrientationPortrait:
                in.gravity_x = gravity.x; in.gravity_y = gravity.y; break;
            case UIInterfaceOrientationPortraitUpsideDown:
                in.gravity_x = -gravity.x; in.gravity_y = -gravity.y; break;
            case UIInterfaceOrientationLandscapeLeft:
                in.gravity_x = -gravity.y; in.gravity_y = gravity.x; break;
            case UIInterfaceOrientationLandscapeRight:
                in.gravity_x = gravity.y; in.gravity_y = -gravity.x; break;
            default:
                in.gravity_known = 0; break;
            }
        }
    }
    enum host_posture next = host_posture_update(&classifier, &in);
    pthread_mutex_lock(&snapshot_lock);
    int posture_changed = snapshot.posture != next;
    int changed = posture_changed || snapshot.upper_half.x != classifier.upper_half.x ||
        snapshot.upper_half.y != classifier.upper_half.y ||
        snapshot.upper_half.width != classifier.upper_half.width ||
        snapshot.upper_half.height != classifier.upper_half.height;
    if (changed) {
        snapshot.posture = next;
        snapshot.upper_half = classifier.upper_half;
        ++snapshot.generation;
    }
    UIEdgeInsets safe = root.safeAreaInsets;
    struct host_posture_layout next_layout = host_posture_layout_make(enabled, snapshot,
        in.hinge, root.bounds.size.width, root.bounds.size.height,
        (struct host_posture_insets){safe.top, safe.left, safe.bottom, safe.right}, root.window.screen.scale);
    next_layout.generation = layout.generation;
    if (host_posture_layout_changed(&next_layout, &layout)) ++next_layout.generation;
    layout = next_layout;
    pthread_mutex_unlock(&snapshot_lock);
    if (posture_changed) {
        const char *names[] = {"flat", "laptop-l", "book-l", "closed"};
        host_logf(HOST_LOG_INFO,
            "posture: %s angle=%.6f rad division=%d frame=(%.3f,%.3f,%.3f,%.3f) pt gravity=%s(%.3f,%.3f,%.3f)",
            names[next], in.hinge.angle, in.hinge.division_active,
            in.hinge.division_x, in.hinge.division_y, in.hinge.division_width, in.hinge.division_height,
            in.gravity_known ? "" : "unknown ", in.gravity_x, in.gravity_y, in.gravity_z);
    }
}
#else
void host_posture_attach(void *root_view) { (void)root_view; }
void host_posture_poll(void) {}
#endif

void host_posture_snapshot(struct host_posture_snapshot *out)
{
    if (!out) return;
    pthread_mutex_lock(&snapshot_lock);
    *out = snapshot;
    pthread_mutex_unlock(&snapshot_lock);
}

void host_posture_layout_snapshot(struct host_posture_layout *out)
{
    if (!out) return;
    pthread_mutex_lock(&snapshot_lock);
    *out = layout;
    pthread_mutex_unlock(&snapshot_lock);
}
