/*
A frame's waits (port/ios/host/frame_waits.c): what leaves the frame's CPU time and what is the timeline row's display
wait, with the game thread encoding the frame itself and with the encode thread, where a frame-slot wait counts only
after the game thread reached its present (before that it worked on). Times in seconds.
*/

#include "frame_waits.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition, ...) do { if (!(condition)) { fprintf(stderr, "frame_waits_probe: " __VA_ARGS__); \
	fprintf(stderr, "\n"); exit(1); } } while (0)

static void expect(const char *name, struct frame_waits_taken taken, double cpu, double display)
{
	CHECK(fabs(taken.cpu - cpu) < 1e-9 && fabs(taken.display - display) < 1e-9,
		"%s: CPU wait %.3f ms and display wait %.3f ms, expected %.3f and %.3f", name, taken.cpu * 1e3,
		taken.display * 1e3, cpu * 1e3, display * 1e3);
}

int main(void)
{
	/* encode thread: the late start's 5 ms and the present's 4 ms for the display leave the CPU time; the late start is
	not in the row's display wait (the barrier's 6 ms less those 4 leaves an encoder tail of 2, host_frame_timeline) */
	frame_waits_add_game(0.005);
	frame_waits_add_display(0.004);
	/* (the display wait read before the take is the take's, and stays in place) */
	CHECK(fabs(frame_waits_display(10.0) - 0.004) < 1e-9 && fabs(frame_waits_display(10.0) - 0.004) < 1e-9,
		"display wait before the take: %.3f ms, expected 4", frame_waits_display(10.0) * 1e3);
	expect("late start and display", frame_waits_take(10.0), 0.009, 0.004);

	/* the game thread encodes the frame: its whole 3 ms slot wait is a wait */
	frame_waits_add_slot(20.000, 20.003);
	expect("slot wait without the encode thread", frame_waits_take(-1.0), 0.003, 0.003);

	/* encode thread: a slot wait wholly before the game thread reached its present was the game thread's work time */
	frame_waits_add_slot(30.000, 30.003);
	expect("slot wait before the present", frame_waits_take(30.010), 0.0, 0.0);

	/* encode thread: a slot wait that runs into the barrier counts from the present on (3 of its 5 ms) */
	frame_waits_add_game(0.001);
	frame_waits_add_slot(40.000, 40.005);
	expect("slot wait into the barrier", frame_waits_take(40.002), 0.004, 0.003);

	/* every take clears: a frame not shown (held for compiles) takes its 16 ms display wait, and the next frame's row
	gets only its own */
	frame_waits_add_display(0.016);
	expect("held frame", frame_waits_take(50.0), 0.016, 0.016);
	frame_waits_add_display(0.002);
	expect("frame after the held one", frame_waits_take(50.020), 0.002, 0.002);
	expect("no waits", frame_waits_take(60.0), 0.0, 0.0);
	expect("no waits, no encode thread", frame_waits_take(-1.0), 0.0, 0.0);
	return 0;
}
