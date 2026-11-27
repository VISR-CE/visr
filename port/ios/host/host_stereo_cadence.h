/* SPDX-License-Identifier: Apache-2.0 */
/* The headset's frame cadence, as pure functions the host and its probe share (port/ios/tests/stereo_cadence_probe.c).

The Compositor's frame repeat count: how many extra refreshes each frame stays up (0: every refresh, 90 Hz; 1: 45
Hz). One function decides it, and every writer (the space opening, debug.knob_cycle's cadence) goes through it
(host_stereo_repeat_refresh, host_stereo.m). */
#pragma once
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
