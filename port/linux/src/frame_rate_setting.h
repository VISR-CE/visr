/* display.frame_rate's text, read in one place: "display" (or anything that is not a number from 1 to 1000) is no
cap, "unlimited" is no limit, a number from 1 to 1000 is a cap in frames a second. Its readers are the renderer's start
(d3d8_device.c), the profile safe point (port_config.c) and the smooth motion rule (smooth_motion.h). */
#ifndef FRAME_RATE_SETTING_H
#define FRAME_RATE_SETTING_H

#include <stdlib.h>
#include <string.h>

#define FRAME_RATE_SETTING_UNLIMITED (-1L)
#define FRAME_RATE_SETTING_MAXIMUM 1000L

/* the cap in frames a second, FRAME_RATE_SETTING_UNLIMITED for "unlimited", 0 for none */
static inline long frame_rate_setting_parse(const char *text)
{
	char *end;
	long cap;

	if (!text || !*text)
		return 0;
	if (!strcmp(text, "unlimited"))
		return FRAME_RATE_SETTING_UNLIMITED;
	cap = strtol(text, &end, 10);
	return *end || cap < 1 || cap > FRAME_RATE_SETTING_MAXIMUM ? 0 : cap;
}

#endif
