/* Import only explicitly selected disc data into this app's own local folder. */
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "ios_host.h"
#include "xiso.h"
#include "host_astc_store.h"
#include "host_texture_upscale.h"
#include "texture_upscale_schedule.h"
#include <stdatomic.h>

@interface VISRMacImport : NSObject {
@public
    atomic_int canceled;
    atomic_int completed;
}
- (void)cancel:(id)sender;
@end
@implementation VISRMacImport
- (void)cancel:(id)sender { (void)sender; atomic_store(&canceled, 1); }
@end
static int progress(void *context, const char *name, uint64_t done, uint64_t total) {
    (void)name; (void)done; (void)total;
    VISRMacImport *operation = (__bridge VISRMacImport *)context;
    return !atomic_load(&operation->canceled);
}
/* ---------- the upscaling phase (host_texture_upscale_prepare_import), as on iPhone and iPad: one bar over the setup
   and the whole set, gray until the main menu's textures are made, then blue with Start playing; "ready to start" marks
   where that happens. The work goes on behind the menus once the player starts. */
static NSColor *bar_blue(void) { return [NSColor colorWithSRGBRed:0.247 green:0.839 blue:1 alpha:1]; }
@interface VISRUpscaleBar : NSView
@property(nonatomic) double fraction, tick;
@property(nonatomic) BOOL ready;   /* Start playing is offered: the fill turns blue */
@end
@implementation VISRUpscaleBar
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    CGFloat width = self.bounds.size.width, top = 8, height = 6;
    NSRect track = NSMakeRect(0, top, width, height);
    [[NSColor colorWithWhite:1 alpha:0.18] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:track xRadius:height / 2 yRadius:height / 2] fill];
    NSRect fill = track; fill.size.width = round(width * MIN(MAX(self.fraction, 0), 1));
    if (fill.size.width > 0) {
        [(self.ready ? bar_blue() : NSColor.systemGrayColor) setFill];
        [[NSBezierPath bezierPathWithRoundedRect:fill xRadius:height / 2 yRadius:height / 2] fill];
    }
    if (self.tick < 0) return;
    CGFloat x = MIN(MAX(round(width * MIN(self.tick, 1) - 1), 0), width - 2);
    [bar_blue() setFill];
    NSRectFill(NSMakeRect(x, top - 5, 2, height + 10));
    NSDictionary *style = @{NSFontAttributeName: [NSFont systemFontOfSize:NSFont.smallSystemFontSize],
        NSForegroundColorAttributeName: bar_blue()};
    NSString *label = @"ready to start";
    NSSize size = [label sizeWithAttributes:style];
    CGFloat left = MIN(MAX(x + 1 - size.width / 2, 0), MAX(width - size.width, 0));
    [label drawAtPoint:NSMakePoint(left, top + height + 8) withAttributes:style];
}
@end
/* The phase's buttons act on the first click, even when the window is not yet key (the click that brings VISR forward
   would otherwise only activate it) */
