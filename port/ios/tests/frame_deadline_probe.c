/* display.frame_deadline (gpu_metal.m, frame_deadline.h), natively (tools/ios_test.py): the deadline each frame gets,
   and the workgroup calls the drawing thread makes, made here on a thread of this Mac as the renderer makes them. */
#include "frame_deadline.h"

#include <AudioToolbox/AudioWorkInterval.h>
#include <mach/mach_time.h>
#include <math.h>
#include <os/workgroup.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static int failures, checks;
#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    fprintf(stderr, "frame_deadline_probe: line %d: %s\n", __LINE__, #condition); } } while (0)
#define NEAR(a, b) (fabs((a) - (b)) < 1e-9)

static void periods(void) {
    /* a cap or a scene class's rate under the display's: its period */
    CHECK(NEAR(frame_deadline_period(30.0, 1.0 / 60.0), 1.0 / 30.0));
    CHECK(NEAR(frame_deadline_period(60.0, 1.0 / 120.0), 1.0 / 60.0));
    /* none: the display's refresh */
    CHECK(NEAR(frame_deadline_period(0.0, 1.0 / 120.0), 1.0 / 120.0));
    CHECK(NEAR(frame_deadline_period(0.0, 1.0 / 60.0), 1.0 / 60.0));
    /* nothing plausible (no refresh yet, a stalled link): 1/60 s */
    CHECK(NEAR(frame_deadline_period(0.0, 0.0), 1.0 / 60.0));
    CHECK(NEAR(frame_deadline_period(0.0, 0.5), 1.0 / 60.0));
    CHECK(NEAR(frame_deadline_period(1000.0, 1.0 / 60.0), 1.0 / 60.0));
}

/* leaving the workgroup before UIKit's loop (host_benchmark_menu.m's ask), joining again at the next shown frame */
static void membership(void) {
    /* only the joined thread leaves, and only while joined */
    CHECK(frame_deadline_state_after_leave(FRAME_DEADLINE_JOINED, 1) == FRAME_DEADLINE_LEFT);
    CHECK(frame_deadline_state_after_leave(FRAME_DEADLINE_JOINED, 0) == FRAME_DEADLINE_JOINED);
    CHECK(frame_deadline_state_after_leave(FRAME_DEADLINE_NEW, 1) == FRAME_DEADLINE_NEW);
    CHECK(frame_deadline_state_after_leave(FRAME_DEADLINE_OFF, 1) == FRAME_DEADLINE_OFF);
    CHECK(frame_deadline_state_after_leave(FRAME_DEADLINE_LEFT, 1) == FRAME_DEADLINE_LEFT);
    /* a shown frame rejoins; an offscreen benchmark frame, or a thread never joined, does not */
    CHECK(frame_deadline_rejoin_wanted(FRAME_DEADLINE_LEFT, 0));
    CHECK(!frame_deadline_rejoin_wanted(FRAME_DEADLINE_LEFT, 1));
    CHECK(!frame_deadline_rejoin_wanted(FRAME_DEADLINE_JOINED, 0));
    CHECK(!frame_deadline_rejoin_wanted(FRAME_DEADLINE_NEW, 0));
    CHECK(!frame_deadline_rejoin_wanted(FRAME_DEADLINE_OFF, 0));
}

/* the calls gpu_metal.m makes: join once on the drawing thread, then a start and a finish around each frame's work */
static void *frames(void *unused) {
    struct os_workgroup_join_token_opaque_s token;
    os_workgroup_interval_t interval = AudioWorkIntervalCreate("VISR frame probe", OS_CLOCK_MACH_ABSOLUTE_TIME, NULL);
    mach_timebase_info_data_t timebase;
    int frame;

    (void)unused;
    mach_timebase_info(&timebase);
    /* (a virtual machine without the audio workgroups, as a CI runner may be: said, not failed) */
    if (!interval) {
        printf("frame_deadline_probe: no workgroup interval on this Mac; the workgroup calls are not checked\n");
        return NULL;
    }
    CHECK(os_workgroup_join(interval, &token) == 0);
    for (frame = 0; frame < 5; frame++) {
        uint64_t now = mach_absolute_time();
        double period = frame_deadline_period(30.0, 1.0 / 60.0);

        CHECK(os_workgroup_interval_start(interval, now,
            now + (uint64_t)(period * 1e9 * (double)timebase.denom / (double)timebase.numer), NULL) == 0);
        usleep(2000);
        CHECK(os_workgroup_interval_finish(interval, NULL) == 0);
    }
    /* leave on the joining thread, run, join the same interval again: the system accepts it */
    os_workgroup_leave(interval, &token);
    usleep(1000);
    CHECK(os_workgroup_join(interval, &token) == 0);
    CHECK(os_workgroup_interval_start(interval, mach_absolute_time(), mach_absolute_time() + 1000000, NULL) == 0);
    CHECK(os_workgroup_interval_finish(interval, NULL) == 0);
    os_workgroup_leave(interval, &token);
    os_release(interval);
    return NULL;
}

int main(void) {
    pthread_t thread;

    periods();
    membership();
    if (__builtin_available(macOS 11.0, *)) {
        CHECK(pthread_create(&thread, NULL, frames, NULL) == 0);
        pthread_join(thread, NULL);
    }
    printf("frame_deadline_probe: %d checks %s\n", checks, failures ? "FAILED" : "passed");
    return failures ? 1 : 0;
}
