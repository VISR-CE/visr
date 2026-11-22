/* A frame the Compositor paces must count as a benchmark frame whichever present path showed it. Class: a
   measurement fed by one presentation path while a platform presents through another (the device benchmark counted
   only the flat screen's presents, so every run in visionOS's immersive space ended with 0 frames). This runs the
   arithmetic every Compositor present path shares (host_stereo_measure.h) into the benchmark's own metrics
   (port/runtime/benchmark_metrics.c); tools/test_stereo_benchmark_frames.py checks that every such path in
   gpu_metal.m calls it. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_stereo_measure.h"
#include "benchmark_metrics.h"

static int failures, checks;
#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    fprintf(stderr, "stereo_benchmark_frames_probe: line %d: %s\n", __LINE__, #condition); } } while (0)
#define NEAR(a, b) (fabs((a) - (b)) < 1e-9)

/* what gpu_metal.m's stereo_pacing_count does with a reading: a run frame goes to the metrics */
static void count(struct benchmark_metrics *metrics, const struct host_stereo_measure_in *reading, int ticks) {
    struct host_stereo_measure_out measured = host_stereo_measure(reading);

    if (measured.run_frame && measured.interval > 0.0)
        benchmark_metrics_frame_split(metrics, measured.interval * 1000.0, measured.cpu > 0.0 ? measured.cpu * 1000.0 : -1.0,
            7.0, reading->simulation >= 0.0 ? reading->simulation * 1000.0 : -1.0,
            measured.render >= 0.0 ? measured.render * 1000.0 : -1.0,
            reading->translation >= 0.0 ? reading->translation * 1000.0 : -1.0, ticks);
}

/* frames of one view at 90 Hz with a tick every third frame: the frame's work begins at the last one's end, the
   Compositor's wait is 3 ms, inside the render span or inside the present */
static size_t run_view(int view, int wait_in_render, struct benchmark_summary *summary) {
    struct benchmark_metrics metrics = { 0 };
    double period = 1.0 / 90.0, presentation = 100.0, last = 0.0;
    size_t frames;

    benchmark_metrics_reset(&metrics, 90.0);
    for (int frame = 0; frame < 300; frame++) {
        int tick = frame % 3 == 0;
        struct host_stereo_measure_in reading = { view, presentation - 0.020, presentation - 0.020 - 0.009, presentation,
            last, 0.003, wait_in_render, tick ? 0.002 : 0.0, wait_in_render ? 0.007 : 0.004, 0.001 };

        count(&metrics, &reading, tick);
        last = presentation;
        presentation += period;
    }
    benchmark_metrics_summarize(&metrics, summary);
    frames = metrics.frames;
    benchmark_metrics_free(&metrics);
    return frames;
}

int main(void) {
    struct benchmark_summary summary;

    /* every view that is a run's frames counts one frame for each frame after the first (the first has no
       interval), with its CPU split, at one refresh each */
    for (int view = HOST_COMPOSITOR_VIEW_FULL; view <= HOST_COMPOSITOR_VIEW_THEATER_SCREEN; view++) {
        int wait_in_render = view != HOST_COMPOSITOR_VIEW_THEATER_SCREEN;
        size_t frames = run_view(view, wait_in_render, &summary);

        CHECK(frames == 299);
        CHECK(summary.frames == 299 && summary.split_frames == 299);
        CHECK(summary.shown[1] == 299);
        /* 9 ms from the work's start to the count, less the 3 ms wait, taken out once */
        CHECK(fabs(summary.cpu.median - 6.0) < 0.11);
        /* the render span: 7 ms with the wait inside it, 4 ms with the wait in the present; 4 ms of work either way */
        CHECK(fabs(summary.render_cpu.median - 4.0) < 0.11);
        CHECK(summary.ticks_per_frame > 0.3 && summary.ticks_per_frame < 0.36);
    }
    /* the UI's quad (menus, loads) is shown and is not a run's frame */
    CHECK(run_view(HOST_COMPOSITOR_VIEW_UI_QUAD, 1, &summary) == 0);

    /* one reading */
    {
        struct host_stereo_measure_in reading = { HOST_COMPOSITOR_VIEW_FULL, 10.0, 9.990, 10.02, 10.02 - 2.0 / 90.0, 0.004, 1,
            -1.0, -1.0, -1.0 };
        struct host_stereo_measure_out measured = host_stereo_measure(&reading);

        CHECK(NEAR(measured.interval, 2.0 / 90.0) && NEAR(measured.cpu, 0.006));
        /* a frame the game did not mark stays unmarked: never a zero that reads as a measurement */
        CHECK(measured.render < 0.0 && measured.run_frame);
        /* no presentation time, or none before it: no interval, so no frame is counted with a made-up one */
        reading.presentation = 0.0;
        CHECK(host_stereo_measure(&reading).interval == 0.0);
        reading.presentation = 10.02; reading.last_presentation = 0.0;
        CHECK(host_stereo_measure(&reading).interval == 0.0);
        /* a frame ended without a new Compositor frame repeats the last presentation time: no interval */
        reading.last_presentation = reading.presentation;
        CHECK(host_stereo_measure(&reading).interval == 0.0);
        /* the work's start unknown (the first frame): no CPU time */
        reading.work_started = 0.0;
        CHECK(host_stereo_measure(&reading).cpu == 0.0);
        /* a wait longer than the span it is taken from leaves zero, not a negative time */
        reading.work_started = 9.999; reading.render = 0.001;
        measured = host_stereo_measure(&reading);
        CHECK(measured.cpu == 0.0 && measured.render == 0.0);
        /* a wait inside the present leaves the render span as the game marked it */
        reading.wait_in_render = 0; reading.render = 0.005;
        CHECK(NEAR(host_stereo_measure(&reading).render, 0.005));
    }
    /* every view has a name in the report, and only the UI's quad is not a run's frame */
    for (int view = 0; view < HOST_COMPOSITOR_VIEW_COUNT; view++) {
        struct host_stereo_measure_in reading = { view, 10.0, 9.990, 10.02, 10.0, 0.0, 0, -1.0, -1.0, -1.0 };

        CHECK(strcmp(host_compositor_view_name(view), "unknown") != 0);
        CHECK(host_stereo_measure(&reading).run_frame == (view != HOST_COMPOSITOR_VIEW_UI_QUAD));
    }
    CHECK(strcmp(host_compositor_view_name(HOST_COMPOSITOR_VIEW_COUNT), "unknown") == 0);
    if (failures) {
        fprintf(stderr, "stereo_benchmark_frames_probe: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("stereo_benchmark_frames_probe: %d checks passed\n", checks);
    return 0;
}
