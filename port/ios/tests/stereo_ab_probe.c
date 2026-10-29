#include "host_stereo_ab.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef HOST_STEREO_AB_PROBE
void host_stereo_ab_sample(double gpu_seconds);
void host_stereo_ab_change(const char *method, float quality, int shadows);
static int reads, logs, enabled;
static char message[256];
void host_config_string(const char *name, const char *fallback, char *out, size_t size)
{
    reads++;
    snprintf(out, size, "%s", !strcmp(name, "debug.knob_cycle") ? enabled ? "true" : "false" : fallback);
}
double host_config_real(const char *name, double fallback) { (void)name; return fallback; }
void host_logf(int priority, const char *format, ...)
{
    (void)priority;
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    logs++;
}
static void host_test(int active)
{
    enabled = active;
    for (int i = 0; i < 360; i++) host_stereo_ab_sample(0.008);
    if (!enabled)
    {
        assert(reads == 1 && logs == 0);
        return;
    }
    assert(logs == 1 && reads == 2);
    assert(!strcmp(message, "stereo: a/b: off, quality 0.60, shadows 512: GPU 8.0 ms, p95 8.0 over 300 frames"));
    host_stereo_ab_change("fxaa", 0.5f, 128);
    for (int i = 0; i < 100; i++) host_stereo_ab_sample(0.050);
    host_stereo_ab_change("msaa2x", 0.7f, 512);
    for (int i = 0; i < 360; i++) host_stereo_ab_sample(0.009);
    assert(logs == 2 && reads == 2);
    assert(!strcmp(message, "stereo: a/b: msaa2x, quality 0.70, shadows 512: GPU 9.0 ms, p95 9.0 over 300 frames"));
    for (int i = 0; i < 400; i++) host_stereo_ab_sample(0.020);
    assert(logs == 2 && reads == 2);
}
#endif

int main(int argc, char **argv)
{
#ifdef HOST_STEREO_AB_PROBE
    host_test(argc > 1 && !strcmp(argv[1], "enabled"));
#else
    (void)argc;
    (void)argv;
#endif
    struct host_stereo_ab_window window = {0};
    double median, high;
    assert(!host_stereo_ab_add(&window, 1.0, &median, &high));
    host_stereo_ab_restart(&window);
    for (int i = 0; i < 60; i++)
        assert(!host_stereo_ab_add(&window, 9.0, &median, &high));
    for (int i = 1; i < 300; i++)
        assert(!host_stereo_ab_add(&window, i / 1000.0, &median, &high));
    assert(host_stereo_ab_add(&window, 0.300, &median, &high));
    assert(fabs(median - 151.0) < 0.001 && fabs(high - 286.0) < 0.001);
    assert(!host_stereo_ab_add(&window, 1.0, &median, &high));
    host_stereo_ab_restart(&window);
    for (int i = 0; i < 100; i++)
        assert(!host_stereo_ab_add(&window, 1.0, &median, &high));
    host_stereo_ab_restart(&window);
    for (int i = 0; i < 359; i++)
        assert(!host_stereo_ab_add(&window, 0.008, &median, &high));
    assert(host_stereo_ab_add(&window, 0.008, &median, &high));
    assert(fabs(median - 8.0) < 0.001 && fabs(high - 8.0) < 0.001);
    puts("PASS: stereo A/B skips 60, collects 300, reports once and restarts");
}
