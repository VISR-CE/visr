/* Smooth motion's automatic rule (port/linux/src/smooth_motion.h): on while the frame rate in force is above 30,
 * off at a 30 cap or lower; the desktop's display.max_fps counts only with vsync off. */
#include "smooth_motion.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	/* the Apple hosts' display.frame_rate */
	assert(smooth_motion_automatic(smooth_motion_cap("display", 1, 0, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("unlimited", 1, 0, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("", 1, 0, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 1, 0, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("120", 1, 0, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("60", 1, 0, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("31", 1, 0, 0)));
	assert(!smooth_motion_automatic(smooth_motion_cap("30", 1, 0, 0)));
	assert(!smooth_motion_automatic(smooth_motion_cap("24", 1, 0, 0)));
	/* a malformed value is no cap */
	assert(smooth_motion_automatic(smooth_motion_cap("60fps", 1, 0, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("-5", 1, 0, 0)));
	/* the desktop: max_fps is a cap only with vsync off (0: twice the refresh, -1: none) */
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 1, 30, 0)));
	assert(!smooth_motion_automatic(smooth_motion_cap(NULL, 0, 30, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 0, 60, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 0, 0, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 0, -1, 0)));
	/* the lower of the two caps wins */
	assert(!smooth_motion_automatic(smooth_motion_cap("60", 0, 30, 0)));
	assert(!smooth_motion_automatic(smooth_motion_cap("30", 0, 120, 0)));
	/* a Vision Pro's Compositor paces frames and ignores display.frame_rate and display.max_fps */
	assert(smooth_motion_automatic(smooth_motion_cap("30", 1, 0, 1)));
	assert(smooth_motion_automatic(smooth_motion_cap("30", 0, 30, 1)));
	assert(!smooth_motion_automatic(smooth_motion_cap("30", 1, 0, 0)));
	/* the frame rate setting is read in one place: 1 to 1000, "unlimited" and no cap */
	assert(frame_rate_setting_parse("30") == 30 && frame_rate_setting_parse("1000") == 1000);
	assert(frame_rate_setting_parse("1001") == 0 && frame_rate_setting_parse("0") == 0);
	assert(frame_rate_setting_parse("unlimited") == FRAME_RATE_SETTING_UNLIMITED);
	assert(frame_rate_setting_parse("display") == 0 && frame_rate_setting_parse("") == 0 && !frame_rate_setting_parse(NULL));
	/* the testing override: the old boolean's spellings still read */
	assert(smooth_motion_override("on") == 1 && smooth_motion_override("true") == 1 && smooth_motion_override("1") == 1);
	assert(smooth_motion_override("off") == 0 && smooth_motion_override("false") == 0 && smooth_motion_override("0") == 0);
	assert(smooth_motion_override("auto") == -1 && smooth_motion_override("") == -1 && smooth_motion_override(NULL) == -1);
	puts("PASS: smooth motion is on above a 30 frame rate and off at 30 or lower");
	return 0;
}
