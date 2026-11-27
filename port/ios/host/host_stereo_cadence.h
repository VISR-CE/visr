/* SPDX-License-Identifier: Apache-2.0 */
/* The headset's frame cadence, as pure functions the host and its probe share (port/ios/tests/stereo_cadence_probe.c).

The Compositor's frame repeat count: how many extra refreshes each frame stays up (0: every refresh, 90 Hz; 1: 45
Hz). One function decides it, and every writer (the space opening, debug.knob_cycle's cadence) goes through it
(host_stereo_repeat_refresh, host_stereo.m). Whether a frame was late, and whether its own work explains it, is
decided here too, for the cadence line in gpu_metal.m. */
#pragma once
#include <math.h>
#include "halo_stereo.h"

/* The repeat count for stereo MODE: 0 outside HEAD and SCREEN modes; FORCED (display.frame_repeat, clamped to 0..3)
when it is not 0; else 1 when LATCHED (a lock to steady 45 Hz, HEAD mode only); else 0. A forced count wins over the
lock, as the cadence session and the benchmarks need */
static inline int stereo_repeat_wanted(int mode, int forced, int latched)
{
	if (mode != HALO_STEREO_HEAD && mode != HALO_STEREO_SCREEN)
		return 0;
	if (forced > 0)
		return forced > 3 ? 3 : forced;
	return latched && mode == HALO_STEREO_HEAD ? 1 : 0;
}

/* ---------- lateness

A frame's presentation is predicted when it opens, after its tick and the previous frame's render and GPU work, so
those are what can make it late. The cadence line ("stereo: cadence", gpu_metal.m) and the trigger below share these
rules; neither keeps its own. Lateness is measured against the repeat count that was asked for: at 1 a frame that
arrives two refreshes after the previous one is on time. */

/* refreshes since the previous presentation, from the interval between them: 1 to MOST */
static inline long stereo_cadence_refreshes(double interval, double period, long most)
{
	long up = lround(interval / period);

	return up < 1 ? 1 : up > most ? most : up;
}

/* a frame arriving REFRESHES after the previous one is late at repeat count REPEAT */
static inline int stereo_cadence_late(long refreshes, int repeat)
{
	return refreshes > (long)repeat + 1;
}

/* the time one frame has at repeat count REPEAT */
static inline double stereo_cadence_slot(double period, int repeat)
{
	return period * (double)(repeat + 1);
}

/* the frame's own work explains its lateness: its TICK plus the PREVIOUS_RENDER (the previous frame's CPU time less
its tick), or the PREVIOUS_GPU time, ran past the SLOT (seconds, or any one unit). A late frame whose own work fit
was paced by the system */
static inline int stereo_cadence_own_work(double tick, double previous_render, double previous_gpu, double slot)
{
	return tick + previous_render > slot || previous_gpu > slot;
}
