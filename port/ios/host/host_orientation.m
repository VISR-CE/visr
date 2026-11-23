#import "host_orientation.h"
#include "ios_host.h"
#include "host_config.h"
#include "host_geometry.h"
#include "host_geometry_debounce.h"
#import <QuartzCore/QuartzCore.h>
#include <pthread.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <stdatomic.h>
#include "host_portrait_rule.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

/* UIKit may ask before host_main.m installs the profile. Only configure
   after that point; early queries keep the existing landscape policy. */
static BOOL allow_portrait;
/* whether the screen the game's scene is on fits portrait (host_portrait_rule.h): -1 before the first check,
   which leaves the device's own answer */
static _Atomic int screen_portrait = -1;
static struct host_geometry_debounce geometry_debounce;
static struct host_geometry settled_geometry;
static unsigned settled_generation;
static pthread_mutex_t geometry_lock = PTHREAD_MUTEX_INITIALIZER;

unsigned host_geometry_settled_generation(void) {
    return host_geometry_settled_size(NULL, NULL);
}

unsigned host_geometry_settled_size(double *width, double *height) {
    pthread_mutex_lock(&geometry_lock);
    if (width) *width = settled_geometry.width;
    if (height) *height = settled_geometry.height;
    unsigned generation = settled_generation;
    pthread_mutex_unlock(&geometry_lock);
    return generation;
}

int host_ios_portrait_allowed(void) { return allow_portrait && atomic_load(&screen_portrait) != 0 ? 1 : 0; }

BOOL host_ios_allows_portrait(void) {
    return host_ios_portrait_allowed() != 0;
}

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_VISION
static UIWindowScene *game_window_scene(void) {
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes)
        if ([scene isKindOfClass:UIWindowScene.class] &&
            [scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication])
            return (UIWindowScene *)scene;
    return nil;
}

static void update_orientation_preferences(UIWindowScene *windowScene) {
    for (UIWindow *window in windowScene.windows) {
        UIViewController *controller = window.rootViewController;
        while (controller) {
            [controller setNeedsUpdateOfSupportedInterfaceOrientations];
            if (@available(iOS 26.0, *)) [controller setNeedsUpdateOfPrefersInterfaceOrientationLocked];
            controller = controller.presentedViewController;
        }
    }
}

/* Main thread: take the portrait answer again for the screen the game's scene is on now (a move between the
   cover and inner screens changes it), and when it changes, tell SDL and UIKit; a scene in portrait on a screen
   that no longer allows it turns to landscape. */
static void portrait_screen_check(void) {
    UIWindowScene *windowScene = game_window_scene();
    UIScreen *screen = windowScene.screen;
    CGSize size = screen ? screen.bounds.size : CGSizeZero;
    struct host_portrait_step step = host_portrait_check(atomic_load(&screen_portrait), allow_portrait, screen != nil,
        size.width, size.height, UIInterfaceOrientationIsPortrait(windowScene.effectiveGeometry.interfaceOrientation));
    if (!step.changed) return;
    int fits = step.answer;
    atomic_store(&screen_portrait, fits);
    host_logf(HOST_LOG_INFO, "portrait: %s on this %.0fx%.0f pt screen (at least %.0f pt wide in portrait)",
        fits ? "allowed" : "not allowed", size.width, size.height, HOST_PORTRAIT_MINIMUM_POINTS);
    SDL_SetHint(SDL_HINT_ORIENTATIONS, fits ? "Portrait PortraitUpsideDown LandscapeLeft LandscapeRight" :
        "LandscapeLeft LandscapeRight");
    update_orientation_preferences(windowScene);
    if (step.turn_to_landscape)
        for (UIWindow *window in windowScene.windows)
            if (!window.hidden && window.rootViewController) { host_ios_require_landscape(window); break; }
}
#endif

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_VISION
/* debug.orientation: one orientation, or a timed sequence ("portrait,landscape@20"): the first is requested
   once the first window scene is active, each later one at its seconds after that. */
#define DEBUG_ORIENTATION_STEPS 4
static char debug_orientation[64];
static struct { BOOL portrait; double seconds; } debug_steps[DEBUG_ORIENTATION_STEPS];
static int debug_step_count;
static BOOL orientation_requested;