@interface VISRFirstClickButton : NSButton
@end
@implementation VISRFirstClickButton
- (BOOL)acceptsFirstMouse:(NSEvent *)event { (void)event; return YES; }
@end
@interface VISRUpscalePhase : NSObject {
@public
    atomic_int skip, done, menu_phase, retry_requested, can_retry, retry_busy;
    _Atomic uint64_t found_bytes, needed_bytes;
    const char *_Atomic retry_note;
    _Atomic double fraction, tick;
    const char *_Atomic message;   /* the phase ended refused (a string literal), or NULL */
    const char *_Atomic detail;    /* its second, smaller line */
}
- (void)skip:(id)sender;
- (void)retry:(id)sender;
@end
@implementation VISRUpscalePhase
- (void)skip:(id)sender { (void)sender; atomic_store(&skip, 1); }
- (void)retry:(id)sender {
    NSButton *button=sender;
    if(!button.enabled)return;
    button.enabled=NO;atomic_store(&retry_busy,1);atomic_store(&retry_requested,1);
}
@end
static int upscale_progress(void *context, const struct host_texture_upscale_import_status *status) {
    VISRUpscalePhase *phase = (__bridge VISRUpscalePhase *)context;
    atomic_store(&phase->fraction, status->fraction); atomic_store(&phase->tick, status->tick);
    atomic_store(&phase->menu_phase, status->menu_phase);
    atomic_store(&phase->message, status->message); atomic_store(&phase->detail, status->detail);
    atomic_store(&phase->retry_note,status->retry_note);
    atomic_store(&phase->found_bytes,status->found_bytes);atomic_store(&phase->needed_bytes,status->needed_bytes);
    atomic_store(&phase->can_retry,status->can_retry);
    if(!atomic_load(&phase->retry_requested))atomic_store(&phase->retry_busy,!status->can_retry);
    /* The main menu's textures come first: a skip counts once Start playing is offered. */
    if(!status->menu_phase&&atomic_load(&phase->skip))return HOST_TEXTURE_UPSCALE_START_PLAYING;
    if(atomic_exchange(&phase->retry_requested,0)){atomic_store(&phase->retry_busy,1);return HOST_TEXTURE_UPSCALE_TRY_AGAIN;}
    return HOST_TEXTURE_UPSCALE_WAIT;
}
static void upscale_phase(NSString *root) {
    VISRUpscalePhase *phase = [VISRUpscalePhase new];
    atomic_store(&phase->tick, -1.0); atomic_store(&phase->menu_phase, 1);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 680, 170)
        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    window.title = @"VISR"; window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    window.backgroundColor = [NSColor colorWithSRGBRed:0.025 green:0.045 blue:0.055 alpha:1];
    NSView *content = window.contentView;
    NSTextField *status = [NSTextField labelWithString:@"Upscaling textures"];
    status.frame = NSMakeRect(24, 116, 632, 20); status.alignment = NSTextAlignmentCenter;
    status.textColor = NSColor.secondaryLabelColor; [content addSubview:status];
    /* (in the bar's place when the work is refused: the line under the message) */
    NSTextField *detail = [NSTextField labelWithString:@""];
    detail.frame = NSMakeRect(24, 66, 632, 44);
    detail.maximumNumberOfLines=2;detail.lineBreakMode=NSLineBreakByClipping; detail.alignment = NSTextAlignmentCenter;
    detail.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize]; detail.textColor = NSColor.secondaryLabelColor;
    detail.hidden = YES; [content addSubview:detail];
    VISRUpscaleBar *bar = [[VISRUpscaleBar alloc] initWithFrame:NSMakeRect(24, 64, 632, 46)];
    bar.tick = -1; [content addSubview:bar];
    NSButton *start = [VISRFirstClickButton buttonWithTitle:@"Start playing" target:phase action:@selector(skip:)];
    start.frame = NSMakeRect(275, 18, 130, 32);start.bezelStyle=NSBezelStyleRounded;
    start.bezelColor=bar_blue();start.contentTintColor=NSColor.blackColor;
    /* (black on the bar's light blue, as on iPhone and iPad: white on it measures about 1.7:1, black about 12:1;
       contentTintColor does not reach a rounded button's title, the attributed title does) */
    start.attributedTitle=[[NSAttributedString alloc] initWithString:@"Start playing" attributes:@{
        NSForegroundColorAttributeName: NSColor.blackColor, NSFontAttributeName: [NSFont systemFontOfSize:NSFont.systemFontSize]}];
    start.alphaValue=0;start.enabled=NO;start.keyEquivalent=@"\r";[content addSubview:start];
    NSButton *retry=[VISRFirstClickButton buttonWithTitle:@"Try Again" target:phase action:@selector(retry:)];
    retry.frame=NSMakeRect(209,18,120,32);retry.bezelStyle=NSBezelStyleRounded;
    retry.alphaValue=0;retry.enabled=NO;[content addSubview:retry];
    [window center];
    /* (the work is the player's own request: keep App Nap from slowing it while the window is behind another app) */
    id activity = [NSProcessInfo.processInfo beginActivityWithOptions:NSActivityUserInitiatedAllowingIdleSystemSleep
        reason:@"Upscaling textures"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        host_texture_upscale_prepare_import(root.fileSystemRepresentation, upscale_progress, (__bridge void *)phase);
        atomic_store(&phase->done, 1);
    });
    /* (frontmost and key, so the first click and Return reach it) */
    [NSApp activateIgnoringOtherApps:YES]; [window makeKeyAndOrderFront:nil];
    NSModalSession session = [NSApp beginModalSessionForWindow:window];
    while (!atomic_load(&phase->done)) {
        [NSApp runModalSession:session];
        BOOL menu = atomic_load(&phase->menu_phase);
        double shown = atomic_load(&phase->fraction);
        bar.fraction = shown; bar.tick = atomic_load(&phase->tick); bar.ready = !menu; bar.needsDisplay = YES;
        /* (rounded down: 100% only when every texture is made) */
        const char *message = atomic_load(&phase->message);
        status.stringValue = message ? @(message) : [NSString stringWithFormat:@"Upscaling textures · %d%%", (int)floor(shown * 100)];
        status.textColor = message ? NSColor.labelColor : NSColor.secondaryLabelColor;
        const char *more = atomic_load(&phase->detail);
        NSString *detailText=more?@(more):@"";
        uint64_t needed=atomic_load(&phase->needed_bytes),found=atomic_load(&phase->found_bytes);
        const char *note=atomic_load(&phase->retry_note);if(note)detailText=@(note);
        if(needed){
            NSString *freeText=[NSByteCountFormatter stringFromByteCount:(int64_t)found countStyle:NSByteCountFormatterCountStyleFile];
            NSString *needText=[NSByteCountFormatter stringFromByteCount:(int64_t)needed countStyle:NSByteCountFormatterCountStyleFile];
            char line[512];
            texture_upscale_import_storage_line(line,sizeof(line),freeText.UTF8String,needText.UTF8String,note!=NULL);
            detailText=[NSString stringWithFormat:@"%s\n%s",line,TEXTURE_UPSCALE_IMPORT_NO_STORAGE_DETAIL];
        }
        bar.alphaValue=message?0:1;detail.alphaValue=message?1:0;detail.hidden=NO;detail.stringValue=detailText;
        start.frame=NSMakeRect(message?341:275,18,130,32);
        start.alphaValue=menu?0:1;start.enabled=!menu;
        retry.alphaValue=message?1:0;retry.enabled=message&&atomic_load(&phase->can_retry)&&!atomic_load(&phase->retry_busy);
        [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    }
    [NSApp endModalSession:session];
    [NSProcessInfo.processInfo endActivity:activity];
    [window orderOut:nil];
}
/* Publishing is separate from extraction. A failed move must retain the previous maps. */
static BOOL publish(NSString *staging, NSString *root, NSError **error) {
    NSFileManager *files = NSFileManager.defaultManager;
    NSString *maps = [root stringByAppendingPathComponent:@"maps"];
    NSString *incoming = [staging stringByAppendingPathComponent:@"maps"];
    NSString *backup = [staging stringByAppendingPathComponent:@"previous-maps"];
    if (!xiso_maps_ready(incoming.fileSystemRepresentation, NULL, 0)) return NO;
    BOOL existing = [files fileExistsAtPath:maps];
    if (existing && ![files moveItemAtPath:maps toPath:backup error:error]) return NO;
    if (![files moveItemAtPath:incoming toPath:maps error:error]) {
        if (existing && ![files moveItemAtPath:backup toPath:maps error:nil])
            host_logf(HOST_LOG_ERROR, "Import rollback failed; original maps preserved in %s", backup.fileSystemRepresentation);
        return NO;
    }
    NSString *movies = [root stringByAppendingPathComponent:@"bink"];
    NSString *incoming_movies = [staging stringByAppendingPathComponent:@"bink"];
    if (![files fileExistsAtPath:movies] && [files fileExistsAtPath:incoming_movies])
        [files moveItemAtPath:incoming_movies toPath:movies error:nil];
    host_astc_store_prepare_import(root.fileSystemRepresentation,NULL,NULL);
    return YES;
}
static void recover(NSString *root) {
    NSFileManager *files = NSFileManager.defaultManager;
    for (NSString *name in [files contentsOfDirectoryAtPath:root error:nil]) {
        if (![name hasPrefix:@".visr-import-"] || ![[NSUUID alloc] initWithUUIDString:[name substringFromIndex:13]]) continue;
        NSString *staging = [root stringByAppendingPathComponent:name];
        NSString *marker = [staging stringByAppendingPathComponent:@"owner.txt"];
        if (![[NSString stringWithContentsOfFile:marker encoding:NSUTF8StringEncoding error:nil] isEqualToString:@"VISR native import v1"]) continue;
        NSString *backup = [staging stringByAppendingPathComponent:@"previous-maps"];
        NSString *maps = [root stringByAppendingPathComponent:@"maps"];
        if ([files fileExistsAtPath:backup]) {
            if (![files fileExistsAtPath:maps]) [files moveItemAtPath:backup toPath:maps error:nil];
            /* Preserve an old set whenever recovery cannot establish a valid current one. */
            if ([files fileExistsAtPath:backup] && !xiso_maps_ready(maps.fileSystemRepresentation, NULL, 0)) continue;
        }
        [files removeItemAtPath:staging error:nil];
    }
}
/* A security-scoped, read-only bookmark to the chosen disc image, kept in the data folder. A later import (the maps
   missing or incomplete) reopens the same image without asking again; the sandboxed app may read it only through
   this bookmark. */
