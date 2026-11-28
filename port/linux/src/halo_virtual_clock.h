/*
HALO_VIRTUAL_CLOCK.H

The clock of debug.fixed_timestep (xbox_kernel.c): time that advances 1/30 s
for each presented frame, however long the frame really took, so two runs
show the same game time on the same frame. Pure arithmetic on the frame
count, tested natively (port/ios/tests/virtual_clock_probe.c).

The performance counter counts frames, HALO_VIRTUAL_CLOCK_RATE per second,
so the game divides one count by the frequency and gets exactly 1/30 s: one
30 Hz tick per frame, with no remainder for interpolation. Millisecond
arithmetic is 64-bit: on the 32-bit guest, unsigned long frames times 1000
overflows after about 40 hours.
*/

#ifndef __HALO_VIRTUAL_CLOCK_H
#define __HALO_VIRTUAL_CLOCK_H

/* frames per second of game time: the game's tick rate, so each frame is one tick */
#define HALO_VIRTUAL_CLOCK_RATE 30

/* where the clocks start: nonzero, as a real clock's are */
#define HALO_VIRTUAL_CLOCK_START_MILLISECONDS 1000000ULL
#define HALO_VIRTUAL_CLOCK_START_SECONDS 1000000000ULL /* 2001-09-09 01:46:40 UTC */

/* QueryPerformanceCounter, at HALO_VIRTUAL_CLOCK_RATE counts per second */
static inline unsigned long long halo_virtual_clock_counter(unsigned long frames)
{
	return HALO_VIRTUAL_CLOCK_START_MILLISECONDS / 1000ULL * HALO_VIRTUAL_CLOCK_RATE + frames;
}

/* GetTickCount */
static inline unsigned long halo_virtual_clock_milliseconds(unsigned long frames)
{
	return (unsigned long)(HALO_VIRTUAL_CLOCK_START_MILLISECONDS +
		(unsigned long long)frames * 1000ULL / HALO_VIRTUAL_CLOCK_RATE);
}

/* time(): Unix time */
static inline unsigned long long halo_virtual_clock_seconds(unsigned long frames)
{
	return HALO_VIRTUAL_CLOCK_START_SECONDS + (unsigned long long)frames / HALO_VIRTUAL_CLOCK_RATE;
}

/* the frame debug.exit_after ends the game on; 0 never */
static inline unsigned long halo_virtual_clock_exit_frame(double seconds)
{
	unsigned long frame;

	if (seconds <= 0.0)
		return 0;
	frame = (unsigned long)(seconds * HALO_VIRTUAL_CLOCK_RATE + 0.5);
	return frame ? frame : 1;
}

/* debug.exit_after's state, as the event thread's pump sees it. The deadline of each clock is pushed out by what the scripted input
waited for its first screen (`frame_delay` frames, `tick_delay` milliseconds); a deadline of 0 is never. */
struct halo_exit_state
{
	int fixed_timestep;               /* debug.fixed_timestep: the frame deadline decides, else the wall-clock one */
	unsigned long frames;             /* presented frames so far */
	unsigned long exit_frame, frame_delay;
	unsigned long long ticks, exit_ticks, tick_delay;   /* milliseconds */
	unsigned long script_began_next;  /* the frame the scripted input's clock began at, plus 1 (0 before it began, so frame 0 counts) */
	unsigned long span;               /* debug.exit_after in frames (30 a second); 0 never */
};

/* whether debug.exit_after has passed: the clock's own deadline (as before), and, once a scripted input's clock has begun, never
before `span` frames after that frame either. A script's presses are marked in frames, and a slow host (a simulator at 7 frames a
second) reached only part of them by the wall-clock deadline; a fast host (60 or 120 frames a second) reaches the frame budget well
before its wall deadline and keeps its old length. The script only ever extends a run. */
static inline int halo_exit_reached(const struct halo_exit_state *s)
{
	int reached = s->fixed_timestep ? s->exit_frame && s->frames >= s->exit_frame + s->frame_delay :
		s->exit_ticks && s->ticks >= s->exit_ticks + s->tick_delay;

	if (reached && s->script_began_next && s->span)
		return s->frames >= s->script_began_next - 1 + s->span;
	return reached;
}

#endif
