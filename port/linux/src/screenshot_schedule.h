/* screenshot_schedule.h: which presented frames debug.screenshot_every saves, and the name each one is saved under

debug.screenshot_clock = "frames" (the default) counts presented frames from launch and names a shot frameNNNNN.
How many frames a launch presents before the game starts (menus, loading, a network game's setup) depends on how
long those took, so under debug.fixed_timestep frame N of one launch can show a different tick than frame N of the
next, although every tick repeats exactly. "ticks" counts the game's ticks instead: a frame is saved when the tick it
shows is a multiple of screenshot_every and was not saved already, and it is named tickNNNNN, so two launches save
the same ticks under the same names. Frames outside a game (the menus, loading) are not saved in "ticks". When the
tick goes back (a new map, a map restart, a checkpoint revert) the schedule logs it and names the shots after it
tickNNNNN-restartN, so they never overwrite the earlier ones. */

#ifndef HALO_SCREENSHOT_SCHEDULE_H
#define HALO_SCREENSHOT_SCHEDULE_H

#include <stdio.h>
#include <string.h>

#define SCREENSHOT_NAME_SIZE 48

enum screenshot_clock
{
	SCREENSHOT_CLOCK_FRAMES,
	SCREENSHOT_CLOCK_TICKS,
};

typedef void (*screenshot_log_proc)(const char *format, ...);

struct screenshot_schedule
{
	int ready;
	enum screenshot_clock clock;
	long every;
	long last_saved; /* the tick last saved under "ticks"; -1 none since the last restart */
	long last_shown; /* the tick the last in-game frame showed; -1 none yet */
	long restarts;
	screenshot_log_proc log;
	char name[SCREENSHOT_NAME_SIZE]; /* the shot due: its file name without the suffix and ".png" */
};

/* the name of a shot taken outside the schedule (a window or cutscene capture), by presented frame */
static inline void screenshot_name_frame(char *name, size_t size, unsigned long frame)
{
	snprintf(name, size, "frame%05lu", frame);
}

/* clock: debug.screenshot_clock's text, "frames" (or empty) or "ticks"; anything else is logged and counts frames */
static inline void screenshot_schedule_init(struct screenshot_schedule *schedule, const char *clock, long every,
	screenshot_log_proc log)
{
	memset(schedule, 0, sizeof(*schedule));
	schedule->ready = 1;
	schedule->clock = SCREENSHOT_CLOCK_FRAMES;
	if (clock && strcmp(clock, "ticks") == 0)
		schedule->clock = SCREENSHOT_CLOCK_TICKS;
	else if (clock && *clock && strcmp(clock, "frames") != 0)
		log("debug.screenshot_clock \"%s\" is not \"frames\" or \"ticks\": counting frames", clock);
	schedule->every = every;
	schedule->last_saved = -1;
	schedule->last_shown = -1;
	schedule->log = log;
}

/* whether this presented frame is saved; if so, *number is the frame or tick it is counted by (what
debug.screenshot_frames selects) and schedule->name the name to save it under. shown_tick gives the game tick the
frame shows, -1 outside a game; it is called only when the "ticks" clock needs it, so a build that saves no shots
never asks the game for its time */
static inline int screenshot_schedule_due(struct screenshot_schedule *schedule, unsigned long frame,
	long (*shown_tick)(void), unsigned long *number)
{
	long tick;

	if (schedule->every <= 0)
		return 0;
	if (schedule->clock == SCREENSHOT_CLOCK_FRAMES)
	{
		if (frame % (unsigned long)schedule->every != 0)
			return 0;
		*number = frame;
		screenshot_name_frame(schedule->name, sizeof(schedule->name), frame);
		return 1;
	}
	tick = shown_tick();
	if (tick < 0)
		return 0;
	if (schedule->last_shown >= 0 && tick < schedule->last_shown)
	{
		schedule->restarts++;
		schedule->last_saved = -1;
		schedule->log("screenshots: the game's tick went back from %ld to %ld; tick shots are now named "
			"tickNNNNN-restart%ld", schedule->last_shown, tick, schedule->restarts);
	}
	schedule->last_shown = tick;
	if (tick == schedule->last_saved || tick % schedule->every != 0)
		return 0;
	schedule->last_saved = tick;
	*number = (unsigned long)tick;
	if (schedule->restarts)
		snprintf(schedule->name, sizeof(schedule->name), "tick%05ld-restart%ld", tick, schedule->restarts);
	else
		snprintf(schedule->name, sizeof(schedule->name), "tick%05ld", tick);
	return 1;
}

#endif
