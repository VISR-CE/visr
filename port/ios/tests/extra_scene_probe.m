/* Exercise scene recovery with real dispatch and filesystem operations, without UIKit. */
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "host_main_thread.h"

#define HOST_LOG_INFO 0
#define HOST_LOG_WARN 1
#define SDL_WINDOW_MINIMIZED 1
typedef struct { int flags; } SDL_Window;
static SDL_Window window;
static int threaded, window_exists, pump_enables, close_calls, foreground_calls;
static int inject_during_recovery, inject_during_close, recovery_logs;
static _Atomic int done;
static char data_root[1024], other_root[1024];
static int scene_start(void);
static int host_game_thread_enabled(void) { return threaded; }
static SDL_Window *host_sdl_window(void) { assert(host_on_main_thread()); return window_exists ? &window : NULL; }
static int SDL_GetWindowFlags(SDL_Window *value) { assert(host_on_main_thread()); return value->flags; }
static void SDL_SetiOSEventPump(bool enabled) { assert(host_on_main_thread() && enabled); pump_enables++; }
static void SDL_OnApplicationWillEnterForeground(void) { assert(host_on_main_thread()); foreground_calls++; }
static void SDL_OnApplicationDidEnterForeground(void) { assert(host_on_main_thread()); window.flags = 0; }
static void request_scene(void) {
    assert(host_on_main_thread());
    assert(!chdir(other_root));
    assert(!scene_start());
}
static void host_logf(int priority, const char *format, ...) {
    (void)priority;
    if (strstr(format, "after the extra window scene's start")) {
        recovery_logs++;
        if (inject_during_recovery) {
            inject_during_recovery = 0;
            host_main_sync(^{ request_scene(); });
        }
    }
}
static BOOL close_extra_window_scenes(void) {
    __block BOOL closed;
    host_main_sync(^{
        close_calls++;
        if (inject_during_close) {
            inject_during_close = 0;
            request_scene();
        }
        closed = window_exists;
    });
    return closed;
}
#include "extra_scene.inc"

static void check_directory(void) {
    char directory[1024];
    assert(getcwd(directory, sizeof(directory)) && !strcmp(directory, data_root));
}
static void exercise(void) {
    int background = !host_on_main_thread();
    /* A missing game window delays closing but must still restore relative paths. */
    host_main_sync(^{ request_scene(); });
    host_extra_scene_poll();
    check_directory();
    assert(extra_scene_pending == 2 && close_calls == 1 && recovery_logs == 1);
    if (!background) assert(pump_enables == 1);
    host_extra_scene_poll();
    assert(extra_scene_pending == 2 && close_calls == 2 && recovery_logs == 1);
    host_main_sync(^{ window_exists = 1; });
    host_extra_scene_poll();
    assert(extra_scene_pending == 0 && extra_scene_watch == 600);
    host_main_sync(^{ window.flags = SDL_WINDOW_MINIMIZED; });
    host_extra_scene_poll();
    assert(extra_scene_watch == 0 && foreground_calls == 1);
    host_main_sync(^{ host_extra_scene_restore_watch(); });
    host_extra_scene_poll();
    assert(extra_scene_watch == 599);
    for (int poll = 0; poll < 600; poll++) host_extra_scene_poll();
    assert(extra_scene_watch == 0 && close_calls == 3);
    if (background) {
        /* A newer main() call must survive both the recovery and close transitions. */
        for (int phase = 0; phase < 3; phase++) {
            host_main_sync(^{ request_scene(); });
            if (phase == 2) {
                host_main_sync(^{ window_exists = 0; });
                host_extra_scene_poll();
                assert(extra_scene_pending == 2);
                host_main_sync(^{ window_exists = 1; });
            }
            inject_during_recovery = phase == 0;
            inject_during_close = phase != 0;
            host_extra_scene_poll();
            assert(extra_scene_pending == 1);
            host_extra_scene_poll();
            assert(extra_scene_pending == 0 && extra_scene_watch == 600);
            check_directory();
        }
        assert(pump_enables == 0);
    }
}
static void *worker(void *unused) {
    (void)unused;
    @autoreleasepool { exercise(); }
    atomic_store(&done, 1);
    return NULL;
}
int main(void) {
    alarm(10);
    assert(!host_game_thread_enabled());
    char original[1024];
    assert(getcwd(original, sizeof(original)));
    char temporary[] = "/tmp/visr-extra-scene-XXXXXX";
    assert(mkdtemp(temporary));
    assert(realpath(temporary, data_root));
    assert(snprintf(other_root, sizeof(other_root), "%s/scene", data_root) < (int)sizeof(other_root));
    assert(!mkdir(other_root, 0700));
    assert(!scene_start()); /* The first call does not request recovery. */
    assert(extra_scene_pending == 0);
    exercise(); /* Flag off: same main-thread path, including nested inline sync. */
    extra_scene_watch = 0;
    pump_enables = close_calls = recovery_logs = foreground_calls = window_exists = 0;
    threaded = 1;
    exercise(); /* Platforms without game-thread startup still need the legacy pump. */
    extra_scene_watch = 0;
    pump_enables = close_calls = recovery_logs = foreground_calls = window_exists = 0;
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, worker, NULL));
    while (!atomic_load(&done)) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
    assert(!pthread_join(thread, NULL));
    assert(!chdir(original));
    assert(!rmdir(other_root) && !rmdir(data_root));
    puts("extra-scene recovery, retained requests, and pump gating passed");
    return 0;
}
