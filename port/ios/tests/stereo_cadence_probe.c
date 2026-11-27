/* SPDX-License-Identifier: Apache-2.0 */
/* host_stereo_cadence.h: the headset cadence's pure functions (tools/ios_test.py). */
#include "host_stereo_cadence.h"
#include <math.h>
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

/* a fixed pseudo-random sequence, 0 to 1 */
static double next_random(unsigned long *state)
{
	*state = *state * 6364136223846793005ul + 1442695040888963407ul;
	return (double)((*state >> 33) & 0xffffff) / (double)0xffffff;
}

/* the cadence line's counts before the rules moved into the header (62b4ade7f): refreshes shown, late at 2 or more,
and paced when the tick plus the previous render and the previous GPU time each fit one refresh */
static long refreshes_before(double interval, double period, long most)
{
	long up = lround(interval / period);

	return up < 1 ? 1 : up > most ? most : up;
}

/* synthetic records for one report: intervals of one to four refreshes with jitter, tick frames every third, times
around the refresh period */
#define RECORDS 600
static double intervals[RECORDS], ticks[RECORDS], renders[RECORDS], gpus[RECORDS];

static void records(unsigned long seed, double period)
{
	int index;

	for (index = 0; index < RECORDS; index++) {
		double r = next_random(&seed);

		intervals[index] = period * (r < 0.5 ? 1.0 : r < 0.8 ? 2.0 : r < 0.95 ? 3.0 : 4.0) *
			(0.9 + 0.2 * next_random(&seed));
		ticks[index] = index % 3 == 0 ? period * 1.2 * next_random(&seed) : 0.0;
		renders[index] = period * 0.9 * next_random(&seed);
		gpus[index] = period * 1.3 * next_random(&seed);
	}
}

/* at repeat count 0 the shared rules give exactly the cadence line's old counts */
static void lateness_matches_the_old_line(void)
{
	unsigned long seed;
	double period = 1.0 / 90.0;

	for (seed = 1; seed <= 20; seed++) {
		unsigned long shown_before[4] = { 0 }, shown[4] = { 0 }, paced_before = 0, paced = 0;
		int index;

		records(seed, period);
		for (index = 0; index < RECORDS; index++) {
			long before = refreshes_before(intervals[index], period, 3);
			long up = stereo_cadence_refreshes(intervals[index], period, 3);

			shown_before[before]++;
			shown[up]++;
			if (before >= 2 && index > 0 && ticks[index] + renders[index - 1] <= period && gpus[index - 1] <= period)
				paced_before++;
			if (stereo_cadence_late(up, 0) && index > 0 &&
				!stereo_cadence_own_work(ticks[index], renders[index - 1], gpus[index - 1], stereo_cadence_slot(period, 0)))
				paced++;
		}
		check(shown[1] == shown_before[1] && shown[2] == shown_before[2] && shown[3] == shown_before[3],
			"repeat count 0: refreshes counted differently");
		check(paced == paced_before, "repeat count 0: paced frames counted differently");
		check(paced_before > 0, "repeat count 0: the synthetic records have no paced frames to compare");
	}
}

/* at repeat count 1 the asked cadence is on time: a frame two refreshes after the previous one is not late, and
its own work has two refreshes */
static void lateness_at_the_asked_count(void)
{
	double period = 1.0 / 90.0;

	check(!stereo_cadence_late(1, 0) && stereo_cadence_late(2, 0) && stereo_cadence_late(3, 0), "repeat 0: late from 2");
	check(!stereo_cadence_late(1, 1) && !stereo_cadence_late(2, 1) && stereo_cadence_late(3, 1), "repeat 1: late from 3");
	check(!stereo_cadence_late(3, 2) && stereo_cadence_late(4, 2), "repeat 2: late from 4");
	check(stereo_cadence_refreshes(0.0, period, 5) == 1 && stereo_cadence_refreshes(10.0 * period, period, 5) == 5,
		"refreshes clamped to 1..most");
	check(stereo_cadence_refreshes(2.4 * period, period, 5) == 2 && stereo_cadence_refreshes(2.6 * period, period, 5) == 3,
		"refreshes rounded");
	/* 15 ms of work: the frame's own at 90 Hz, not at 45 */
	check(stereo_cadence_own_work(0.006, 0.009, 0.005, stereo_cadence_slot(period, 0)), "repeat 0: 15 ms is own work");
	check(!stereo_cadence_own_work(0.006, 0.009, 0.005, stereo_cadence_slot(period, 1)), "repeat 1: 15 ms fits");
	check(stereo_cadence_own_work(0.0, 0.0, 0.023, stereo_cadence_slot(period, 1)), "repeat 1: 23 ms of GPU is own work");
}

int main(void)
{
	repeat_wanted();
	lateness_matches_the_old_line();
	lateness_at_the_asked_count();
	if (!failures)
		printf("stereo cadence probe: ok\n");
	return failures ? 1 : 0;
}
