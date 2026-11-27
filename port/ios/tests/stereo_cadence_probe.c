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

/* ---------- the trigger */

#define PERIOD (1.0 / 90.0)

/* a synthetic game: each frame's refreshes, whether it ticked, its tick and the previous frame's render and GPU
times (milliseconds), from a pattern repeated for SECONDS, with the period measured and the headset cool unless
told otherwise. Returns the events seen; *FIRED_AT gets when "would drop" first fired (seconds, -1 if not) */
struct frame_kind { long refreshes; int ticked; double tick, previous_render, previous_gpu; };

static int run(struct stereo_cadence_trigger *trigger, double *now, double seconds, const struct frame_kind *pattern,
	int kinds, int period_measured, int warm, double *fired_at, unsigned long *started)
{
	double end = *now + seconds;
	int events = 0, index = 0;

	if (fired_at)
		*fired_at = -1.0;
	while (*now < end) {
		const struct frame_kind *frame = &pattern[index++ % kinds];
		*now += (double)frame->refreshes * PERIOD;
		if (stereo_cadence_window_add(trigger, *now, frame->ticked, frame->refreshes, frame->tick / 1000.0,
			frame->previous_render / 1000.0, frame->previous_gpu / 1000.0, PERIOD)) {
			int seen = stereo_cadence_decide(trigger, period_measured, warm, NULL, NULL);

			if ((seen & STEREO_CADENCE_WOULD_DROP) && fired_at && *fired_at < 0.0)
				*fired_at = *now;
			if ((seen & STEREO_CADENCE_STREAK_STARTED) && started)
				(*started)++;
			events |= seen;
		}
	}
	return events;
}

/* M1's cadences at repeat count 0 */
static const struct frame_kind clean_90[] = { { 1, 1, 2.2, 5.0, 7.7 }, { 1, 0, 0.0, 5.6, 7.7 }, { 1, 0, 0.0, 5.6, 7.7 } };
/* 64 players, drifting: tick frames 198/4 */
static const struct frame_kind drifting_64[] = { { 1, 1, 2.9, 7.3, 8.5 }, { 1, 0, 0.0, 7.3, 8.5 }, { 1, 0, 0.0, 7.3, 8.5 },
	{ 1, 1, 2.9, 7.3, 8.5 }, { 1, 0, 0.0, 7.3, 8.5 }, { 1, 0, 0.0, 7.3, 8.5 }, { 2, 1, 4.0, 8.0, 8.5 },
	{ 1, 0, 0.0, 7.3, 8.5 }, { 1, 0, 0.0, 7.3, 8.5 } };
/* 96 players, the clean 2,1 lock: a tick frame of 4.7 ms after a 10.5 ms render takes two refreshes */
static const struct frame_kind lock_96[] = { { 2, 1, 4.7, 10.5, 7.4 }, { 1, 0, 0.0, 6.0, 7.4 } };
/* 128 players at repeat count 0: tick frames all late by their own work, other frames 142/105, a few late frames
the system's (at most 11% in the measured runs) */
static const struct frame_kind heavy_128[] = { { 2, 1, 6.8, 15.4, 9.2 }, { 1, 0, 0.0, 8.0, 9.0 },
	{ 2, 0, 0.0, 15.4, 16.3 }, { 2, 1, 6.8, 15.4, 9.2 }, { 1, 0, 0.0, 8.0, 9.0 }, { 2, 0, 0.0, 15.4, 16.3 },
	{ 2, 1, 6.8, 15.4, 9.2 }, { 1, 0, 0.0, 8.0, 9.0 }, { 2, 0, 0.0, 8.0, 9.0 } };