static BOOL debug_orientation_parse(const char *text) {
    char copy[sizeof(debug_orientation)];
    snprintf(copy, sizeof(copy), "%s", text);
    debug_step_count = 0;
    for (char *save = NULL, *item = strtok_r(copy, ",", &save); item; item = strtok_r(NULL, ",", &save)) {
        if (debug_step_count == DEBUG_ORIENTATION_STEPS) return NO;
        char *at = strchr(item, '@');
        double seconds = 0;
        if (at) { *at = 0; seconds = atof(at + 1); }
        if (strcmp(item, "portrait") && strcmp(item, "landscape")) return NO;
        if (debug_step_count && seconds <= debug_steps[debug_step_count - 1].seconds) return NO;
        debug_steps[debug_step_count].portrait = !strcmp(item, "portrait");
        debug_steps[debug_step_count].seconds = seconds;
        debug_step_count++;
    }
    return debug_step_count > 0 && debug_steps[0].seconds == 0;
}

static void request_debug_step(UIWindowScene *scene, int step) {
    BOOL portrait = debug_steps[step].portrait;
    if (portrait && !allow_portrait) {
        host_logf(HOST_LOG_INFO, "orientation request: portrait requires display.allow_portrait");
        return;
    }
    UIInterfaceOrientationMask mask = portrait ? UIInterfaceOrientationMaskPortrait : UIInterfaceOrientationMaskLandscape;
    host_logf(HOST_LOG_INFO, "orientation request: %s (step %d of %d)", portrait ? "portrait" : "landscape",
        step + 1, debug_step_count);
    UIWindowSceneGeometryPreferencesIOS *geometry = [[UIWindowSceneGeometryPreferencesIOS alloc]
        initWithInterfaceOrientations:mask];
    [scene requestGeometryUpdateWithPreferences:geometry errorHandler:^(NSError *error) {
        host_logf(HOST_LOG_WARN, "orientation request: %s", error.localizedDescription.UTF8String);
    }];
}

static void request_debug_orientation(UIWindowScene *scene) {
    if (orientation_requested || !debug_step_count ||
        scene.activationState != UISceneActivationStateForegroundActive ||
        ![scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication]) return;
    orientation_requested = YES;
    request_debug_step(scene, 0);
    for (int step = 1; step < debug_step_count; step++) {
        __weak UIWindowScene *weak_scene = scene;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(debug_steps[step].seconds * NSEC_PER_SEC)),
            dispatch_get_main_queue(), ^{
                UIWindowScene *target = weak_scene;
                if (target) request_debug_step(target, step);
            });
    }
}
#endif

void host_ios_orientation_configure(void) {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_VISION
    static BOOL configured;
    if (configured || !host_config_profile_installed()) return;
    configured = YES;
    char text[16];
    const char *environment = getenv("HALO_ALLOW_PORTRAIT");
    host_config_string("display.allow_portrait", "false", text, sizeof(text));
    const char *value = environment ? environment : text;
    /* Match port_config.c's config_text_is_false for environment overrides. */
    allow_portrait = strcasecmp(value, "false") && strcasecmp(value, "0") &&
        strcasecmp(value, "no") && strcasecmp(value, "off");
    environment = getenv("HALO_ORIENTATION");
    host_config_string("debug.orientation", "", debug_orientation, sizeof(debug_orientation));
    if (environment) snprintf(debug_orientation, sizeof(debug_orientation), "%s", environment);
    if (debug_orientation[0] && !debug_orientation_parse(debug_orientation)) {
        host_logf(HOST_LOG_WARN, "orientation request: unknown orientation %s", debug_orientation);
        debug_orientation[0] = 0;
        debug_step_count = 0;
    }
    if (allow_portrait)
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait PortraitUpsideDown LandscapeLeft LandscapeRight");
    dispatch_async(dispatch_get_main_queue(), ^{
        portrait_screen_check();
        for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
            if (![scene isKindOfClass:UIWindowScene.class]) continue;
            UIWindowScene *windowScene = (UIWindowScene *)scene;
            update_orientation_preferences(windowScene);
            request_debug_orientation(windowScene);
        }
        [NSNotificationCenter.defaultCenter addObserverForName:UISceneDidActivateNotification object:nil
            queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *notification) {
                if ([notification.object isKindOfClass:UIWindowScene.class]) {
                    /* (the first check above finds no scene when configure runs before one connects) */
                    portrait_screen_check();
                    request_debug_orientation(notification.object);
                }
            }];
    });