static NSString *bookmark_path(NSString *root) { return [root stringByAppendingPathComponent:@"disc-image.bookmark"]; }
static void remember(NSURL *url, NSString *root) {
    NSError *error = nil;
    NSData *data = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope |
        NSURLBookmarkCreationSecurityScopeAllowOnlyReadAccess includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
    if (!data || ![data writeToFile:bookmark_path(root) options:NSDataWritingAtomic error:&error])
        host_logf(HOST_LOG_WARN, "Could not remember the disc image: %s", error.localizedDescription.UTF8String ?: "unknown error");
}
static NSURL *remembered(NSString *root, BOOL *stale) {
    NSData *data = [NSData dataWithContentsOfFile:bookmark_path(root)];
    if (!data) return nil;
    NSError *error = nil;
    NSURL *url = [NSURL URLByResolvingBookmarkData:data options:NSURLBookmarkResolutionWithSecurityScope |
        NSURLBookmarkResolutionWithoutUI relativeToURL:nil bookmarkDataIsStale:stale error:&error];
    if (!url) {
        host_logf(HOST_LOG_WARN, "The remembered disc image is gone: %s", error.localizedDescription.UTF8String ?: "unknown error");
        [NSFileManager.defaultManager removeItemAtPath:bookmark_path(root) error:nil];
    }
    return url;
}
static NSURL *choose(void) {
    NSOpenPanel *picker = [NSOpenPanel openPanel];
    picker.title = @"Choose your Halo disc image";
    picker.message = @"Select an original Xbox PAL or NTSC-US ISO or XISO. No game files are included.";
    picker.canChooseDirectories = NO; picker.allowsMultipleSelection = NO;
    /* XISO providers may report a generic data type. The extractor validates content. */
    picker.allowedContentTypes = @[UTTypeData];
    if ([picker runModal] != NSModalResponseOK) host_exit(0);
    return picker.URL;
}
/* Extract, validate and publish one disc image. Returns NO with a message for the player on failure. */
static BOOL import_disc(NSURL *url, NSString *root, BOOL save_bookmark, NSString **failure) {
    NSFileManager *files = NSFileManager.defaultManager;
    BOOL scoped = [url startAccessingSecurityScopedResource];
    NSString *staging = [root stringByAppendingPathComponent:[@".visr-import-" stringByAppendingString:NSUUID.UUID.UUIDString]];
    NSError *error = nil;
    if (![files createDirectoryAtPath:staging withIntermediateDirectories:NO attributes:nil error:&error] ||
        ![@"VISR native import v1" writeToFile:[staging stringByAppendingPathComponent:@"owner.txt"] atomically:YES encoding:NSUTF8StringEncoding error:&error]) {
        if (scoped) [url stopAccessingSecurityScopedResource];
        *failure = error.localizedDescription ?: @"Could not start the import.";
        return NO;
    }
    NSAlert *dialog = [NSAlert new]; dialog.messageText = @"Importing Halo";
    dialog.informativeText = @"Your original disc image stays unchanged.";
    [dialog addButtonWithTitle:@"Cancel"];
    NSProgressIndicator *spinner = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, 240, 20)];
    spinner.indeterminate = YES; spinner.style = NSProgressIndicatorStyleBar;
    spinner.accessibilityLabel = @"Import progress";
    dialog.accessoryView = spinner; [spinner startAnimation:nil];
    VISRMacImport *operation = [VISRMacImport new];
    NSButton *cancel = dialog.buttons.firstObject; cancel.target = operation; cancel.action = @selector(cancel:);
    __block BOOL modal_running = YES;
    __block BOOL imported = NO;
    __block NSString *message_text;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @autoreleasepool {
            char message[1024] = {0};
            imported = xiso_extract_maps(url.fileSystemRepresentation, staging.fileSystemRepresentation,
                progress, (__bridge void *)operation, message, sizeof(message));
            if (!imported) message_text = @(message);
            if (atomic_load(&operation->canceled)) { imported = NO; message_text = @"Import canceled."; }
            atomic_store(&operation->completed, 1);
            dispatch_async(dispatch_get_main_queue(), ^{ if (modal_running) [NSApp stopModal]; });
        }
    });
    [dialog runModal];
    modal_running = NO;
    if (!atomic_load(&operation->completed)) atomic_store(&operation->canceled, 1);
    /* A keyboard dismissal must not release security scope or staging while the worker still reads them. */
    while (!atomic_load(&operation->completed))
        [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    [dialog.window orderOut:nil];
    /* The bookmark is made while the panel's (or the old bookmark's) access is still open. */
    if (imported && save_bookmark) remember(url, root);
    if (scoped) [url stopAccessingSecurityScopedResource];
    if (imported && !publish(staging, root, &error)) {
        imported = NO; message_text = error.localizedDescription ?: @"Could not finish the import. Existing data was preserved.";
    }
    /* A failed rollback leaves its unique backup intact for the next startup. */
    if (imported || ![files fileExistsAtPath:[staging stringByAppendingPathComponent:@"previous-maps"]])
        [files removeItemAtPath:staging error:nil];
    if (!imported) *failure = message_text ?: @"The import did not complete.";
    return imported;
}
void host_ios_prepare_assets(const char *directory) {
    NSString *root = @(directory);
    recover(root);
    NSString *maps = [root stringByAppendingPathComponent:@"maps"];
    if (xiso_maps_ready(maps.fileSystemRepresentation, NULL, 0)) {
        /* a game imported before the upscaling bar existed shows it once, where upscaling is on */
        BOOL forced = NO;
#ifdef HALO_DEVELOPMENT_HOOKS
        forced = getenv("HALO_TEXTURE_UPSCALE_IMPORT_PHASE") != NULL;   /* (development builds only) */
#endif
        if (forced || host_texture_upscale_first_run_phase(directory)) {
            host_texture_upscale_init(directory);
            if (host_texture_upscale_enabled()) upscale_phase(root);
        }
        return;
    }
    BOOL stale = NO;
    NSURL *url = remembered(root, &stale);
    while (1) {
        BOOL automatic = url != nil;
        if (!url) url = choose();
        NSString *failure = nil;
        if (import_disc(url, root, !automatic || stale, &failure)) {
            host_logf(HOST_LOG_INFO, "XISO import complete; original map bytes preserved");
            host_texture_upscale_init(directory);
            if (host_texture_upscale_enabled()) upscale_phase(root);
            return;
        }
        url = nil;
        /* The remembered image failed (moved, changed or canceled): ask for one instead. */
        if (automatic) {
            host_logf(HOST_LOG_WARN, "Import from the remembered disc image failed: %s", failure.UTF8String);
            continue;
        }
        if (host_alert("Import did not complete", failure.UTF8String, "Try Again", "Quit") != 0) host_exit(0);
    }
}
