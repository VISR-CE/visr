/* Waiting for another thread, a callback or a file in a native test program. A test never waits a short fixed time
for something on another thread: a loaded job host starves that thread for longer than any number picked on an idle
machine (save_writer_probe failed "save_writer_wait(10)" under a long build line). Every wait is a wall-clock wait
on a monotonic clock with one generous cap: it returns the moment the condition holds, and the cap only bounds a
real hang. A passing run does not get slower. tools/test_probe_waits.py refuses the counted sleep-poll loops, short
dispatch timeouts and short alarms that this header replaces. A wait that is meant to be short (a promptness check,
or "nothing more arrives") marks its line with TEST_WAIT_SHORT_ON_PURPOSE and says why. */
#ifndef TEST_WAIT_H
#define TEST_WAIT_H

#include <time.h>
#include <unistd.h>

#define TEST_WAIT_CAP_SECONDS 120.0        /* the default cap on any wait for another thread */
#define TEST_WAIT_WATCHDOG_SECONDS 300     /* a whole-program alarm: a hang guard, not a measurement */
#define TEST_WAIT_SHORT_ON_PURPOSE         /* marks a line that is a deliberate promptness or absence check */

/* The dispatch semaphore and group timeout for a wait with the default cap (needs <dispatch/dispatch.h>). */
#define TEST_WAIT_SEMAPHORE_TIMEOUT dispatch_time(DISPATCH_TIME_NOW, (int64_t)(TEST_WAIT_CAP_SECONDS * 1e9))

static inline double test_wait_now(void)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

/* 1 when the condition held before `seconds` passed, 0 on the cap. The condition is evaluated again after every
1 ms sleep, so it may read a flag another thread sets or take a report from a queue. */
#define TEST_WAIT_UNTIL_FOR(condition, seconds) ({ \
	double test_wait_end_ = test_wait_now() + (seconds); \
	int test_wait_held_ = 0; \
	for (;;) \
	{ \
		if (condition) { test_wait_held_ = 1; break; } \
		if (test_wait_now() >= test_wait_end_) break; \
		usleep(1000); \
	} \
	test_wait_held_; })

#define TEST_WAIT_UNTIL(condition) TEST_WAIT_UNTIL_FOR(condition, TEST_WAIT_CAP_SECONDS)

#endif
