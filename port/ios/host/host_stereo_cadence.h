/* SPDX-License-Identifier: Apache-2.0 */
/* The headset's frame cadence, as pure functions the host and its probe share (port/ios/tests/stereo_cadence_probe.c).

The Compositor's frame repeat count: how many extra refreshes each frame stays up (0: every refresh, 90 Hz; 1: 45
Hz). One function decides it, and every writer (the space opening, debug.knob_cycle's cadence) goes through it
(host_stereo_repeat_refresh, host_stereo.m). Whether a frame was late, and whether its own work explains it, is
decided here too, for the cadence line in gpu_metal.m. */
#pragma once
#include <math.h>
#include <stddef.h>
#include "halo_stereo.h"

/* The repeat count for stereo MODE: 0 outside HEAD and SCREEN modes; FORCED (display.frame_repeat, clamped to 0..3)
when it is not 0; else 1 when LATCHED (a lock to steady 45 Hz, HEAD mode only); else 0. A forced count wins over the
lock, as the cadence session and the benchmarks need */
static inline int stereo_repeat_wanted(int mode, int forced, int latched)
{
	if (mode != HALO_STEREO_HEAD && mode != HALO_STEREO_SCREEN)
		return 0;
	if (forced > 0)
		return forced > 3 ? 3 : forced;
	return latched && mode == HALO_STEREO_HEAD ? 1 : 0;
}

/* ---------- lateness

A frame's presentation is predicted when it opens, after its tick and the previous frame's render and GPU work, so
those are what can make it late. The cadence line ("stereo: cadence", gpu_metal.m) and the trigger below share these
rules; neither keeps its own. Lateness is measured against the repeat count that was asked for: at 1 a frame that
arrives two refreshes after the previous one is on time. */

/* refreshes since the previous presentation, from the interval between them: 1 to MOST */
static inline long stereo_cadence_refreshes(double interval, double period, long most)
{
	long up = lround(interval / period);

	return up < 1 ? 1 : up > most ? most : up;
}

/* a frame arriving REFRESHES after the previous one is late at repeat count REPEAT */
static inline int stereo_cadence_late(long refreshes, int repeat)
{
	return refreshes > (long)repeat + 1;
}

/* the time one frame has at repeat count REPEAT */
static inline double stereo_cadence_slot(double period, int repeat)
{
	return period * (double)(repeat + 1);
}

/* the frame's own work explains its lateness: its TICK plus the PREVIOUS_RENDER (the previous frame's CPU time less
its tick), or the PREVIOUS_GPU time, ran past the SLOT (seconds, or any one unit). A late frame whose own work fit
was paced by the system */
static inline int stereo_cadence_own_work(double tick, double previous_render, double previous_gpu, double slot)
{
	return tick + previous_render > slot || previous_gpu > slot;
}

enum stereo_cadence_lateness
{
	STEREO_CADENCE_ON_TIME,
	STEREO_CADENCE_LATE_OWN_WORK, /* late, and its own work explains it */
	STEREO_CADENCE_LATE_SYSTEM    /* late, but its own work fit its slot: the system paced it */
};

/* a frame REFRESHES after the previous presentation at repeat count REPEAT and refresh PERIOD, with its TICK and the
previous frame's render and GPU times (PERIOD's unit): the one composition of the rules above */
static inline enum stereo_cadence_lateness stereo_cadence_classify(long refreshes, int repeat, double period,
	double tick, double previous_render, double previous_gpu)
{
	if (!stereo_cadence_late(refreshes, repeat))
		return STEREO_CADENCE_ON_TIME;
	return stereo_cadence_own_work(tick, previous_render, previous_gpu, stereo_cadence_slot(period, repeat)) ?
		STEREO_CADENCE_LATE_OWN_WORK : STEREO_CADENCE_LATE_SYSTEM;
}

