/* screenshot_schedule.h: which presented frames debug.screenshot_every saves, and the number in their names

debug.screenshot_clock = "frames" (the default) counts presented frames from launch. How many frames a launch
presents before the game starts (menus, loading, a network game's setup) depends on how long those took, so under
debug.fixed_timestep frame N of one launch can show a different tick than frame N of the next, although every tick
repeats exactly. "ticks" counts the game's ticks instead: a frame is saved when the tick it shows is a multiple of
screenshot_every and was not saved already, and it is named by that tick, so two launches save the same ticks under
the same names. Frames outside a game (the menus, loading) are not saved in "ticks". */

#ifndef HALO_SCREENSHOT_SCHEDULE_H
#define HALO_SCREENSHOT_SCHEDULE_H

enum screenshot_clock
{
	SCREENSHOT_CLOCK_FRAMES,
	SCREENSHOT_CLOCK_TICKS,
};

struct screenshot_schedule
{
	enum screenshot_clock clock;
	long every;
	long last_tick; /* the tick last saved under "ticks"; -1 none */
};

static inline void screenshot_schedule_init(struct screenshot_schedule *schedule, enum screenshot_clock clock, long every)
{
	schedule->clock = clock;
	schedule->every = every;
	schedule->last_tick = -1;
}

/* whether this presented frame is saved; *number is the number its file is named by (frame or tick). shown_tick
gives the game tick the frame shows, -1 outside a game; it is called only when the "ticks" clock needs it, so a
build that saves no shots never asks the game for its time */
static inline int screenshot_schedule_due(struct screenshot_schedule *schedule, unsigned long frame,
	long (*shown_tick)(void), unsigned long *number)
{
	long tick;

	if (schedule->every <= 0)
		return 0;
	if (schedule->clock == SCREENSHOT_CLOCK_FRAMES)
	{
		*number = frame;
		return frame % (unsigned long)schedule->every == 0;
	}
	tick = shown_tick();
	if (tick < 0 || tick == schedule->last_tick || tick % schedule->every != 0)
		return 0;
	schedule->last_tick = tick;
	*number = (unsigned long)tick;
	return 1;
}

#endif