#endif
}

UIInterfaceOrientation host_ios_presentation_orientation(void) {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_VISION
    if (host_ios_allows_portrait()) {
        for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
            if (![scene isKindOfClass:UIWindowScene.class] ||
                ![scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication]) continue;
            UIInterfaceOrientation orientation = ((UIWindowScene *)scene).effectiveGeometry.interfaceOrientation;
            if (orientation != UIInterfaceOrientationUnknown) return orientation;
        }
    }
#endif
    return UIInterfaceOrientationLandscapeRight;
}

static const char *orientation_name(UIInterfaceOrientation orientation) {
    switch (orientation) {
        case UIInterfaceOrientationPortrait: return "portrait";
        case UIInterfaceOrientationPortraitUpsideDown: return "portrait-upside-down";
        case UIInterfaceOrientationLandscapeLeft: return "landscape-left";
        case UIInterfaceOrientationLandscapeRight: return "landscape-right";
        default: return "unknown";
    }
}

/* A move between the cover and inner screens (either direction, any orientation)
   can leave SDL's view, the layer's drawable and the SDL window at the old
   screen's size. After the geometry settles, force the view to layout and make
   the layer and the window agree with the scene's bounds. */
static double last_relayout_log;
static double relayout_again_at;
static void relayout_to_scene(void) {
#if !TARGET_OS_VISION && !TARGET_OS_MACCATALYST && !TARGET_OS_TV
    SDL_Window *sdl = host_sdl_window();
    CAMetalLayer *layer = (__bridge CAMetalLayer *)host_sdl_metal_layer();
    if (!sdl || !layer) return;
    int pinned_w = 0, pinned_h = 0;
    if (host_sdl_output_cap(&pinned_w, &pinned_h) && pinned_w) return;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class] ||
            ![scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication]) continue;
        UIWindowScene *windowScene = (UIWindowScene *)scene;
        for (UIWindow *window in windowScene.windows) {
            if (window.hidden || !window.rootViewController) continue;
            CGRect bounds = windowScene.coordinateSpace.bounds;
            if (!CGRectEqualToRect(window.frame, bounds)) window.frame = bounds;
            UIView *view = window.rootViewController.view;
            BOOL changed = NO;
            if (!CGRectEqualToRect(view.frame, window.bounds)) { view.frame = window.bounds; changed = YES; }
            for (UIView *sub in view.subviews) {
                if (![NSStringFromClass(sub.class) isEqualToString:@"HaloControls"]) continue;
                if (!CGRectEqualToRect(sub.frame, view.bounds)) { sub.frame = view.bounds; [sub setNeedsLayout]; changed = YES; }
            }
            /* The SDL metal view (the layer's delegate) can keep the previous screen's frame while the
               drawable already matches, so SDL never sees a resize. Check the layer's own bounds too. */
            UIView *metal_view = [layer.delegate isKindOfClass:UIView.class] ? (UIView *)layer.delegate : nil;
            BOOL bounds_off = NO;
            if (metal_view && !CGRectEqualToRect(metal_view.frame, view.bounds)) {
                metal_view.frame = view.bounds; [metal_view setNeedsLayout]; bounds_off = YES;
            }
            if (!CGSizeEqualToSize(layer.bounds.size, view.bounds.size)) {
                layer.frame = view.bounds; bounds_off = YES;
            }
            if (bounds_off) changed = YES;
            CGFloat scale = windowScene.screen.nativeScale > 0 ? windowScene.screen.nativeScale : windowScene.screen.scale;
            CGSize want = CGSizeMake(round(bounds.size.width * scale), round(bounds.size.height * scale));
            int width = 0, height = 0;
            SDL_GetWindowSize(sdl, &width, &height);
            /* (a pixel either way is the rounding of a fractional bounds times the scale, which UIKit and SDL
               round their own way: correcting it each frame fought them forever) */
            BOOL layer_off = fabs(layer.drawableSize.width - want.width) > 1.0 ||
                             fabs(layer.drawableSize.height - want.height) > 1.0;
            int pw = 0, ph = 0;
            SDL_GetWindowSizeInPixels(sdl, &pw, &ph);
            BOOL sdl_off = width != (int)bounds.size.width || height != (int)bounds.size.height ||
                           abs(pw - (int)want.width) > 1 || abs(ph - (int)want.height) > 1;
            if (changed) { [view setNeedsLayout]; [view layoutIfNeeded]; }
            if (layer.contentsScale != scale) layer.contentsScale = scale;
            double now = CACurrentMediaTime();
            BOOL log_it = (changed || layer_off || sdl_off) && now - last_relayout_log > 0.25;
            if (log_it) last_relayout_log = now;
            if (layer_off) {
                if (log_it) host_logf(HOST_LOG_INFO, "relayout: drawable %.0fx%.0f -> %.0fx%.0f",
                    layer.drawableSize.width, layer.drawableSize.height, want.width, want.height);
                layer.drawableSize = want;
            }
            if (sdl_off) {
                if (log_it) host_logf(HOST_LOG_INFO, "relayout: SDL window %dx%d -> %.0fx%.0f", width, height,
                    bounds.size.width, bounds.size.height);
                SDL_SetWindowSize(sdl, (int)bounds.size.width, (int)bounds.size.height);
            }
            if (log_it && changed && !layer_off && !sdl_off)
                host_logf(HOST_LOG_INFO, "relayout: views resized to %.0fx%.0f", bounds.size.width, bounds.size.height);
            return;
        }
    }
