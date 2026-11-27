/* SPDX-License-Identifier: Apache-2.0 */
/* host_stereo_cadence.h: the headset cadence's pure functions (tools/ios_test.py). */
#include "host_stereo_cadence.h"
#include <stdio.h>

static int failures;

static void check(int ok, const char *what)
{
	if (!ok) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

/* the repeat count's one owner: outside HEAD and SCREEN nothing repeats; a forced count wins, clamped; then the
lock, in HEAD mode only */
static void repeat_wanted(void)
{
	int mode;

	for (mode = HALO_STEREO_OFF; mode <= HALO_STEREO_SIDE_BY_SIDE; mode++) {
		int stereo = mode == HALO_STEREO_HEAD || mode == HALO_STEREO_SCREEN;

		check(stereo_repeat_wanted(mode, 0, 0) == 0, "no count asked: every refresh");
		check(stereo_repeat_wanted(mode, 2, 0) == (stereo ? 2 : 0), "forced count only in HEAD and SCREEN");
		check(stereo_repeat_wanted(mode, 2, 1) == (stereo ? 2 : 0), "a forced count wins over the lock");
		check(stereo_repeat_wanted(mode, 9, 0) == (stereo ? 3 : 0), "a forced count is clamped to 3");
		check(stereo_repeat_wanted(mode, -1, 0) == 0, "a negative forced count is none");
		check(stereo_repeat_wanted(mode, 0, 1) == (mode == HALO_STEREO_HEAD ? 1 : 0), "the lock: HEAD mode only");
		check(stereo_repeat_wanted(mode, -1, 1) == (mode == HALO_STEREO_HEAD ? 1 : 0), "a negative count leaves the lock");
	}
}

int main(void)
{
	repeat_wanted();
	if (!failures)
		printf("stereo cadence probe: ok\n");
	return failures ? 1 : 0;
}
