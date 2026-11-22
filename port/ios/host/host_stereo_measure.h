/* What one Compositor-paced frame contributes to the measurements: the device
   benchmark's frame, the overlay and the performance reports. Every present
   path that hands a picture to the Compositor (gpu_metal.m: the eyes, the
   eyes on the theater screen, the theater screen's mono picture and the UI's
   quad from either present entry) passes its readings through here, so a path
   cannot be shown and go uncounted. Plain C: the probe
   (port/ios/tests/stereo_benchmark_frames_probe.c) runs the same arithmetic. */
#ifndef HALO_HOST_STEREO_MEASURE_H
#define HALO_HOST_STEREO_MEASURE_H

enum host_compositor_view {
    /* head-tracked eyes */
    HOST_COMPOSITOR_VIEW_FULL,
    /* the eyes on the theater screen */
    HOST_COMPOSITOR_VIEW_STEREO_SCREEN,
    /* the theater screen's mono picture */
    HOST_COMPOSITOR_VIEW_THEATER_SCREEN,
    /* the UI's quad: menus and loads, which are not a run's frames */
    HOST_COMPOSITOR_VIEW_UI_QUAD,
    HOST_COMPOSITOR_VIEW_COUNT
};

/* the view's name in a benchmark report's "compositor" object */
static inline const char *host_compositor_view_name(int view) {
    switch (view) {
    case HOST_COMPOSITOR_VIEW_FULL: return "full";
    case HOST_COMPOSITOR_VIEW_STEREO_SCREEN: return "stereo_screen";
    case HOST_COMPOSITOR_VIEW_THEATER_SCREEN: return "theater_screen";
    case HOST_COMPOSITOR_VIEW_UI_QUAD: return "ui_quad";
    default: return "unknown";
    }
}

struct host_stereo_measure_in {
    int view;
    /* seconds on one clock: now, when the frame's work began (0: unknown), and the Compositor's predicted
       presentation time of this frame and of the one before (0: unknown) */
    double now, work_started, presentation, last_presentation;
    /* the seconds since work_started spent waiting for the Compositor's frame, taken once */
    double waited;
    /* 1 when that wait fell inside the game's render span (the frame was opened at the game's frame begin), 0 when
       it fell inside the present: the theater code, which opens the frame, says which (host_theater_take_waited) */
    int wait_in_render;
    /* the game's marks (host_frame_cpu_split): negative where it marked none */
    double simulation, render, translation;
};

struct host_stereo_measure_out {
    /* seconds; interval 0 where either presentation time is unknown, cpu 0 where the work's start is. A frame the
       Compositor opened and then ended empty or canceled still has a new presentation time, so it has an interval
       and is counted; telling those apart waits for the frame timeline's unshown flag. */
    double interval, cpu;
    /* the render span without the Compositor's wait; negative where unknown */
    double render;
    /* whether the frame is one of a benchmark run's: the UI's quad is not */
    int run_frame;
};

static inline struct host_stereo_measure_out host_stereo_measure(const struct host_stereo_measure_in *in) {
    struct host_stereo_measure_out out = { 0.0, 0.0, in->render, in->view != HOST_COMPOSITOR_VIEW_UI_QUAD };
    double waited = in->waited > 0.0 ? in->waited : 0.0;

    if (in->presentation > 0.0 && in->last_presentation > 0.0 && in->presentation > in->last_presentation)
        out.interval = in->presentation - in->last_presentation;
    if (in->work_started > 0.0) {
        out.cpu = in->now - in->work_started - waited;
        if (out.cpu < 0.0) out.cpu = 0.0;
    }
    if (in->wait_in_render && out.render >= 0.0) {
        out.render -= waited;
        if (out.render < 0.0) out.render = 0.0;
    }
    return out;
}

#endif