#endif
}

void host_geometry_poll(void) {
    if (!NSThread.isMainThread) return;
    struct host_geometry geometry;
    double now = CACurrentMediaTime();
    if (host_geometry_debounce_settled(&geometry_debounce, now, &geometry)) {
        pthread_mutex_lock(&geometry_lock);
        settled_geometry = geometry;
        unsigned generation = ++settled_generation;
        pthread_mutex_unlock(&geometry_lock);
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_VISION
        portrait_screen_check();
#endif
        relayout_to_scene();
        relayout_again_at = now + 0.4;    /* the display move and rotation can finish after the first settle */
        host_logf(HOST_LOG_INFO, "scene geometry settled %.0fx%.0f orientation=%d (%s) generation=%u",
            geometry.width, geometry.height, geometry.orientation,
            orientation_name((UIInterfaceOrientation)geometry.orientation), generation);
        return;
    }
    /* Safety net: every frame, make the views, layer and SDL window agree with the scene.
       The check only writes on a mismatch, so a steady frame costs a few comparisons. */
    relayout_to_scene();
    if (relayout_again_at > 0 && now >= relayout_again_at) {
        relayout_again_at = 0;
        relayout_to_scene();
    }
}

/* SDL 3.4.16 owns these controllers. These categories add only orientation
   preferences inherited from UIKit, never replace SDL's own methods. Recheck
   the selectors when upgrading SDL; no runtime class mutation is needed. */
@interface SDL_uikitviewcontroller : UIViewController
@end
@interface SDLLaunchScreenController : UIViewController
@end
@interface SDLUIKitSceneDelegate : NSObject <UIWindowSceneDelegate>
@end

static BOOL landscape_lock(UIViewController *controller) {
    /* Don't lock a transient portrait scene before the geometry request has
       completed. The scene callback updates this preference after rotation. */
    return !host_ios_allows_portrait() && UIInterfaceOrientationIsLandscape(controller.viewIfLoaded.window.windowScene.interfaceOrientation);
}

#define HALO_LANDSCAPE_PREFERENCES \
- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation { return host_ios_presentation_orientation(); } \
- (BOOL)prefersInterfaceOrientationLocked { return landscape_lock(self); }

