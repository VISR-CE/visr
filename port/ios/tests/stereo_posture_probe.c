/* host_stereo_posture.c: the posture estimate the headset logs at Enter VR
and a recenter (phase 0 of the comfort and aim design). Prints PASS or the
failures and exits nonzero. */
#include "host_stereo_posture.h"

#include <math.h>
#include <stdio.h>

static int failures;

static void check(int condition, const char *what)
{
	if (!condition)
	{
		printf("FAIL: %s\n", what);
		failures++;
	}
}

/* feeds height(t) at 90 Hz from start to end; returns the last event's decided */
static int feed(struct host_stereo_posture *posture, double start, double end, float (*height)(double), float pitch,
	struct host_stereo_posture_event *last)
{
	int decided = 0;
	double t;

	for (t = start; t < end; t += 1.0 / 90.0)
	{
		struct host_stereo_posture_event event;

		if (host_stereo_posture_sample(posture, t, height(t), pitch, &event))
		{
			decided = event.decided;
			*last = event;
		}
	}
	return decided;
}

static float seated(double t) { return 1.15f + 0.02f * (float)sin(t * 3.0); }
static float standing(double t) { return 1.62f + 0.02f * (float)sin(t * 3.0); }
static float no_floor(double t) { (void)t; return 0.05f; }
static float near_threshold(double t) { (void)t; return POSTURE_STANDING_HEIGHT + 0.5f * POSTURE_HYSTERESIS; }
/* stands up at 10 s */
static float stands_up(double t) { return t < 10.0 ? seated(t) : standing(t); }
/* bobs over the threshold for a second at a time */
static float bobbing(double t) { return fmod(t, 2.0) < 1.0 ? seated(t) : standing(t); }

int main(void)
{
	struct host_stereo_posture posture;
	struct host_stereo_posture_event event = { 0 };

	host_stereo_posture_start(&posture, 0.0);
	check(feed(&posture, 0.0, 1.9, seated, 0.0f, &event) == 0, "no estimate before the window ends");
	check(feed(&posture, 1.9, 2.1, seated, 0.0f, &event) == 1, "the first estimate at the window's end");
	check(event.posture == HOST_POSTURE_SEATED, "a seated head reads seated");
	check(fabsf(event.height - 1.15f) < 0.03f, "the median is the seated height");

	host_stereo_posture_start(&posture, 0.0);
	feed(&posture, 0.0, 2.1, standing, 0.0f, &event);
	check(event.posture == HOST_POSTURE_STANDING, "a standing head reads standing");

	host_stereo_posture_start(&posture, 0.0);
	feed(&posture, 0.0, 2.1, no_floor, 0.0f, &event);
	check(event.posture == HOST_POSTURE_NO_FLOOR, "a height near the origin is no floor reference");

	/* looking down at the floor is left out of the window */
	host_stereo_posture_start(&posture, 0.0);
	feed(&posture, 0.0, 1.0, standing, -70.0f, &event);
	feed(&posture, 1.0, 2.1, standing, 0.0f, &event);
	check(event.decided == 1 && event.skipped > 80 && event.posture == HOST_POSTURE_STANDING,
		"samples pitched far down are skipped");

	/* standing up mid-game is a change after the hold, and only once */
	host_stereo_posture_start(&posture, 0.0);
	feed(&posture, 0.0, 2.1, stands_up, 0.0f, &event);
	check(event.posture == HOST_POSTURE_SEATED, "seated before standing up");
	check(feed(&posture, 2.1, 12.9, stands_up, 0.0f, &event) == 0, "no change before the hold ends");
	check(feed(&posture, 12.9, 13.2, stands_up, 0.0f, &event) == 2 && event.posture == HOST_POSTURE_STANDING,
		"standing up is a change once it holds");
	check(feed(&posture, 13.2, 30.0, stands_up, 0.0f, &event) == 0, "one change for one stand");

	/* a height inside the hysteresis band never flips a standing decision */
	host_stereo_posture_start(&posture, 0.0);
	feed(&posture, 0.0, 2.1, standing, 0.0f, &event);
	check(feed(&posture, 2.1, 20.0, near_threshold, 0.0f, &event) == 0, "hysteresis holds near the threshold");

	/* bobbing across the threshold for less than the hold never changes it */
	host_stereo_posture_start(&posture, 0.0);
	feed(&posture, 0.0, 2.1, seated, 0.0f, &event);
	check(feed(&posture, 2.1, 30.0, bobbing, 0.0f, &event) == 0, "short crossings are no change");

	if (failures)
		return 1;
	printf("PASS: stereo posture\n");
	return 0;
}
