/* Smooth motion (frames drawn between the game's 30 ticks a second) is automatic: on while the frame rate in
force is above 30, off at a 30 cap or lower. This is the one rule; config_smooth_motion (port_config.c) applies
it, and the renderer (halo_interpolation_enabled) and the performance reports call that. display.interpolation
is a testing override of it ("auto", "on" or "off"), never a menu row. */
#ifndef SMOOTH_MOTION_H
#define SMOOTH_MOTION_H

#include "frame_rate_setting.h"

/* the cap a display.frame_rate value sets, in frames a second: 0 for none ("display", "unlimited", empty or
anything that is not a number from 1 to 1000, all of which draw above 30 on a display that refreshes faster) */
static inline long smooth_motion_cap_of(const char *frame_rate)
{
	long cap = frame_rate_setting_parse(frame_rate);

	return cap > 0 ? cap : 0;
}

/* the frame rate in force from the two caps (0: none). The desktop's display.max_fps only caps with vsync off.
compositor_paced (a Vision Pro, whose Compositor paces frames and ignores display.frame_rate) has no cap of its own */
static inline long smooth_motion_cap(const char *frame_rate, int vsync, long max_fps, int compositor_paced)
{
	long cap = compositor_paced ? 0 : smooth_motion_cap_of(frame_rate);

	if (!compositor_paced && !vsync && max_fps > 0 && (!cap || max_fps < cap))
		cap = max_fps;
	return cap;
}

/* whether smooth motion is on at that cap */
static inline int smooth_motion_automatic(long cap)
{
	return cap <= 0 || cap > 30;
}

/* display.interpolation: 1 for "on", 0 for "off", -1 for "auto" (and for anything else). The old boolean's
spellings read the same, so HALO_INTERPOLATION=false still means off */
static inline int smooth_motion_override(const char *value)
{
	if (!value)
		return -1;
	if (!strcmp(value, "on") || !strcmp(value, "true") || !strcmp(value, "1"))
		return 1;
	if (!strcmp(value, "off") || !strcmp(value, "false") || !strcmp(value, "0"))
		return 0;
	return -1;
}

#endif