/* ---------- the trigger (log only)

Whether a big game has stopped holding the headset's full rate by its own work, for a lock to steady 45 Hz that is
not built (display.steady_cadence = "log": this evaluates and logs and never changes the repeat count). Fed once a
stereo frame in HEAD mode, in a multiplayer (variable) workload context, by host_stereo_cadence_frame (host_stereo.m).

A window is the frames whose refreshes add up to STEREO_CADENCE_WINDOW_REFRESHES (a second at 90 Hz). In it, T tick
frames and O other frames, Lt and Lo the late ones of each whose own work explains them, and S the late frames the
rules give the system. A window is skipped (neither extending nor breaking a streak) in the first
STEREO_CADENCE_SETTLE_SECONDS after a map loads or the space opens, before the pacing report has measured a period,
while the thermal state is above nominal or Low Power Mode is on, when a frame took STEREO_CADENCE_HITCH_REFRESHES
or more (a load or a shader hitch), and when S is a quarter or more of its late frames: at 45 Hz times rise and the
own-work rule gives the system most of them, so such a window is the system's pacing, not the game's. It would drop
when Lt >= T/2 in STEREO_CADENCE_STREAK consecutive counted windows, or Lo >= O/2 in as many. */

#define STEREO_CADENCE_WINDOW_REFRESHES 90
#define STEREO_CADENCE_SETTLE_SECONDS 5.0
#define STEREO_CADENCE_HITCH_REFRESHES 5
#define STEREO_CADENCE_STREAK 3
/* streak starts reported a pacing report at most (a game at the threshold can start one every other second); the
summary counts them all */
#define STEREO_CADENCE_STREAK_LOGS 3

enum stereo_cadence_skip
{
	STEREO_CADENCE_COUNTED,
	STEREO_CADENCE_SKIP_PERIOD,   /* no measured refresh period yet */
	STEREO_CADENCE_SKIP_SETTLING, /* a map loaded or the space opened less than five seconds before */
	STEREO_CADENCE_SKIP_THERMAL,  /* thermal state above nominal, or Low Power Mode */
	STEREO_CADENCE_SKIP_HITCH,    /* a frame of five or more refreshes */
	STEREO_CADENCE_SKIP_SYSTEM,   /* a quarter or more of the late frames were the system's */
	STEREO_CADENCE_SKIP_REASONS
};

/* what stereo_cadence_decide saw in a window */
enum
{
	STEREO_CADENCE_STREAK_STARTED = 1,
	STEREO_CADENCE_WOULD_DROP = 2 /* the first time in this map */
};

struct stereo_cadence_window
{
	double started;                  /* seconds, the window's first frame */
	long refreshes;                  /* the window's frames' refreshes so far */
	unsigned long tick_frames, other_frames;
	unsigned long tick_late, other_late; /* late by their own work */
	unsigned long system_late;           /* late, but their own work fit */
	int hitch;
};

struct stereo_cadence_trigger
{
	int repeat;                      /* the count frames are scored at */
	double settled;                  /* seconds: windows starting before this are skipped (settling) */
	struct stereo_cadence_window window;
	int tick_streak, other_streak;
	int fired;                       /* would drop fired since the map loaded */
	/* since the last pacing report */
	unsigned long counted, skipped[STEREO_CADENCE_SKIP_REASONS];
	int longest;
	int fired_in_report;
	unsigned long streaks;           /* streaks begun */
};

/* the asked count changed at NOW (seconds): frames are scored at REPEAT from a fresh window and streak, after the
settling time. Would drop stays fired for the map: a knob press does not re-arm it */
static inline void stereo_cadence_trigger_rescore(struct stereo_cadence_trigger *trigger, int repeat, double now)
{
	struct stereo_cadence_window empty = { 0 };

	trigger->repeat = repeat;
	trigger->settled = now + STEREO_CADENCE_SETTLE_SECONDS;
	trigger->window = empty;
	trigger->tick_streak = trigger->other_streak = 0;
}

/* a map loaded or the space opened at NOW: as a rescore, and would drop may fire again */
static inline void stereo_cadence_trigger_reset(struct stereo_cadence_trigger *trigger, int repeat, double now)
{
	stereo_cadence_trigger_rescore(trigger, repeat, now);
	trigger->fired = 0;
}

/* the per-report counts start again (after the summary line) */
static inline void stereo_cadence_report_reset(struct stereo_cadence_trigger *trigger)
{
	int reason;

	trigger->counted = 0;
	for (reason = 0; reason < STEREO_CADENCE_SKIP_REASONS; reason++)
		trigger->skipped[reason] = 0;
	trigger->longest = trigger->tick_streak > trigger->other_streak ? trigger->tick_streak : trigger->other_streak;
	trigger->fired_in_report = 0;
	trigger->streaks = 0;
}