@implementation HaloLandscapeController
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return host_ios_allows_portrait() ? UIInterfaceOrientationMaskAll : UIInterfaceOrientationMaskLandscape; }
HALO_LANDSCAPE_PREFERENCES
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    host_ios_require_landscape(self.view.window);
}
@end

@implementation HaloLandscapeDocumentPicker
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return host_ios_allows_portrait() ? UIInterfaceOrientationMaskAll : UIInterfaceOrientationMaskLandscape; }
HALO_LANDSCAPE_PREFERENCES
@end

@implementation SDL_uikitviewcontroller (HaloLandscape)
HALO_LANDSCAPE_PREFERENCES
@end

@implementation SDLLaunchScreenController (HaloLandscape)
HALO_LANDSCAPE_PREFERENCES
@end

@implementation SDLUIKitSceneDelegate (HaloLandscape)
/* iPadOS 27 consults the scene delegate in preference to Info.plist. */
- (UIInterfaceOrientationMask)supportedInterfaceOrientationsForWindowScene:(UIWindowScene *)scene {
    if (host_ios_allows_portrait()) return UIInterfaceOrientationMaskAll;
    /* Only the native launch menu supports portrait; gameplay keeps its landscape contract. */
    Class menuClass = NSClassFromString(@"HaloBenchmarkMenuController");
    if (menuClass) for (UIWindow *window in scene.windows)
        if (!window.hidden && [window.rootViewController isKindOfClass:menuClass])
            return UIInterfaceOrientationMaskAllButUpsideDown;
    return UIInterfaceOrientationMaskLandscape;
}
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000
- (void)windowScene:(UIWindowScene *)scene didUpdateEffectiveGeometry:(UIWindowSceneGeometry *)previousGeometry API_AVAILABLE(ios(26.0)) {
    BOOL wasLandscape=UIInterfaceOrientationIsLandscape(previousGeometry.interfaceOrientation);
    BOOL isLandscape=UIInterfaceOrientationIsLandscape(scene.effectiveGeometry.interfaceOrientation);
    if (wasLandscape != isLandscape) {
        for (UIWindow *window in scene.windows) {
            UIViewController *controller=window.rootViewController;
            while (controller.presentedViewController) controller=controller.presentedViewController;
            [controller setNeedsUpdateOfPrefersInterfaceOrientationLocked];
        }
    }
    CGSize size=scene.effectiveGeometry.coordinateSpace.bounds.size;
    host_logf(HOST_LOG_INFO,"scene geometry %.0fx%.0f orientation=%ld (%s) locked=%d",
        size.width,size.height,(long)scene.effectiveGeometry.interfaceOrientation,
        orientation_name(scene.effectiveGeometry.interfaceOrientation),
        scene.effectiveGeometry.isInterfaceOrientationLocked);
    /* External-display scene callbacks must not replace the game's geometry. */
    if ([scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication])
        host_geometry_debounce_note(&geometry_debounce, CACurrentMediaTime(),
            size.width, size.height, (int)scene.effectiveGeometry.interfaceOrientation);
}
#endif
@end

void host_ios_require_landscape(UIWindow *window) {
#if TARGET_OS_VISION || TARGET_OS_MACCATALYST
    /* a visionOS window has no orientation, and its scene rejects iOS's
       geometry preferences; the controllers' landscape preferences above are
       never consulted there. A Mac Catalyst window has none either, and its
       scene logs a warning for the request. */
    (void)window;
    return;
#endif
    if (host_ios_allows_portrait() || !window) return;
    [window.rootViewController setNeedsUpdateOfSupportedInterfaceOrientations];
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000
    if (@available(iOS 26.0,*)) [window.rootViewController setNeedsUpdateOfPrefersInterfaceOrientationLocked];
#endif
    UIWindowSceneGeometryPreferencesIOS *geometry=[[UIWindowSceneGeometryPreferencesIOS alloc]
        initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape];
    [window.windowScene requestGeometryUpdateWithPreferences:geometry errorHandler:^(NSError *error) {
        host_logf(HOST_LOG_WARN,"landscape request: %s",error.localizedDescription.UTF8String);
    }];
}