/* repeat count 1 forced at 64 players, scored as if 0: every frame two refreshes, its work mostly within one
refresh (the measured leak gives the system 523 to 597 of about 600) */
static const struct frame_kind forced_45_64[] = { { 2, 1, 2.9, 7.5, 9.3 }, { 2, 0, 0.0, 8.0, 9.3 },
	{ 2, 1, 2.9, 7.5, 9.3 }, { 2, 0, 0.0, 8.0, 9.3 }, { 2, 1, 2.9, 7.5, 9.3 }, { 2, 0, 0.0, 8.0, 9.3 },
	{ 2, 1, 2.9, 7.5, 9.3 }, { 2, 0, 0.0, 8.0, 9.3 }, { 2, 1, 2.9, 7.5, 9.3 }, { 2, 0, 0.0, 10.5, 13.4 } };
/* 128 players at repeat count 1, scored as if 0: about a third of the late frames fit (192 of 600) */
static const struct frame_kind forced_45_128[] = { { 2, 1, 6.8, 9.0, 12.0 }, { 2, 0, 0.0, 15.4, 16.3 },
	{ 2, 0, 0.0, 8.0, 9.0 } };

static void trigger_cadences(void)
{
	struct stereo_cadence_trigger trigger = { 0 }, asked = { 0 };
	double now = 0.0, fired_at;
	unsigned long started = 0;
	int events;

	stereo_cadence_trigger_reset(&trigger, 0, now);
	events = run(&trigger, &now, 120.0, clean_90, 3, 1, 0, NULL, NULL);
	check(!(events & STEREO_CADENCE_WOULD_DROP) && trigger.counted > 100, "90 Hz at 48 players: would drop");
	stereo_cadence_trigger_reset(&trigger, 0, now);
	events = run(&trigger, &now, 120.0, drifting_64, 9, 1, 0, NULL, NULL);
	check(!(events & STEREO_CADENCE_WOULD_DROP), "64 players drifting: would drop");
	/* the lock begins ten seconds into the map: would drop within ten seconds of it */
	stereo_cadence_trigger_reset(&trigger, 0, now);
	run(&trigger, &now, 10.0, clean_90, 3, 1, 0, NULL, NULL);
	{
		double lock_began = now;

		events = run(&trigger, &now, 30.0, lock_96, 2, 1, 0, &fired_at, &started);
		check((events & STEREO_CADENCE_WOULD_DROP) && fired_at - lock_began < 10.0, "96-player lock: no would drop in 10 s");
		check(started == 1, "96-player lock: one streak start");
	}
	stereo_cadence_trigger_reset(&trigger, 0, now);
	run(&trigger, &now, 10.0, clean_90, 3, 1, 0, NULL, NULL);
	{
		double heavy_began = now;

		events = run(&trigger, &now, 30.0, heavy_128, 9, 1, 0, &fired_at, NULL);
		check((events & STEREO_CADENCE_WOULD_DROP) && fired_at - heavy_began < 10.0, "128 players: no would drop in 10 s");
	}
	/* repeat count 1 forced, scored as if 0 (the stand-in for the system's own 45 Hz): the system share skips the
	windows; scored at the asked count nothing is late */
	stereo_cadence_report_reset(&trigger);
	stereo_cadence_trigger_reset(&trigger, 0, now);
	stereo_cadence_trigger_reset(&asked, 1, now);
	events = run(&trigger, &now, 120.0, forced_45_64, 10, 1, 0, NULL, NULL);
	check(!(events & STEREO_CADENCE_WOULD_DROP), "forced 45 at 64 players scored as 0: would drop");
	check(trigger.skipped[STEREO_CADENCE_SKIP_SYSTEM] > 100, "forced 45 at 64 players: the system share skipped no window");
	now -= 120.0;
	events = run(&asked, &now, 120.0, forced_45_64, 10, 1, 0, NULL, NULL);
	check(!(events & STEREO_CADENCE_WOULD_DROP) && asked.counted > 100, "forced 45 scored at 1: would drop");
	stereo_cadence_trigger_reset(&trigger, 0, now);
	events = run(&trigger, &now, 120.0, forced_45_128, 3, 1, 0, NULL, NULL);
	check(!(events & STEREO_CADENCE_WOULD_DROP), "forced 45 at 128 players scored as 0: would drop");
}