/* one frame at NOW (seconds), REFRESHES after the previous presentation (stereo_cadence_refreshes, at most
STEREO_CADENCE_HITCH_REFRESHES), with its TICK time (0 without one) and the previous frame's render and GPU times
(PERIOD's unit). Returns 1 when the window is full and stereo_cadence_decide should run */
static inline int stereo_cadence_window_add(struct stereo_cadence_trigger *trigger, double now, int ticked,
	long refreshes, double tick, double previous_render, double previous_gpu, double period)
{
	struct stereo_cadence_window *window = &trigger->window;

	if (window->refreshes == 0)
		window->started = now;
	window->refreshes += refreshes;
	if (ticked)
		window->tick_frames++;
	else
		window->other_frames++;
	if (refreshes >= STEREO_CADENCE_HITCH_REFRESHES)
		window->hitch = 1;
	switch (stereo_cadence_classify(refreshes, trigger->repeat, period, tick, previous_render, previous_gpu))
	{
	case STEREO_CADENCE_LATE_SYSTEM:
		window->system_late++;
		break;
	case STEREO_CADENCE_LATE_OWN_WORK:
		if (ticked)
			window->tick_late++;
		else
			window->other_late++;
		break;
	case STEREO_CADENCE_ON_TIME:
		break;
	}
	return window->refreshes >= STEREO_CADENCE_WINDOW_REFRESHES;
}

/* the full window's verdict. PERIOD_MEASURED: the pacing report has measured the refresh period; WARM: the thermal
state is above nominal or Low Power Mode is on. Returns STEREO_CADENCE_STREAK_STARTED and STEREO_CADENCE_WOULD_DROP
bits (a streak's start at most STEREO_CADENCE_STREAK_LOGS times between report resets); *SKIPPED and *SEEN (each optional) get the window's skip reason (or STEREO_CADENCE_COUNTED) and its counts.
The window starts again */
static inline int stereo_cadence_decide(struct stereo_cadence_trigger *trigger, int period_measured, int warm,
	enum stereo_cadence_skip *skipped, struct stereo_cadence_window *seen)
{
	struct stereo_cadence_window *window = &trigger->window, empty = { 0 };
	unsigned long late = window->tick_late + window->other_late + window->system_late;
	enum stereo_cadence_skip skip = STEREO_CADENCE_COUNTED;
	int events = 0, tick_hit, other_hit, was_streaking;

	if (!period_measured)
		skip = STEREO_CADENCE_SKIP_PERIOD;
	else if (window->started < trigger->settled)
		skip = STEREO_CADENCE_SKIP_SETTLING;
	else if (warm)
		skip = STEREO_CADENCE_SKIP_THERMAL;
	else if (window->hitch)
		skip = STEREO_CADENCE_SKIP_HITCH;
	else if (late > 0 && 4 * window->system_late >= late)
		skip = STEREO_CADENCE_SKIP_SYSTEM;
	if (skipped)
		*skipped = skip;
	if (seen)
		*seen = *window;
	if (skip != STEREO_CADENCE_COUNTED)
	{
		trigger->skipped[skip]++;
		*window = empty;
		return 0;
	}
	trigger->counted++;
	tick_hit = window->tick_frames > 0 && 2 * window->tick_late >= window->tick_frames;
	other_hit = window->other_frames > 0 && 2 * window->other_late >= window->other_frames;
	was_streaking = trigger->tick_streak > 0 || trigger->other_streak > 0;
	trigger->tick_streak = tick_hit ? trigger->tick_streak + 1 : 0;
	trigger->other_streak = other_hit ? trigger->other_streak + 1 : 0;
	if (!was_streaking && (tick_hit || other_hit) && trigger->streaks++ < STEREO_CADENCE_STREAK_LOGS)
		events |= STEREO_CADENCE_STREAK_STARTED;
	if (trigger->tick_streak > trigger->longest)
		trigger->longest = trigger->tick_streak;
	if (trigger->other_streak > trigger->longest)
		trigger->longest = trigger->other_streak;
	if (!trigger->fired && (trigger->tick_streak >= STEREO_CADENCE_STREAK || trigger->other_streak >= STEREO_CADENCE_STREAK))
	{
		trigger->fired = 1;
		trigger->fired_in_report = 1;
		events |= STEREO_CADENCE_WOULD_DROP;
	}
	*window = empty;
	return events;
}

