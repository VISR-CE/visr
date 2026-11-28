/* Waiting for another thread, a callback or a file in a native test program. A test never waits a short fixed time
for something on another thread: a loaded job host starves that thread for longer than any number picked on an idle
machine (save_writer_probe failed "save_writer_wait(10)" under a long build line). Every wait is a wall-clock wait
on a monotonic clock with one generous cap: it returns the moment the condition holds, and the cap only bounds a
real hang. A passing run does not get slower. tools/test_probe_waits.py refuses the counted sleep-poll loops, short
dispatch timeouts, short CFAbsoluteTime deadlines and short alarms that this header replaces.

A line that must be short marks itself with TEST_WAIT_SHORT_ON_PURPOSE and says which of three cases it is:
  - a promptness check: the product is required to answer quickly, and a slow answer is the failure;
  - an absence check: nothing more is meant to happen, so a pause is the only way to show it;
  - nothing reports it: the event has no signal a test can observe (a kernel registration, say), so a pause is all
    there is. Before using this one, look for something observable: a file, a counter, a retry that succeeds once
    the other side is ready.
No fixed pause, marked or not, lasts half a second or more (TEST_WAIT_PAUSE_LIMIT_SECONDS in tools/test_probe_waits.py).
A longer one is a wait for the product's own timer with a fixed margin, and uses TEST_WAIT_PUMPING_FOR_TIMER below. */
#ifndef TEST_WAIT_H
#define TEST_WAIT_H

/* nanosleep and clock_gettime are POSIX, and glibc hides them under -std=c11 (the strict build Tooling's Linux job
uses; a Mac's libc does not hide them, so only that job finds out). _POSIX_C_SOURCE 200809L asks for exactly the POSIX
names this header uses and nothing more. A feature macro only counts before the first system header, so a probe
includes this header first (tools/test_probe_waits.py checks it). It is defined on Linux only: on Apple the same macro
HIDES the Darwin extensions (mkdtemp, pthread_setname_np, pthread_set_qos_class_self_np) that other headers and probes
use, and Apple's libc shows nanosleep and clock_gettime without it. tools/test_probe_waits.py refuses an unguarded
feature-test macro in a shared test header. */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE) && !defined(_GNU_SOURCE) && !defined(_DEFAULT_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <time.h>

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
		nanosleep(&(struct timespec){ 0, 1000000 }, NULL);   /* 1 ms between two looks */ \
	} \
	test_wait_held_; })

#define TEST_WAIT_UNTIL(condition) TEST_WAIT_UNTIL_FOR(condition, TEST_WAIT_CAP_SECONDS)

/* The same wait for a probe that must keep the main run loop turning while it waits (UIKit animations, timers and
the callbacks being waited for run on it); needs <CoreFoundation/CoreFoundation.h>. Each turn lasts at most 10 ms. */
#define TEST_WAIT_PUMPING_UNTIL_FOR(condition, seconds) ({ \
	double test_wait_end_ = test_wait_now() + (seconds); \
	int test_wait_held_ = 0; \
	for (;;) \
	{ \
		if (condition) { test_wait_held_ = 1; break; } \
		if (test_wait_now() >= test_wait_end_) break; \
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true); \
	} \
	test_wait_held_; })

#define TEST_WAIT_PUMPING_UNTIL(condition) TEST_WAIT_PUMPING_UNTIL_FOR(condition, TEST_WAIT_CAP_SECONDS)

/* A wait for the product's own wall-clock timer (a timeout, a delay, a debounce) that keeps the main run loop
turning. Never wait the timer's length plus a small margin and then check: a loaded host runs the timer late by more
than any margin (benchmark_menu_probe waited 2.1 s for a 2 s timeout and failed). This polls until `condition` holds,
with the default cap, and returns the seconds since `started` (a test_wait_now() taken before the code under test
armed the timer), or -1 on the cap. The caller checks the result is at least the timer's length, less a few
milliseconds for the two clocks, so a timer that fires early still fails. */
#define TEST_WAIT_PUMPING_FOR_TIMER(condition, started) ({ \
	double test_wait_started_ = (started); \
	TEST_WAIT_PUMPING_UNTIL(condition) ? test_wait_now() - test_wait_started_ : -1.0; })

/* The cap on a wait for a child process. It stays under the default cap, which in turn stays under
tools/probe_timeouts.py's PROBE_RUN_SECONDS, so on a hang the probe reaches it first and terminates the child, instead
of the wrapper killing the probe and leaving the child running. */
#define TEST_WAIT_CHILD_CAP_SECONDS 60.0
#ifndef __cplusplus
_Static_assert((int)TEST_WAIT_CHILD_CAP_SECONDS < (int)TEST_WAIT_CAP_SECONDS, "a child's cap stays under the default cap");
#endif

#endif
