/* UIKit and the frame workgroup (gpu_metal.m's display.frame_deadline), as a small Mac Catalyst app: the main thread
   draws the game and is in the "VISR frame" workgroup, and UIKit raises "Failed to create work interval" when one of
   its animations begins on such a thread. tvOS's text dialog (host_tv.m) goes up while the game keeps drawing, and an
   Apple TV crashed as the alert's collection view animated its content offset in the game's event pump. Here frames
   go on around a nested run loop while a text dialog is up and a scroll view animates (on a Mac the alert itself
   animates no scroll view, so the scroll view stands in for the tvOS alert's).
   argv[1] "fixed": the dialog holds the thread out of the workgroup (frame_deadline.h's rules, as host_tv.m holds it)
   until it has gone, then frames join again; exits 0, or 1 when a rule is broken.
   argv[1] "unfixed": the thread stays joined, as before the fix; UIKit's exception ends the app (the control). */
#import <UIKit/UIKit.h>
#import <AudioToolbox/AudioToolbox.h>
#include <mach/mach_time.h>
#include <os/workgroup.h>
#include <stdio.h>
#include <string.h>
#include "frame_deadline.h"

static int fixed, failures;
static os_workgroup_interval_t interval;
static struct os_workgroup_join_token_opaque_s token;
static int state = FRAME_DEADLINE_NEW, held;

static void check(int condition, const char *message) {
    if (!condition) { failures++; fprintf(stderr, "FAIL: %s\n", message); }
}

/* gpu_metal.m's frame_deadline_begin: a shown frame joins again after a leave unless held */
static void frame(void) {
    if (frame_deadline_rejoin_wanted(state, 0, held)) {
        check(os_workgroup_join(interval, &token) == 0, "a frame joins the workgroup again");
        state = FRAME_DEADLINE_JOINED;
    }
    if (state == FRAME_DEADLINE_JOINED) {
        uint64_t now = mach_absolute_time();
        (void)os_workgroup_interval_start(interval, now, now + 16000000, NULL);
        (void)os_workgroup_interval_finish(interval, NULL);
    }
}

/* gpu_metal_frame_deadline_hold */
static void hold(int on) {
    held = on;
    if (on && frame_deadline_leave_wanted(state, 1)) {
        os_workgroup_leave(interval, &token);
        state = FRAME_DEADLINE_LEFT;
    }
}

@interface ProbeDelegate : UIResponder <UIWindowSceneDelegate>
@property (nonatomic, strong) UIWindow *window;
@end

@implementation ProbeDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options {
    (void)session; (void)options;
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    self.window.rootViewController = [UIViewController new];
    [self.window makeKeyAndVisible];
    /* (from a timer, as the game's loop runs: a block on the main queue would keep UIKit's own main-queue work, the
       dialog's transitions among it, from running in the nested loop) */
    [self performSelector:@selector(play) withObject:nil afterDelay:0];
}

- (void)play {
    {
        UIViewController *root = self.window.rootViewController;
        UIScrollView *scroll = [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)];
        scroll.contentSize = CGSizeMake(200, 2000);
        [root.view addSubview:scroll];
        interval = AudioWorkIntervalCreate("VISR frame probe", OS_CLOCK_MACH_ABSOLUTE_TIME, NULL);
        if (!interval || os_workgroup_join(interval, &token)) {
            fprintf(stderr, "SKIP: no workgroup to join here\n");
            exit(0);
        }
        state = FRAME_DEADLINE_JOINED;
        UIAlertController *alert = nil;
        __block BOOL dismissed = NO, gone = NO;
        int n, gone_at = -1;
        /* (on until a few frames after the dialog has gone, so they join again) */
        for (n = 0; n < 400 && !(gone_at >= 0 && n > gone_at + 10); n++) {
            @autoreleasepool {
                frame();
                if (n == 5) {
                    /* host_tv_text_prompt: the hold, then the dialog */
                    if (fixed) hold(1);
                    alert = [UIAlertController alertControllerWithTitle:@"Text" message:nil
                        preferredStyle:UIAlertControllerStyleAlert];
                    [alert addTextFieldWithConfigurationHandler:^(UITextField *field) { field.text = @"a8j5 g7fv"; }];
                    [alert addAction:[UIAlertAction actionWithTitle:@"Done" style:UIAlertActionStyleDefault handler:nil]];
                    [root presentViewController:alert animated:YES completion:nil];
                    [scroll setContentOffset:CGPointMake(0, 800) animated:YES];
                }
                if (n == 60) {
                    [alert dismissViewControllerAnimated:YES completion:^{ gone = YES; }];
                    [scroll setContentOffset:CGPointMake(0, 0) animated:YES];
                    dismissed = YES;
                }
                if (fixed && n > 5 && !dismissed)
                    check(state != FRAME_DEADLINE_JOINED, "no frame joins while the dialog is up");
                /* host_tv.m's text_input_blocked: the dialog has gone, the hold ends */
                if (gone && gone_at < 0) {
                    gone_at = n;
                    if (held) hold(0);
                }
                CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.016, true);
            }
        }
        if (!gone)
            fprintf(stderr, "note: the dialog had not gone after %d frames (presenting %d, being dismissed %d)\n", n,
                alert.presentingViewController != nil, alert.beingDismissed);
        if (fixed) {
            check(!held, "the hold ends once the dialog has gone");
            check(state == FRAME_DEADLINE_JOINED, "frames join the workgroup again after the dialog");
        }
        if (state == FRAME_DEADLINE_JOINED) os_workgroup_leave(interval, &token);
        printf("frame workgroup probe (%s): %s\n", fixed ? "fixed" : "unfixed", failures ? "FAILED" : "no exception");
        exit(failures ? 1 : 0);
    }
}
@end

int main(int argc, char **argv) {
    fixed = argc > 1 && !strcmp(argv[1], "fixed");
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, nil); }
}
