#ifndef FRAME_WAITS_H
#define FRAME_WAITS_H

/* A frame's waits, which are not the frame's work, in three parts by thread and meaning (the Metal backend's; seconds
on one clock, CACurrentMediaTime there; port/ios/tests/frame_waits_probe.c):
- game: the game thread's (the late start's sleep, the stream ring's producer_wait);
- display: inside the present (the drawable, frame_context_wait): the timeline row's display wait, the part of the
  encode thread's present barrier that is not encoding;
- slot: frame_begin's wait for a frame slot, on the thread that encodes.
With the encode thread (debug.encode_thread = "thread") the present takes them while the game thread waits at the
present barrier, and the game thread adds to game only outside it, so no two threads touch one part at once. */

struct frame_waits_taken
{
	/* what leaves the frame's CPU time: every part, but with the encode thread only the slot wait after the game
	thread reached its present (before that it worked on) */
	double cpu;
	/* the timeline row's display wait: display and the counted slot wait (not the late start, which comes before the
	controller read) */
	double display;
};

void frame_waits_add_game(double seconds);
void frame_waits_add_display(double seconds);
/* (one slot wait a frame: frame_begin waits once, so the last interval is the frame's) */
void frame_waits_add_slot(double started, double ended);
/* the frame's waits, cleared for the next. Every flat present takes them, shown or not, so a frame's waits never land
on the next frame's row (the visionOS stereo, theater and UI-quad presents do not yet: the visionOS encode step). present_called: when the game thread reached its present with the encode thread on
(host_gpu_present_called); a negative value means the game thread encoded the frame itself. */
struct frame_waits_taken frame_waits_take(double present_called);

#endif
