/* Exercise startup and shutdown without SDL, a window, or game data. */
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "host_main_thread.h"
#include "host_quit_signals.h"

#ifndef HALO_NATIVE_MAC
#define HALO_NATIVE_MAC 1
#endif
/* Compile the UIKit control flow against Foundation on the Mac. */
#undef TARGET_OS_IOS
#undef TARGET_OS_MACCATALYST
#undef TARGET_OS_VISION
#define TARGET_OS_IOS (!HALO_NATIVE_MAC)
#define TARGET_OS_MACCATALYST 0
#define TARGET_OS_VISION 0
#define TARGET_OS_TV 0
#define HOST_LOG_INFO 0
static size_t main_stack_size;
static void *main_alternate;
static _Atomic int pumps, quit_events;
static int quit_mode;
#if !HALO_NATIVE_MAC
static int startup_returned;
#endif
#include "host_startup.h"
static struct host_startup_info startup;
const struct host_startup_info *host_startup_get(void) { return &startup; }
static CFRunLoopTimerRef probe_timer(CFAllocatorRef allocator, CFAbsoluteTime fire,
        CFTimeInterval interval, CFOptionFlags flags, CFIndex order,
        void (^handler)(CFRunLoopTimerRef)) {
    assert(host_on_main_thread());
    double rate = HALO_NATIVE_MAC ? 240.0 : (quit_mode == 0 ? 60.0 : 120.0);
    assert(interval == 1.0 / rate);
    return CFRunLoopTimerCreateWithHandler(allocator, fire, interval, flags, order, handler);
}
#define CFRunLoopTimerCreateWithHandler probe_timer
#define SDL_EVENT_QUIT 1
typedef struct { int type; } SDL_Event;
static int SDL_PushEvent(SDL_Event *event) {
    assert(host_on_main_thread() && event->type == SDL_EVENT_QUIT);
    atomic_fetch_add(&quit_events, 1);
    return 1;
}
static int host_game_thread_enabled(void) { return 1; }
static void host_logf(int priority, const char *format, ...) { (void)priority; (void)format; }
static void host_spatial_rumble_hold(int held) { assert(host_on_main_thread() && held); }
static void host_cloud_saves_leaving(int seconds) { assert(host_on_main_thread() && seconds == 3); }
static void host_shader_upload_suspend(void) { assert(host_on_main_thread()); }
static void host_perf_reports_finish(int failed) { assert(host_on_main_thread() && failed == (quit_mode != 2)); }
static void host_crash(const char *message) { (void)message; abort(); }
static void host_exit(int code);
static void host_event_pump(void) { assert(host_on_main_thread()); atomic_fetch_add(&pumps, 1); }
static uint32_t host_guest_on_stack(uintptr_t start, uint32_t boot, uintptr_t arena, void *stack) {
    assert(!host_on_main_thread());
    assert(start == 123 && boot == 456 && arena == 789 && stack == (void *)1024);
    assert(pthread_get_stacksize_np(pthread_self()) >= main_stack_size);
    assert(qos_class_self() == QOS_CLASS_USER_INTERACTIVE);
    char name[32]; assert(!pthread_getname_np(pthread_self(), name, sizeof(name)));
    assert(!strcmp(name, "game"));
    stack_t alternate; assert(!sigaltstack(NULL, &alternate));
    assert(!(alternate.ss_flags & SS_DISABLE) && alternate.ss_size >= 64 * 1024);
    assert(alternate.ss_sp != main_alternate);
    host_main_sync(^{
        assert(host_on_main_thread());
#if !HALO_NATIVE_MAC
        assert(startup_returned);
#endif
    });
    while (!atomic_load(&pumps)) usleep(1000);
    if (quit_mode) {
        assert(!kill(getpid(), SIGTERM));
        while (!atomic_load(&quit_events)) usleep(1000);
        if (quit_mode == 2) for (;;) pause();
    }
    host_exit(23);
    abort();
}
#include "game_thread_start.inc"

int main(void) {
    assert(host_game_thread_enabled());
    main_stack_size = pthread_get_stacksize_np(pthread_self());
    char alternate[64 * 1024];
    stack_t stack = { .ss_sp = alternate, .ss_size = sizeof(alternate) };
    assert(!sigaltstack(&stack, NULL)); main_alternate = alternate;
    for (quit_mode = 0; quit_mode < 3; quit_mode++) {
        pid_t child = fork(); assert(child >= 0);
        if (!child) {
            alarm(5);
            assert(host_quit_signals_install(dispatch_get_main_queue(), quit_signal));
            startup.maximum_frames_per_second = quit_mode == 0 ? 60 : (quit_mode == 1 ? 120 : 240);
            game_thread_run(123, 456, 789, (void *)1024);
#if HALO_NATIVE_MAC
            abort();
#else
            /* Simulate returning from SDL's launch callback into UIKit's loop. */
            startup_returned = 1;
            for (;;) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0e10, false);
#endif
        }
        int status; assert(waitpid(child, &status, 0) == child);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == (quit_mode == 2 ? 0 : 23));
    }
    puts(HALO_NATIVE_MAC ? "native game thread startup and main-thread exit passed" :
         "iOS game thread startup return and main-thread exit passed");
    return 0;
}