/* skipped windows neither extend nor break a streak; a counted window that misses breaks it; would drop fires once
a map */
static void trigger_skips_and_streaks(void)
{
	struct stereo_cadence_trigger trigger = { 0 };
	double now = 100.0, fired_at;
	enum stereo_cadence_skip skip;
	int events;

	/* the first five seconds after a map loads, and before a measured period */
	stereo_cadence_trigger_reset(&trigger, 0, now);
	events = run(&trigger, &now, 4.5, lock_96, 2, 1, 0, NULL, NULL);
	check(!events && trigger.skipped[STEREO_CADENCE_SKIP_SETTLING] >= 4 && trigger.counted == 0, "settling counted");
	stereo_cadence_trigger_reset(&trigger, 0, now);
	now += 10.0;
	events = run(&trigger, &now, 30.0, lock_96, 2, 0, 0, NULL, NULL);
	check(!events && trigger.skipped[STEREO_CADENCE_SKIP_PERIOD] >= 20, "no measured period counted");
	events = run(&trigger, &now, 30.0, lock_96, 2, 1, 1, NULL, NULL);
	check(!events && trigger.skipped[STEREO_CADENCE_SKIP_THERMAL] >= 20, "warm headset counted");
	/* two hits, a hitch, a cool skip, then a third hit: fires */
	stereo_cadence_report_reset(&trigger);
	stereo_cadence_trigger_reset(&trigger, 0, now - 10.0);
	run(&trigger, &now, 2.0, lock_96, 2, 1, 0, NULL, NULL);
	check(trigger.tick_streak == 2 && trigger.counted == 2, "two lock windows: streak of two");
	{
		static const struct frame_kind hitch[] = { { 5, 0, 0.0, 6.0, 7.4 }, { 1, 0, 0.0, 6.0, 7.4 } };
		struct frame_kind frame;
		int index = 0;

		do
			frame = hitch[index ? 1 : 0], index++;
		while (!stereo_cadence_window_add(&trigger, now += frame.refreshes * PERIOD, frame.ticked, frame.refreshes,
			frame.tick, frame.previous_render, frame.previous_gpu, PERIOD));
		events = stereo_cadence_decide(&trigger, 1, 0, &skip, NULL);
		check(!events && skip == STEREO_CADENCE_SKIP_HITCH && trigger.tick_streak == 2, "a hitch broke or extended the streak");
	}
	events = run(&trigger, &now, 1.0, lock_96, 2, 1, 0, &fired_at, NULL);
	check((events & STEREO_CADENCE_WOULD_DROP) && trigger.fired, "streak across skipped windows: no would drop");
	events = run(&trigger, &now, 20.0, lock_96, 2, 1, 0, NULL, NULL);
	check(!(events & STEREO_CADENCE_WOULD_DROP) && trigger.longest > STEREO_CADENCE_STREAK, "would drop fired twice in a map");
	/* a counted window that misses breaks the streak */
	stereo_cadence_trigger_reset(&trigger, 0, now - 10.0);
	run(&trigger, &now, 2.0, lock_96, 2, 1, 0, NULL, NULL);
	run(&trigger, &now, 1.0, clean_90, 3, 1, 0, NULL, NULL);
	check(trigger.tick_streak == 0, "a clean window kept the streak");
	events = run(&trigger, &now, 2.0, lock_96, 2, 1, 0, NULL, NULL);
	check(!(events & STEREO_CADENCE_WOULD_DROP), "a broken streak still fired");
	/* a new map: fires again */
	stereo_cadence_trigger_reset(&trigger, 0, now - 10.0);
	events = run(&trigger, &now, 4.0, lock_96, 2, 1, 0, NULL, NULL);
	check(events & STEREO_CADENCE_WOULD_DROP, "a new map did not fire again");
}

int main(void)
{
	repeat_wanted();
	lateness_matches_the_old_line();
	lateness_at_the_asked_count();
	trigger_cadences();
	trigger_skips_and_streaks();
	if (!failures)
		printf("stereo cadence probe: ok\n");
	return failures ? 1 : 0;
}
