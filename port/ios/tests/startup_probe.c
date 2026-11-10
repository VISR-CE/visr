#include "host_startup.h"
#include "host_main_thread.h"
#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static double live_scale = 2.0;
static atomic_int done;
/* Replace only the platform read; the snapshot and main dispatch are real. */
void host_startup_read_live(struct host_startup_info *info) {
    assert(host_on_main_thread());
    *info = (struct host_startup_info){ .screen_scale = live_scale,
        .pixel_width = 1179.5, .pixel_height = 2556.25, .maximum_frames_per_second = 59.94 };
    snprintf(info->documents_root, sizeof(info->documents_root), "/documents/%.0f", live_scale);
}
static void *before_capture(void *unused) {
    (void)unused;
    assert(host_startup_get()->screen_scale == 2.0);
    atomic_store(&done, 1);
    return NULL;
}
static void *after_capture(void *unused) {
    (void)unused;
    assert(host_startup_get()->screen_scale == 3.0);
    assert(host_startup_get()->pixel_width == 1179.5);
    assert(host_startup_get()->pixel_height == 2556.25);
    assert(host_startup_get()->maximum_frames_per_second == 59.94);
    assert(!strcmp(host_startup_get()->documents_root, "/documents/3"));
    host_startup_capture();
    assert(host_startup_get()->screen_scale == 3.0);
    return NULL;
}
static void timed_out(int number) { (void)number; _exit(1); }
int main(void) {
    signal(SIGALRM, timed_out); alarm(10);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, before_capture, NULL) == 0);
    while (!atomic_load(&done)) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, false);
    assert(pthread_join(thread, NULL) == 0);
    live_scale = 3.0;
    assert(host_startup_get()->screen_scale == 3.0);
    host_startup_capture();
    live_scale = 4.0;
    /* Joining without pumping main proves captured worker reads do not dispatch. */
    assert(pthread_create(&thread, NULL, after_capture, NULL) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert(host_startup_get()->screen_scale == 3.0);
    alarm(0);
    puts("PASS: startup fallback runs on main; capture stays stable without main dispatch");
}
