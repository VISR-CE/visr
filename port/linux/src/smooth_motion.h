/* Smooth motion (frames drawn between the game's 30 ticks a second) is automatic: on while the frame rate in
force is above 30, off at a 30 cap or lower. This is the one rule; config_smooth_motion (port_config.c) applies
it, and the renderer (halo_interpolation_enabled) and the performance reports call that. display.interpolation is a testing override of it, never a menu row. */
#ifndef SMOOTH_MOTION_H
#define SMOOTH_MOTION_H

#include <stdlib.h>
#include <string.h>

/* the cap a display.frame_rate value sets, in frames a second: 0 for none ("display", "unlimited", empty or
anything that is not a number, all of which draw above 30 on a display that refreshes faster) */
static inline long smooth_motion_cap_of(const char *frame_rate)
{
	char *end;
	long rate;

	if (!frame_rate || !*frame_rate)
		return 0;
	rate = strtol(frame_rate, &end, 10);
	return *end || rate < 1 ? 0 : rate;
}

/* the frame rate in force from the two caps (0: none). The desktop's display.max_fps only caps with vsync off */
static inline long smooth_motion_cap(const char *frame_rate, int vsync, long max_fps)
{
	long cap = smooth_motion_cap_of(frame_rate);

	if (!vsync && max_fps > 0 && (!cap || max_fps < cap))
		cap = max_fps;
	return cap;
}

/* whether smooth motion is on at that cap */
static inline int smooth_motion_automatic(long cap)
{
	return cap <= 0 || cap > 30;
}

#endif
