/* The frame pacer: when a frame may start toward the display, how long it
must stay there, and the game's 60 Hz blank, decided apart from whatever
draws the frame (frame_pacer.h has the reasons and the rules). A renderer
tells it three things and asks it two, with no graphics types either way, so
the Metal backend, the OpenGL ES one, another backend or a test can use the
same pacer:

	frame_pacer_frame_start   as the game starts a frame, before it reads input (may wait)
	frame_pacer_frame_begin   before a frame asks for its drawable (may wait)
	frame_pacer_frame_finished when the GPU finished the frame, with its times
	frame_pacer_present       as the frame is presented: how long it must stay up
	frame_pacer_presented     when the display showed it, with the time
	frame_pacer_frame_dropped the frame had nothing to show after all
	frame_pacer_refresh       each refresh of the display, from a display link

Compiled in with HALO_FRAME_PACING (port/ios/CMakeLists.txt; tools/ios_build.py
--frame-pacing): on for the Mac app, an iPhone, an iPad and an Apple TV, off
for a Vision Pro, where the Compositor is the pacer. Off, every function here
is an inline that does nothing, so a renderer's calls compile away and its
frames go as they did before the pacer existed.

Settings (port_config.c): display.frame_source, display.frames_in_flight,
display.frame_spacing, display.late_start, debug.present_times,
debug.frame_load_cpu, debug.frame_load_every. Whether frames wait for the
display at all is display.frame_rate's to say (host_gpu_dispatch.c and the
backend's frame_rate_sync, which the pacer is told the answer of). */
#pragma once

#include <stdint.h>

#ifndef HALO_FRAME_PACING
#include <TargetConditionals.h>
#define HALO_FRAME_PACING (!TARGET_OS_VISION)
#endif

/* one frame's dealings with the pacer; the renderer keeps it from
frame_pacer_frame_begin to frame_pacer_presented or frame_pacer_frame_dropped
(copied into the presented handler) and reads none of it */
struct frame_pacer_frame
{
	unsigned long number;
	double began;            /* when the frame's work began (the pacer's clock), for the delay to the display */
	double aim;              /* the refresh a late start aimed it at; 0: none */
	uint8_t waits_for_display;
	uint8_t slot;            /* holds one of the frames in flight */
	uint8_t watched;         /* frame_pacer_presented wants to hear of it */
	uint8_t counted;         /* counted as handed to the display and not yet shown (the extended late start) */
	uint8_t in_flight;       /* frames handed to the display and not yet shown, before this one */
	uint8_t start_state;     /* frame_pacer.h's frame_pacer_start_state */
};

#if HALO_FRAME_PACING

/* once, before the first frame; fixed_timestep (debug.fixed_timestep): the
pacer stays out of the way, so runs repeat */
void frame_pacer_start(int fixed_timestep);
/* frames are drawn unshown for a while (debug.benchmark_offscreen): nothing waits or is paced until
frame_pacer_start again */
void frame_pacer_stop(void);

/* Where frames that wait for the display get their place on it
(display.frame_source):

"clock": the renderer asks for a drawable when a frame is ready, and the
pacer's own rules apply (frames in flight, the minimum time on screen),
timed by a display link's refreshes. Works everywhere.

"system": the system's own pacer hands the renderer one drawable for each
refresh, with the delay it was asked to keep (CAMetalDisplayLink, from iOS
17, tvOS 17 and macOS 14, with preferredFrameLatency), and the pacer's rules
stand aside. It cannot draw faster than the display or hold a cap, so it is
used only with display.frame_rate = "display".

A renderer that has a system pacer asks frame_pacer_wants_system_source
before the first frame, and if it starts one, says so with
frame_pacer_system_source. frame_pacer_in_flight is the delay to ask it for
(0: the system's default). */
int frame_pacer_wants_system_source(void);
void frame_pacer_system_source(void);
int frame_pacer_in_flight(void);

/* display.frames_in_flight_at_30 as the game resolves it: 1 or 2, 0 for none (host_frames_in_flight_scene) */
void frame_pacer_in_flight_at_30_set(int at_30);

/* a refresh of the display: its time and the display's refresh period, in
seconds on the clock frame_pacer_presented's times use (CACurrentMediaTime's);
any thread */
void frame_pacer_refresh(double timestamp, double period);
/* the panel's own refresh period when it can run faster than the display link
(frame_pacer_minimum_duration_panel); 0 for the link's */
void frame_pacer_panel_period(double period);

/* The game is about to start a frame: called from its loop before it reads
the controllers and steps the blend between ticks (d3d8_device.c's
halo_frame_start, through gpu_frame_start). With display.late_start, waits
until the frame's work would be done just in time for the next refresh
(frame_pacer.h, "a late start"); otherwise only notes the time. Nothing
under debug.fixed_timestep, where the pacer isn't started. may_wait: 0 when
something else decides how long this frame stays up (display.frame_pacing's
whole-refresh holds), and the frame starts at once. */
void frame_pacer_frame_start(int may_wait);
/* whether the frame's late start gave it a refresh to aim at: its waiting for
the display was done before the input read, and a renderer's own wait for a
frame rate's refreshes inside its present is skipped */
static inline int frame_pacer_aimed(const struct frame_pacer_frame *frame)
{
	return frame->aim > 0.0;
}
/* the renderer handed the frame's slot and count to its presented handler
(a copy of the frame): this copy holds neither any more */
static inline void frame_pacer_frame_handed(struct frame_pacer_frame *frame)
{
	frame->slot = frame->counted = 0;
}
/* The GPU finished the frame's last command buffer (any thread). number: the
frame's, as given to frame_pacer_frame_begin; gpu_end: when, on the pacer's
clock (0 unknown); gpu_seconds: the GPU's time on this frame's own command
buffers; waited: how long the frame waited for the display inside its present
(a slot, a drawable, a frame rate's refreshes). The extended late start's
cost of a frame (frame_pacer.h). */
void frame_pacer_frame_finished(unsigned long number, double gpu_end, double gpu_seconds, double waited);
/* the picture has ratio times the pixels from the next frame on (dynamic
resolution changed its lines): the extended late start allows for the GPU's
longer work before the first such frame starts */
void frame_pacer_pixels(double ratio);
/* what the frame's start decided, as a word for a log or the frame timeline
row: "engaged", "no_fit", "holdoff" or "off" */
const char *frame_pacer_start_state_name(int state);

