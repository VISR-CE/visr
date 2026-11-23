/* Smooth motion's automatic rule (port/linux/src/smooth_motion.h): on while the frame rate in force is above 30,
 * off at a 30 cap or lower; the desktop's display.max_fps counts only with vsync off. */
#include "smooth_motion.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	/* the Apple hosts' display.frame_rate */
	assert(smooth_motion_automatic(smooth_motion_cap("display", 1, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("unlimited", 1, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("", 1, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 1, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("120", 1, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("60", 1, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("31", 1, 0)));
	assert(!smooth_motion_automatic(smooth_motion_cap("30", 1, 0)));
	assert(!smooth_motion_automatic(smooth_motion_cap("24", 1, 0)));
	/* a malformed value is no cap */
	assert(smooth_motion_automatic(smooth_motion_cap("60fps", 1, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap("-5", 1, 0)));
	/* the desktop: max_fps is a cap only with vsync off (0: twice the refresh, -1: none) */
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 1, 30)));
	assert(!smooth_motion_automatic(smooth_motion_cap(NULL, 0, 30)));
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 0, 60)));
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 0, 0)));
	assert(smooth_motion_automatic(smooth_motion_cap(NULL, 0, -1)));
	/* the lower of the two caps wins */
	assert(!smooth_motion_automatic(smooth_motion_cap("60", 0, 30)));
	assert(!smooth_motion_automatic(smooth_motion_cap("30", 0, 120)));
	puts("PASS: smooth motion is on above a 30 frame rate and off at 30 or lower");
	return 0;
}
