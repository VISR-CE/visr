/* The native Mac app's Game and View menus. Each item carries a shortcut the
   keyboard shows and runs (host_macos_shortcuts.h keeps the game from also
   receiving the keystrokes) and works from the menu too. */
#import <AppKit/AppKit.h>
#include <SDL3/SDL.h>
#include "ios_host.h"
#include "host_macos.h"
#include "host_macos_shortcuts.h"
#include "host.h"

static NSWindow *game_window_for_menu(void) {
    SDL_Window *window = host_sdl_window();
    return window ? (__bridge NSWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL) : nil;
}

@interface VISRMacMenuActions : NSObject <NSMenuItemValidation>
@end
@implementation VISRMacMenuActions
- (void)toggleFullScreen:(id)sender {
    (void)sender;
    NSWindow *window = game_window_for_menu();
    host_logf(HOST_LOG_INFO, "menu: %s", (window.styleMask & NSWindowStyleMaskFullScreen) ? "Exit Full Screen" : "Enter Full Screen");
    window.collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
    [window toggleFullScreen:nil];
}
- (void)toggleMouse:(id)sender {
    (void)sender;
    SDL_Window *sdl = host_sdl_window();
    host_logf(HOST_LOG_INFO, "menu: %s", sdl && SDL_GetWindowRelativeMouseMode(sdl) ? "Release Mouse" : "Capture Mouse");
    host_sdl_request_key(HOST_MACOS_SHORTCUT_MOUSE);
}
- (void)goBack:(id)sender {
    (void)sender;
    host_logf(HOST_LOG_INFO, "menu: Back");
    host_sdl_request_key(HOST_MACOS_SHORTCUT_BACK);
}
- (BOOL)validateMenuItem:(NSMenuItem *)item {
    NSWindow *window = game_window_for_menu();
    if (item.action == @selector(toggleFullScreen:)) {
        BOOL full = (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
        item.title = full ? @"Exit Full Screen" : @"Enter Full Screen";
        return window != nil;
    }
    if (item.action == @selector(toggleMouse:)) {
        SDL_Window *sdl = host_sdl_window();
        item.title = sdl && SDL_GetWindowRelativeMouseMode(sdl) ? @"Release Mouse" : @"Capture Mouse";
        return sdl != NULL;
    }
    if (item.action == @selector(goBack:)) return host_sdl_window() != NULL;
    return YES;
}
@end

static NSMenuItem *shortcut_item(NSString *title, SEL action, NSString *key, NSEventModifierFlags flags, id target) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];
    item.keyEquivalentModifierMask = flags;
    item.target = target;
    return item;
}

/* the Game and View menus, after the application menu */
void host_macos_menu_add(NSMenu *bar) {
    static VISRMacMenuActions *actions;
    if (!actions) actions = [VISRMacMenuActions new];
    NSEventModifierFlags command = NSEventModifierFlagCommand,
        control_command = NSEventModifierFlagControl | NSEventModifierFlagCommand;
    NSMenu *game = [[NSMenu alloc] initWithTitle:@"Game"];
    [game addItem:shortcut_item(@"Release Mouse", @selector(toggleMouse:), @"m", control_command, actions)];
    [game addItem:shortcut_item(@"Back", @selector(goBack:), @"[", command, actions)];
    NSMenuItem *game_item = [NSMenuItem new];
    game_item.submenu = game;
    [bar addItem:game_item];
    NSMenu *view = [[NSMenu alloc] initWithTitle:@"View"];
    [view addItem:shortcut_item(@"Enter Full Screen", @selector(toggleFullScreen:), @"f", control_command, actions)];
    NSMenuItem *view_item = [NSMenuItem new];
    view_item.submenu = view;
    [bar addItem:view_item];
}
