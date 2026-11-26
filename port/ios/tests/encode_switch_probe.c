/* encode_switch.h: when a change of workload context switches the encode thread. Built by tools/ios_test.py. */
#include "encode_switch.h"

#include <stdio.h>

static int checks, failures;
#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    fprintf(stderr, "encode_switch_probe: line %d: %s\n", __LINE__, #condition); } } while (0)

int main(void)
{
	/* (metal, have_unwrapped, debug_set, scene_thread, queue_possible, on_main_thread, thread_on) */
	/* a variable workload starts the thread; a predictable one stops it; the same wish changes nothing */
	CHECK(encode_switch_wanted(1, 1, 0, 1, 1, 0, 0) == 1);
	CHECK(encode_switch_wanted(1, 1, 0, 0, 1, 0, 1) == 1);
	CHECK(encode_switch_wanted(1, 1, 0, 1, 1, 0, 1) == 0);
	CHECK(encode_switch_wanted(1, 1, 0, 0, 1, 0, 0) == 0);
	/* the game has not said yet */
	CHECK(encode_switch_wanted(1, 1, 0, -1, 1, 0, 0) == 0);
	CHECK(encode_switch_wanted(1, 1, 0, -1, 1, 0, 1) == 0);
	/* debug.encode_thread decided the mode */
	CHECK(encode_switch_wanted(1, 1, 1, 1, 1, 0, 0) == 0);
	CHECK(encode_switch_wanted(1, 1, 1, 0, 1, 0, 1) == 0);
	/* a thread the host cannot deliver is no change: no set in the ring or no queue on this build, or the main thread */
	CHECK(encode_switch_wanted(1, 1, 0, 1, 0, 0, 0) == 0);
	CHECK(encode_switch_wanted(1, 1, 0, 1, 1, 1, 0) == 0);
	/* (off is always deliverable: a thread running where it cannot is stopped) */
	CHECK(encode_switch_wanted(1, 1, 0, 0, 0, 0, 1) == 1);
	CHECK(encode_switch_wanted(1, 1, 0, 0, 1, 1, 1) == 1);
	/* OpenGL ES, or before the backend was wrapped */
	CHECK(encode_switch_wanted(0, 1, 0, 1, 1, 0, 0) == 0);
	CHECK(encode_switch_wanted(1, 0, 0, 1, 1, 0, 0) == 0);
	if (failures)
	{
		fprintf(stderr, "encode_switch_probe: %d of %d checks failed\n", failures, checks);
		return 1;
	}
	printf("encode_switch_probe: %d checks passed\n", checks);
	return 0;
}
