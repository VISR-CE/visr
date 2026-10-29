/* Apache-2.0. Native entry point for the iOS Discord trial. */
#import <TargetConditionals.h>
#if HALO_DISCORD && TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_TV && !TARGET_OS_VISION
#import <UIKit/UIKit.h>
#include <SDL3/SDL.h>
#include "ios_host.h"
#include "host_discord.h"

void host_discord_ui_install(void) {
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{ host_discord_ui_install(); });
        return;
    }
    static __weak UIButton *settings_button;
    SDL_Window *game = host_sdl_window();
    if (!game) return;
    UIWindow *window = (__bridge UIWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(game),
        SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    UIView *view = window.rootViewController.view;
    if (!view || settings_button.superview == view) return;
    [settings_button removeFromSuperview];
    UIButtonConfiguration *configuration = [UIButtonConfiguration filledButtonConfiguration];
    configuration.title = @"Discord Settings…";
    configuration.baseBackgroundColor = UIColor.secondarySystemBackgroundColor;
    configuration.baseForegroundColor = UIColor.labelColor;
    configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    configuration.contentInsets = NSDirectionalEdgeInsetsMake(10, 16, 10, 16);
    UIButton *button = [UIButton buttonWithConfiguration:configuration primaryAction:
        [UIAction actionWithTitle:@"Discord Settings…" image:nil identifier:nil handler:^(UIAction *action) {
            (void)action;
            host_discord_show_settings();
        }]];
    button.accessibilityIdentifier = @"discord-settings";
    button.accessibilityLabel = @"Discord Settings";
    button.accessibilityHint = @"Link or unlink your Discord account and review sharing status.";
    button.titleLabel.adjustsFontForContentSizeCategory = YES;
    button.translatesAutoresizingMaskIntoConstraints = NO;
    [view addSubview:button];
    [NSLayoutConstraint activateConstraints:@[
        [button.topAnchor constraintEqualToAnchor:view.safeAreaLayoutGuide.topAnchor constant:12],
        [button.trailingAnchor constraintEqualToAnchor:view.safeAreaLayoutGuide.trailingAnchor constant:-12],
        [button.leadingAnchor constraintGreaterThanOrEqualToAnchor:view.safeAreaLayoutGuide.leadingAnchor constant:12],
        [button.heightAnchor constraintGreaterThanOrEqualToConstant:44]
    ]];
    settings_button = button;
}
#else
void host_discord_ui_install(void) {}
#endif
