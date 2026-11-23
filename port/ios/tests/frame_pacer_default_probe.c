/* The frame pacer's default frames in flight as host_frame_pacer.c reads it (frame_pacer.h's
   frame_pacer_in_flight_default): two with the encode thread off, one with it on, and an explicit
   display.frames_in_flight over either; and the trace fields a frame carries for adaptive frames in flight (slot wait,
grant, present). Built by tools/ios_test.py with the shared host stubs. */
#include "host_frame_pacer.h"
#include "ios_host.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int encode_thread, checks, failures;
static double rate_target;
static const char *frames_in_flight = "";
static const char *frame_source = "";

#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    fprintf(stderr, "frame_pacer_default_probe: line %d: %s\n", __LINE__, #condition); } } while (0)

int host_gpu_encode_thread_on(void) { return encode_thread; }
int host_frame_rate_unlimited(void) { return 0; }
double host_frame_rate_cap(void) { return 0.0; }
double host_frame_rate_target(void) { return rate_target; }
double host_config_real(const char *name, double fallback) { (void)name; return fallback; }
void host_config_string(const char *name, const char *fallback, char *out, size_t size)
{
    snprintf(out, size, "%s", !strcmp(name, "display.frames_in_flight") ? frames_in_flight :
        !strcmp(name, "display.frame_source") ? frame_source : fallback);
}
void platform_log(const char *format, ...)
{
    (void)format;
}

static int started_in_flight(int thread, const char *setting)
{
    encode_thread = thread;
    frames_in_flight = setting;
    frame_pacer_start(0);
    frame_pacer_stop();
    return frame_pacer_in_flight();
}

