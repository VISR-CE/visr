#import "host_orientation.h"
#include "ios_host.h"
#include "host_config.h"
#include "host_geometry.h"
#include "host_geometry_debounce.h"
#import <QuartzCore/QuartzCore.h>
#include <pthread.h>
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

/* UIKit may ask before host_main.m installs the profile. Only configure
   after that point; early queries keep the existing landscape policy. */
static BOOL allow_portrait;
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

int host_ios_portrait_allowed(void) { return allow_portrait ? 1 : 0; }

BOOL host_ios_allows_portrait(void) {
    return allow_portrait;
}

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_VISION
static char debug_orientation[16];
static BOOL orientation_requested;

static void request_debug_orientation(UIWindowScene *scene) {
    if (orientation_requested || !debug_orientation[0] ||
        scene.activationState != UISceneActivationStateForegroundActive ||
        ![scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication]) return;
    orientation_requested = YES;
    BOOL portrait = !strcmp(debug_orientation, "portrait");
    if (portrait && !allow_portrait) {
        host_logf(HOST_LOG_INFO, "orientation request: portrait requires display.allow_portrait");
        return;
    }
    UIInterfaceOrientationMask mask = portrait ? UIInterfaceOrientationMaskPortrait : UIInterfaceOrientationMaskLandscape;
    host_logf(HOST_LOG_INFO, "orientation request: %s", debug_orientation);
    UIWindowSceneGeometryPreferencesIOS *geometry = [[UIWindowSceneGeometryPreferencesIOS alloc]
        initWithInterfaceOrientations:mask];
    [scene requestGeometryUpdateWithPreferences:geometry errorHandler:^(NSError *error) {
        host_logf(HOST_LOG_WARN, "orientation request: %s", error.localizedDescription.UTF8String);
    }];
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
    if (debug_orientation[0] && strcmp(debug_orientation, "portrait") && strcmp(debug_orientation, "landscape")) {
        host_logf(HOST_LOG_WARN, "orientation request: unknown orientation %s", debug_orientation);
        debug_orientation[0] = 0;
    }
    if (allow_portrait)
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait PortraitUpsideDown LandscapeLeft LandscapeRight");
    dispatch_async(dispatch_get_main_queue(), ^{
        for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
            if (![scene isKindOfClass:UIWindowScene.class]) continue;
            UIWindowScene *windowScene = (UIWindowScene *)scene;
            for (UIWindow *window in windowScene.windows) {
                UIViewController *controller = window.rootViewController;
                while (controller) {
                    [controller setNeedsUpdateOfSupportedInterfaceOrientations];
                    if (@available(iOS 26.0, *)) [controller setNeedsUpdateOfPrefersInterfaceOrientationLocked];
                    controller = controller.presentedViewController;
                }
            }
            request_debug_orientation(windowScene);
        }
        [NSNotificationCenter.defaultCenter addObserverForName:UISceneDidActivateNotification object:nil
            queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *notification) {
                if ([notification.object isKindOfClass:UIWindowScene.class])
                    request_debug_orientation(notification.object);
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
            view.frame = window.bounds;
            for (UIView *sub in view.subviews) if ([NSStringFromClass(sub.class) isEqualToString:@"HaloControls"]) { sub.frame = view.bounds; [sub setNeedsLayout]; }
            [view setNeedsLayout];
            [view layoutIfNeeded];
            CGFloat scale = windowScene.screen.nativeScale > 0 ? windowScene.screen.nativeScale : windowScene.screen.scale;
            if (layer.contentsScale != scale) layer.contentsScale = scale;
            CGSize want = CGSizeMake(round(bounds.size.width * scale), round(bounds.size.height * scale));
            if (!CGSizeEqualToSize(layer.drawableSize, want)) {
                host_logf(HOST_LOG_INFO, "relayout: drawable %.0fx%.0f -> %.0fx%.0f",
                    layer.drawableSize.width, layer.drawableSize.height, want.width, want.height);
                layer.drawableSize = want;
            }
            int width = 0, height = 0;
            SDL_GetWindowSize(sdl, &width, &height);
            if (width != (int)bounds.size.width || height != (int)bounds.size.height) {
                host_logf(HOST_LOG_INFO, "relayout: SDL window %dx%d -> %.0fx%.0f", width, height,
                    bounds.size.width, bounds.size.height);
                SDL_SetWindowSize(sdl, (int)bounds.size.width, (int)bounds.size.height);
            }
            return;
        }
    }
#endif
}

void host_geometry_poll(void) {
    if (!NSThread.isMainThread) return;
    struct host_geometry geometry;
    if (!host_geometry_debounce_settled(&geometry_debounce, CACurrentMediaTime(), &geometry)) return;
    pthread_mutex_lock(&geometry_lock);
    settled_geometry = geometry;
    unsigned generation = ++settled_generation;
    pthread_mutex_unlock(&geometry_lock);
    relayout_to_scene();
    host_logf(HOST_LOG_INFO, "scene geometry settled %.0fx%.0f orientation=%d (%s) generation=%u",
        geometry.width, geometry.height, geometry.orientation,
        orientation_name((UIInterfaceOrientation)geometry.orientation), generation);
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
