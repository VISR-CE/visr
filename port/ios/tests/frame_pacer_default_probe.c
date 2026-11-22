/* The frame pacer's default frames in flight as host_frame_pacer.c reads it (frame_pacer.h's
   frame_pacer_in_flight_default): two with the encode thread off, one with it on, and an explicit
   display.frames_in_flight over either. Built by tools/ios_test.py with the shared host stubs. */
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
    if (failures)
        return 1;
    printf("frame_pacer_default_probe: %d checks passed\n", checks);
    return 0;
}