/* the GPU's time on the frames it has finished since the last call, in
seconds: a renderer's measure for the late start's guess (0 if it has none) */
void frame_pacer_gpu_time(double seconds);

/* display.late_start on and off while the game runs (the Mac app's F8): the
new setting */
int frame_pacer_late_start_toggle(void);

/* A frame is about to ask for its drawable. number: the frame's; began:
when its work began (0 unknown); waits_for_display: whether it will be shown
in step with the display (not when frames run faster than the display and
may not be shown at all, or when this frame is held back). Waits here for
one of the frames in flight's slots, 100 ms at most; on a Mac about one
refresh, after which the frame gives up its present (frame_pacer.h,
frame_pacer_slot_wait). Returns 1 to present the frame, 0 when it gives up
its present: the renderer then shows nothing (no drawable) and the frame is
dropped (frame_pacer_frame_dropped), its work done all the same. */
int frame_pacer_frame_begin(struct frame_pacer_frame *frame, unsigned long number, double began, int waits_for_display);

/* the frame is being presented: the least time it must stay on screen after
the frame before it, in seconds (presentDrawable:afterMinimumDuration:), or 0
to present it as it comes */
double frame_pacer_present(const struct frame_pacer_frame *frame);

/* whether frame_pacer_presented must be called for this frame once it is
shown (a renderer that can't say when frames are shown leaves the frame's
slot to time out, so it should set display.frames_in_flight = "system") */
static inline int frame_pacer_watches(const struct frame_pacer_frame *frame)
{
	return frame->watched;
}

/* the display showed the frame at this time (a drawable's presentedTime; 0
or less when the system doesn't say, as on a virtual display, and the time
of this call stands in for it, a little late); any thread */
void frame_pacer_presented(const struct frame_pacer_frame *frame, double at);

/* the frame had nothing to show (no drawable, in the background) */
void frame_pacer_frame_dropped(struct frame_pacer_frame *frame);

/* waits for the display's next 60 Hz blank and returns 1, or returns 0 when
the display isn't keeping one (no refresh yet, or none for 50 ms: the
background) and the caller must keep its own timer */
uint32_t frame_pacer_vertical_blank_wait(void);

#else

static inline void frame_pacer_start(int fixed_timestep)
{
	(void)fixed_timestep;
}

static inline void frame_pacer_stop(void)
{
}

static inline void frame_pacer_in_flight_at_30_set(int at_30)
{
	(void)at_30;
}

static inline int frame_pacer_wants_system_source(void)
{
	return 0;
}

static inline void frame_pacer_system_source(void)
{
}

static inline int frame_pacer_in_flight(void)
{
	return 0;
}

static inline void frame_pacer_refresh(double timestamp, double period)
{
	(void)timestamp;
	(void)period;
}

static inline void frame_pacer_frame_start(int may_wait)
{
	(void)may_wait;
}
static inline int frame_pacer_aimed(const struct frame_pacer_frame *frame)
{
	(void)frame;
	return 0;
}
static inline void frame_pacer_frame_handed(struct frame_pacer_frame *frame)
{
	(void)frame;
}
static inline void frame_pacer_frame_finished(unsigned long number, double gpu_end, double gpu_seconds, double waited)
{
	(void)number;
	(void)gpu_end;
	(void)gpu_seconds;
	(void)waited;
}
static inline void frame_pacer_pixels(double ratio)
{
	(void)ratio;
}
static inline const char *frame_pacer_start_state_name(int state)
{
	(void)state;
	return "off";
}

static inline void frame_pacer_gpu_time(double seconds)
{
	(void)seconds;
}

static inline int frame_pacer_late_start_toggle(void)
{
	return 0;
}

static inline int frame_pacer_frame_begin(struct frame_pacer_frame *frame, unsigned long number, double began, int waits_for_display)
{
	(void)frame;
	(void)number;
	(void)began;
	(void)waits_for_display;
	return 1;
}

static inline double frame_pacer_present(const struct frame_pacer_frame *frame)
{
	(void)frame;
	return 0.0;
}

static inline int frame_pacer_watches(const struct frame_pacer_frame *frame)
{
	(void)frame;
	return 0;
}

static inline void frame_pacer_presented(const struct frame_pacer_frame *frame, double at)
{
	(void)frame;
	(void)at;
}

static inline void frame_pacer_frame_dropped(struct frame_pacer_frame *frame)
{
	(void)frame;
}

static inline uint32_t frame_pacer_vertical_blank_wait(void)
{
	return 0;
}

static inline void frame_pacer_panel_period(double period)
{
	(void)period;
}

#endif

/* a guest import (guest_host.h), defined in host_frame_pacer.c whether or not the pacer is compiled in */
void host_frames_in_flight_scene(int at_30);