/* ---------- the trigger's step: scope, the two evaluations and the resets

Once a stereo frame of the eyes. Only HEAD mode in a variable (multiplayer) workload context is evaluated: campaign,
the menu and the theater screen never are. Two evaluations: at the asked repeat count (what a trigger that acted
would see), and, while that count is above 0, as if it were 0 (the stand-in for the Compositor choosing 45 Hz by
itself, which the acceptance runs check). A new GENERATION (a map's workload decision, the space opening) starts
both again; a change in the asked count scores from a fresh window, but would drop fires at most once a map. The previous frame's render and GPU times are carried from the
last frame fed, and forgotten outside the scope and at every start. */

/* the refresh period frames are counted in before the pacing report has measured one (every window is then
skipped, STEREO_CADENCE_SKIP_PERIOD) */
#define STEREO_CADENCE_UNMEASURED_PERIOD (1.0 / 90.0)

struct stereo_cadence_input
{
	int mode;                        /* enum halo_stereo_mode in effect */
	int variable;                    /* the map's workload context is variable */
	unsigned generation;
	int repeat;                      /* the asked repeat count */
	double now;                      /* seconds */
	int ticked;
	double interval, tick, render, gpu; /* seconds: since the previous presentation; this frame's times */
	double period;                   /* the measured refresh period; 0 before one */
	int (*warm)(void);               /* thermal state above nominal or Low Power Mode; asked once a step at most */
};

struct stereo_cadence_state
{
	struct stereo_cadence_trigger asked, as_zero;
	unsigned generation;
	int started;
	double previous_render, previous_gpu;
};

/* a window that closed: which evaluation, what stereo_cadence_decide returned, and its counts */
struct stereo_cadence_verdict
{
	const struct stereo_cadence_trigger *trigger;
	int events;
	struct stereo_cadence_window window;
};

/* feeds one frame; returns how many windows closed (0 to 2), described in VERDICTS */
static inline int stereo_cadence_step(struct stereo_cadence_state *state, const struct stereo_cadence_input *in,
	struct stereo_cadence_verdict verdicts[2])
{
	int measured = in->period > 0.0, closed = 0, warm = -1, index;
	double period = measured ? in->period : STEREO_CADENCE_UNMEASURED_PERIOD;
	struct stereo_cadence_trigger *triggers[2];
	long refreshes;

	if (in->mode != HALO_STEREO_HEAD || !in->variable)
	{
		state->previous_render = state->previous_gpu = 0.0;
		return 0;
	}
	if (!state->started || in->generation != state->generation)
	{
		state->started = 1;
		state->generation = in->generation;
		stereo_cadence_trigger_reset(&state->asked, in->repeat, in->now);
		stereo_cadence_trigger_reset(&state->as_zero, 0, in->now);
		state->previous_render = state->previous_gpu = 0.0;
	}
	else if (in->repeat != state->asked.repeat)
	{
		stereo_cadence_trigger_rescore(&state->asked, in->repeat, in->now);
		stereo_cadence_trigger_rescore(&state->as_zero, 0, in->now);
		state->previous_render = state->previous_gpu = 0.0;
	}
	refreshes = stereo_cadence_refreshes(in->interval, period, STEREO_CADENCE_HITCH_REFRESHES);
	triggers[0] = &state->asked;
	triggers[1] = in->repeat > 0 ? &state->as_zero : NULL;
	for (index = 0; index < 2; index++)
	{
		struct stereo_cadence_trigger *trigger = triggers[index];

		if (!trigger || !stereo_cadence_window_add(trigger, in->now, in->ticked, refreshes, in->tick,
			state->previous_render, state->previous_gpu, period))
			continue;
		if (warm < 0)
			warm = in->warm ? in->warm() != 0 : 0;
		verdicts[closed].trigger = trigger;
		verdicts[closed].events = stereo_cadence_decide(trigger, measured, warm, NULL, &verdicts[closed].window);
		closed++;
	}
	state->previous_render = in->render;
	state->previous_gpu = in->gpu;
	return closed;
}