int main(void)
{
    CHECK(started_in_flight(0, "") == 2);
    CHECK(started_in_flight(1, "") == 1);
    CHECK(started_in_flight(1, "2") == 2);
    CHECK(started_in_flight(0, "1") == 1);
    CHECK(started_in_flight(1, "system") == 0);
    /* (the system's pacer keeps a fixed limit: "auto" and the thread's default fall back to theirs, not one) */
    frame_source = "system";
    CHECK(started_in_flight(0, "auto") == 2);
    CHECK(started_in_flight(1, "") == 1);
    frame_source = "";
    /* one frame in flight: the second frame finds the slot taken and waits; both record their grant and present */
    {
        struct frame_pacer_frame first, second;

        encode_thread = 1;
        frames_in_flight = "1";
        frame_pacer_start(0);
        CHECK(frame_pacer_frame_begin(&first, 1, 0.0, 1) == 1);
        CHECK(first.slot && !first.slot_waited && !first.others_in_flight && first.granted > 0.0 && first.presenting == 0.0);
        frame_pacer_present(&first);
        CHECK(first.presenting >= first.granted);
        frame_pacer_frame_begin(&second, 2, 0.0, 1);
        CHECK(second.slot_waited && second.granted >= first.presenting);
        /* (it gave up waiting: the first frame still holds the one slot, so the second holds none) */
        CHECK(!second.slot && !second.others_in_flight);
        frame_pacer_presented(&first, 0.0);
        frame_pacer_frame_dropped(&second);
        frame_pacer_stop();
        /* two in flight: the second frame takes the second slot at once, with the first still in flight */
        frames_in_flight = "2";
        frame_pacer_start(0);
        frame_pacer_frame_begin(&first, 4, 0.0, 1);
        frame_pacer_frame_begin(&second, 5, 0.0, 1);
        CHECK(second.slot && !second.slot_waited && second.others_in_flight == 1);
        frame_pacer_presented(&first, 0.0);
        frame_pacer_presented(&second, 0.0);
        frame_pacer_stop();
        frames_in_flight = "";
        /* (a frame that does not wait for the display asks for no slot and records no grant) */
        frame_pacer_start(0);
        CHECK(frame_pacer_frame_begin(&first, 3, 0.0, 0) == 1 && !first.slot && first.granted == 0.0);
        frame_pacer_stop();
    }
    /* adaptive (the thread's default, or "auto"): it starts at two, and a fitting stream of presented frames takes it
       to one; an explicit "1" is fixed at one from the start */
    {
        static const char *const adaptive[][2] = { { "1", "" }, { "0", "auto" } };

        for (int c = 0; c < 2; c++)
        {
            struct frame_pacer_frame frame[2];
            double at = 1000.0;

            encode_thread = adaptive[c][0][0] == '1';
            frames_in_flight = adaptive[c][1];
            frame_pacer_start(0);
            frame_pacer_refresh(at, 1.0 / 60.0);
            frame_pacer_gpu_time(0.004);
            frame_pacer_frame_begin(&frame[0], 1, 0.0, 1);
            frame_pacer_frame_begin(&frame[1], 2, 0.0, 1);
            CHECK(frame[0].slot && frame[1].slot && frame[1].others_in_flight == 1);
            frame_pacer_presented(&frame[0], at);
            frame_pacer_presented(&frame[1], at += 1.0 / 60.0);
            for (unsigned long number = 3; number < 3 + 120; number++)
            {
                frame_pacer_frame_begin(&frame[0], number, 0.0, 1);
                frame_pacer_present(&frame[0]);
                frame_pacer_presented(&frame[0], at += 1.0 / 60.0);
            }
            /* (now at one: a frame that holds the slot makes the next one wait) */
            frame_pacer_frame_begin(&frame[0], 200, 0.0, 1);
            frame_pacer_frame_begin(&frame[1], 201, 0.0, 1);
            CHECK(frame[0].slot && frame[1].slot_waited && !frame[1].slot);
            /* (the wait it logs runs from the wait's start to the grant) */
            CHECK(frame[1].wait_began > 0.0 && frame[1].granted >= frame[1].wait_began && frame[0].wait_began == 0.0);
            frame_pacer_presented(&frame[0], at += 1.0 / 60.0);
            frame_pacer_stop();
        }
        /* explicit one: fixed from the first frame */
        encode_thread = 1;
        frames_in_flight = "1";
        frame_pacer_start(0);
        {
            struct frame_pacer_frame frame[2];

            frame_pacer_frame_begin(&frame[0], 1, 0.0, 1);
            frame_pacer_frame_begin(&frame[1], 2, 0.0, 1);
            CHECK(frame[0].slot && frame[1].slot_waited && !frame[1].slot);
            frame_pacer_presented(&frame[0], 0.0);
        }
        frame_pacer_stop();
        frames_in_flight = "";
    }
    /* the game calls frame_pacer_frame_start every frame, which sets the limit again from the rule and
    display.frames_in_flight_at_30: a stream whose GPU time does not fit a refresh must still get its second slot, and
    the at-30 cap pins one only while the frame rate in force is 30 or lower (main's cap once overwrote the rule's two) */
    {
        static const struct { double target; int at_30; int second_slot; } cases[] = {
            { 0.0, 0, 1 }, { 60.0, 0, 1 }, { 60.0, 1, 1 }, { 30.0, 0, 1 }, { 30.0, 2, 1 }, { 30.0, 1, 0 }, { 29.97, 1, 0 },
        };

        for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++)
        {
            struct frame_pacer_frame frame[2];

            encode_thread = 1;
            frames_in_flight = "";
            rate_target = cases[index].target;
            frame_pacer_start(0);
            frame_pacer_in_flight_at_30_set(cases[index].at_30);
            frame_pacer_refresh(1000.0, 1.0 / 60.0);
            frame_pacer_gpu_time(0.020);
            frame_pacer_frame_start();
            frame_pacer_frame_begin(&frame[0], 1, 0.0, 1);
            frame_pacer_frame_start();
            frame_pacer_frame_begin(&frame[1], 2, 0.0, 1);
            CHECK(frame[0].slot);
            CHECK(!!frame[1].slot == cases[index].second_slot);
            frame_pacer_presented(&frame[0], 1000.0);
            if (frame[1].slot)
                frame_pacer_presented(&frame[1], 1000.02);
            frame_pacer_stop();
        }
        rate_target = 0.0;
    }
    if (failures)
        return 1;
    printf("frame_pacer_default_probe: %d checks passed\n", checks);
    return 0;
}
