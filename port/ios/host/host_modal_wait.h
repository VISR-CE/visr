/* Off-main counterpart of a modal UI's polling loop. The caller holds no lock
   that main can take. The predicate and its captured UI state belong to main;
   the semaphore publishes their final result back to the calling thread. */
#pragma once
#include "host_main_thread.h"
#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <stdbool.h>

static inline void host_modal_wait(double interval, bool (^finished)(void)) {
    assert(!host_on_main_thread());
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block CFRunLoopTimerRef timer = NULL;
    host_main_sync(^{
        if (finished()) {
            dispatch_semaphore_signal(done);
            return;
        }
        timer = CFRunLoopTimerCreateWithHandler(NULL, CFAbsoluteTimeGetCurrent() + interval,
            interval, 0, 0, ^(CFRunLoopTimerRef source) {
                if (finished()) {
                    CFRunLoopTimerInvalidate(source);
                    dispatch_semaphore_signal(done);
                }
            });
        CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopCommonModes);
    });
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    host_main_sync(^{
        if (timer) { CFRunLoopTimerInvalidate(timer); CFRelease(timer); }
    });
#if !OS_OBJECT_USE_OBJC
    dispatch_release(done);
#endif
}
