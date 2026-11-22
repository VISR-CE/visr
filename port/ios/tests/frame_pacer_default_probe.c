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
static const char *frames_in_flight = "";

#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    fprintf(stderr, "frame_pacer_default_probe: line %d: %s\n", __LINE__, #condition); } } while (0)

int host_gpu_encode_thread_on(void) { return encode_thread; }
int host_frame_rate_unlimited(void) { return 0; }
double host_frame_rate_cap(void) { return 0.0; }
double host_frame_rate_target(void) { return 0.0; }
double host_config_real(const char *name, double fallback) { (void)name; return fallback; }
void host_config_string(const char *name, const char *fallback, char *out, size_t size)
{
    snprintf(out, size, "%s", !strcmp(name, "display.frames_in_flight") ? frames_in_flight : fallback);
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
    /* one frame in flight: the second frame finds the slot taken and waits; both record their grant and present */
    {
        struct frame_pacer_frame first, second;

        encode_thread = 1;
        frames_in_flight = "";
        frame_pacer_start(0);
        CHECK(frame_pacer_frame_begin(&first, 1, 0.0, 1) == 1);
        CHECK(first.slot && !first.slot_waited && first.granted > 0.0 && first.presenting == 0.0);
        frame_pacer_present(&first);
        CHECK(first.presenting >= first.granted);
        frame_pacer_frame_begin(&second, 2, 0.0, 1);
        CHECK(second.slot_waited && second.granted >= first.presenting);
        frame_pacer_presented(&first, 0.0);
        frame_pacer_frame_dropped(&second);
        frame_pacer_stop();
        /* (a frame that does not wait for the display asks for no slot and records no grant) */
        frame_pacer_start(0);
        CHECK(frame_pacer_frame_begin(&first, 3, 0.0, 0) == 1 && !first.slot && first.granted == 0.0);
        frame_pacer_stop();
    }
    if (failures)
        return 1;
    printf("frame_pacer_default_probe: %d checks passed\n", checks);
    return 0;
}
