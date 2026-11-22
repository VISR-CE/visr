/* A model of the paced frame loop around frame_pacer.h's rules, for
frame_pacer_sim_probe.c: one game thread, one GPU, a display that shows at
most one frame a refresh, and the pacer's own planner code deciding the
starts. No clock and no threads: time is a number the model advances.

What the model takes from the host (host_frame_pacer.c, gpu_metal.m's
Present):

1. frame start: the late start's decision and wait, then the input read;
2. the frame's CPU work;
3. Present: the wait for one of the frames in flight's slots, given back when
   the display's shown notice arrives, a little after the refresh; under a
   scene class's rate and no aim, the wait for the frame's refreshes
   (frame_context_wait);
4. commit: the GPU runs frames one after another, each when the one before
   has ended;
5. the display shows the frame at the first refresh that is at least `latch`
   after its GPU work ended and at least the pacer's minimum duration after
   the frame before;
6. dynamic resolution (dynamic_resolution.h, the real planner) is fed each
   shown frame in order and picks the level later frames render at.

All times are seconds. */
#pragma once

#include "dynamic_resolution.h"
#include "frame_pacer.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum
{
	SIM_OFF,      /* display.late_start = "false" */
	SIM_CLASSIC,  /* "true": frame_pacer_late_wait */
	SIM_EXTENDED  /* "extended": frame_pacer_extended_wait */
};

enum { SIM_EVENTS = 64 };

struct sim;

/* a frame's costs: its CPU work, and its GPU work as a fixed part plus a part for each pixel */
typedef void sim_cost(const struct sim *sim, long frame, double now, double *cpu, double *gpu_fixed,
	double *gpu_per_pixel);

struct sim_frame
{
	double read;        /* the input read (after the late start's wait) */
	double waited;      /* in Present, for a slot or the scene class's refreshes */
	double slept;       /* the late start's wait */
	double cpu, gpu;
	double gpu_end, shown;
	double interval;    /* since the frame shown before */
	double aim, allowed;
	int level;
	int state;
	int in_flight_at_present;
};

struct sim_event
{
	double at;
	int kind;           /* 0: the GPU ended a frame; 1: the shown notice */
	long frame;
};

struct sim
{
	/* the device */
	double panel, link, share;
	int limit, mode;
	double latch;          /* the GPU's end to the refresh that can show it */
	double notice;         /* the refresh to the shown notice */
	int context_wait;      /* a scene class's rate: Present waits for the frame's refreshes when it has no aim */
	sim_cost *cost;
	void *user;
	/* dynamic resolution: off when drs_height is 0 */
	int drs_width, drs_height, drs_floor;
	struct dynamic_resolution plan;
	int level;
	/* the pacer */
	struct frame_pacer_flight flight;
	struct frame_pacer_late classic;
	struct frame_pacer_extended extended;
	/* the run */
	double now, gpu_free, shown_last, shown_noticed, gpu_total, gpu_counted;
	double present_refreshes; /* link refreshes counted at the last Present (frame_context_wait) */
	int waited_for_display;
	long handed, shown_highest; /* frames handed to the display, and the highest of them known shown: the count in
	                            flight is the difference, so a notice that never comes is made up by the next */
	long lose_notice;           /* this frame's shown notice never arrives (0: none) */
	struct sim_event events[SIM_EVENTS];
	int event_count;
	struct sim_frame *frames;
	long count, capacity;
	long drs_fed;
};

static inline double sim_pixels(const struct sim *sim, int level)
{
	return sim->drs_height ? sim->plan.pixels[level] : 1.0;
}

static inline void sim_start(struct sim *sim, long capacity)
{
	sim->frames = calloc((size_t)capacity, sizeof(*sim->frames));
	sim->capacity = capacity;
	sim->flight.limit = sim->limit;
	sim->now = 100.0;
	if (sim->drs_height)
		dynamic_resolution_init(&sim->plan, sim->drs_width, sim->drs_height, sim->drs_floor, 0.0, 0.0);
}

static inline void sim_event_add(struct sim *sim, double at, int kind, long frame)
{
	int index = sim->event_count++;

	while (index > 0 && sim->events[index - 1].at > at)
	{
		sim->events[index] = sim->events[index - 1];
		index--;
	}
	sim->events[index].at = at;
	sim->events[index].kind = kind;
	sim->events[index].frame = frame;
}

static inline double sim_slot(const struct sim *sim)
{
	return sim->share * sim->link;
}

/* the events up to a time, in order: what the completion and presented handlers do */
static inline void sim_events_until(struct sim *sim, double until)
{
	while (sim->event_count && sim->events[0].at <= until)
	{
		struct sim_event event = sim->events[0];
		struct sim_frame *frame = &sim->frames[event.frame];

		memmove(sim->events, sim->events + 1, (size_t)--sim->event_count * sizeof(event));
		if (!event.kind)
		{
			sim->gpu_total += frame->gpu;
			if (sim->mode == SIM_EXTENDED)
				frame_pacer_extended_cost(&sim->extended, frame->gpu_end - frame->read - frame->waited, frame->gpu,
					frame->cpu, frame->allowed, event.at);
			continue;
		}
		frame_pacer_flight_give(&sim->flight);
		if (sim->lose_notice && event.frame == sim->lose_notice)
			continue;
		if (event.frame + 1 > sim->shown_highest)
			sim->shown_highest = event.frame + 1;
		sim->shown_noticed = frame->shown;
		if (sim->mode == SIM_CLASSIC)
			frame_pacer_late_shown(&sim->classic, frame->aim, frame->shown, sim->link);
		else if (sim->mode == SIM_EXTENDED)
			frame_pacer_extended_shown(&sim->extended, frame->aim, frame->shown, frame->aim - frame->gpu_end,
				frame_pacer_extended_step(sim->link, sim->panel), event.at);
		if (sim->drs_height)
		{
			struct dynamic_resolution_frame fed = { 0 };
			int before = sim->plan.level;

			fed.frame_id = (unsigned long long)event.frame + 1;
			fed.level = frame->level;
			fed.elapsed = frame->interval > 0.0 ? frame->interval : sim_slot(sim);
			fed.scene = frame->gpu;
			fed.cpu = frame->cpu;
			fed.budget = sim_slot(sim);
			fed.gpu_available = 1;
			fed.late = frame->interval > 1.5 * sim_slot(sim);
			(void)dynamic_resolution_present(&sim->plan, &fed);
			sim->drs_fed++;
			if (sim->plan.level != before && sim->mode == SIM_EXTENDED)
				frame_pacer_extended_scale(&sim->extended, sim->plan.pixels[sim->plan.level] / sim->plan.pixels[before]);
			sim->level = sim->plan.level;
		}
	}
}

/* the first refresh of the panel at or after a time (refreshes are whole multiples of the panel's period) */
static inline double sim_refresh_from(const struct sim *sim, double at)
{
	return ceil(at / sim->panel - 1e-7) * sim->panel;
}

/* the display link's latest refresh at a time */
static inline double sim_link_refresh(const struct sim *sim, double at)
{
	return floor(at / sim->link + 1e-7) * sim->link;
}

/* one frame of the loop */
static inline void sim_frame(struct sim *sim)
{
	struct sim_frame *frame = &sim->frames[sim->count];
	double cpu, gpu_fixed, gpu_per_pixel, aim = 0.0, wait = 0.0, minimum, shown, begun;
	int taken;

	memset(frame, 0, sizeof(*frame));
	sim_events_until(sim, sim->now);
	frame->state = FRAME_PACER_START_OFF;
	/* 1. frame start */
	if (sim->mode == SIM_CLASSIC)
	{
		double gpu_time = sim->gpu_total - sim->gpu_counted;

		sim->gpu_counted = sim->gpu_total;
		sim->classic.allowed = 0.0;
		if (sim->waited_for_display && sim->share == 1.0)
			wait = frame_pacer_late_wait(&sim->classic, sim->now, sim_link_refresh(sim, sim->now), sim->link, &aim);
		frame->state = aim > 0.0 ? FRAME_PACER_START_ENGAGED : FRAME_PACER_START_NO_FIT;
		/* (the host adds the GPU's time since the last start to this frame's CPU work at its Present) */
		frame->allowed = gpu_time;
	}
	else if (sim->mode == SIM_EXTENDED && sim->waited_for_display)
	{
		struct frame_pacer_extended_input in = { sim->now, sim_link_refresh(sim, sim->now), sim->link, sim->panel,
			sim->share, sim->limit, (int)(sim->handed - sim->shown_highest), sim->shown_noticed };
		struct frame_pacer_extended_start start = frame_pacer_extended_wait(&sim->extended, &in);

		wait = start.wait;
		aim = start.aim;
		frame->allowed = start.allowed;
		frame->state = start.state;
	}
	sim->now += wait;
	sim_events_until(sim, sim->now);
	frame->slept = wait;
	frame->read = sim->now;
	frame->aim = aim;
	frame->level = sim->level;
	/* 2. the frame's CPU work */
	sim->cost(sim, sim->count, sim->now, &cpu, &gpu_fixed, &gpu_per_pixel);
	frame->cpu = cpu;
	frame->gpu = gpu_fixed + gpu_per_pixel * sim_pixels(sim, frame->level);
	sim->now += cpu;
	sim_events_until(sim, sim->now);
	/* 3. Present */
	begun = sim->now;
	if (sim->mode == SIM_CLASSIC)
	{
		frame_pacer_late_cost(&sim->classic, sim->now - frame->read + frame->allowed);
		frame->allowed = sim->classic.allowed;
	}
	sim->waited_for_display = 1;
	frame->in_flight_at_present = (int)(sim->handed - sim->shown_highest);
	taken = frame_pacer_flight_take(&sim->flight);
	while (!taken && sim->event_count)
	{
		if (sim->events[0].at > sim->now)
			sim->now = sim->events[0].at;
		sim_events_until(sim, sim->now);
		taken = frame_pacer_flight_take(&sim->flight);
	}
	if (sim->context_wait && aim <= 0.0)
	{
		/* until the link has called for the frame's refreshes since the last Present */
		double due = (sim->present_refreshes + sim->share) * sim->link;

		if (due > sim->now)
			sim->now = due;
		sim_events_until(sim, sim->now);
	}
	sim->present_refreshes = floor(sim->now / sim->link + 1e-7);
	frame->waited = sim->now - begun;
	/* 4. commit */
	frame->gpu_end = (sim->gpu_free > sim->now ? sim->gpu_free : sim->now) + frame->gpu;
	sim->gpu_free = frame->gpu_end;
	sim_event_add(sim, frame->gpu_end, 0, sim->count);
	/* 5. the display */
	minimum = frame_pacer_minimum_duration_panel(1, 1, sim->link, sim->share > 1.0 ? 1.0 / sim_slot(sim) : 0.0,
		sim->panel < sim->link ? sim->panel : 0.0);
	shown = sim_refresh_from(sim, frame->gpu_end + sim->latch);
	if (sim->shown_last > 0.0)
	{
		double after = sim_refresh_from(sim, sim->shown_last + minimum);

		if (after < sim->shown_last + sim->panel - 1e-9)
			after = sim->shown_last + sim->panel;
		if (shown < after)
			shown = after;
		frame->interval = shown - sim->shown_last;
	}
	frame->shown = shown;
	sim->shown_last = shown;
	sim->handed = sim->count + 1;
	sim_event_add(sim, shown + sim->notice, 1, sim->count);
	sim->count++;
}

static inline void sim_run(struct sim *sim, long frames)
{
	while (frames-- > 0 && sim->count < sim->capacity)
		sim_frame(sim);
}

/* ---------- what a span of frames did */

struct sim_result
{
	long frames;
	double read_to_shown_median, read_to_shown_p95, read_to_shown_worst;
	long late;            /* intervals over 1.5 slots */
	long engaged;         /* frames with an aim */
	long missed;          /* frames shown over half a refresh after their aim */
	long waited;          /* frames that waited over 0.5 ms in Present */
	double waited_mean;   /* the mean wait in Present */
	double rate;          /* frames a second */
	int level_low, level_last;
};

static int sim_compare(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;

	return x < y ? -1 : x > y;
}

static inline struct sim_result sim_measure(const struct sim *sim, long first, long last)
{
	struct sim_result result = { 0 };
	double *delays = calloc((size_t)(last - first), sizeof(double));
	double step = frame_pacer_extended_step(sim->link, sim->panel), waited = 0.0;
	long index;

	for (index = first; index < last; index++)
	{
		const struct sim_frame *frame = &sim->frames[index];

		delays[result.frames++] = frame->shown - frame->read;
		result.late += frame->interval > 1.5 * sim_slot(sim);
		result.engaged += frame->aim > 0.0;
		result.missed += frame->aim > 0.0 && frame->shown > frame->aim + 0.5 * step;
		result.waited += frame->waited > 0.0005;
		waited += frame->waited;
		if (frame->level > result.level_low)
			result.level_low = frame->level;
		result.level_last = frame->level;
	}
	qsort(delays, (size_t)result.frames, sizeof(double), sim_compare);
	result.read_to_shown_median = delays[result.frames / 2];
	result.read_to_shown_p95 = delays[result.frames * 95 / 100];
	result.read_to_shown_worst = delays[result.frames - 1];
	result.waited_mean = waited / (double)result.frames;
	result.rate = (double)(last - first - 1) / (sim->frames[last - 1].shown - sim->frames[first].shown);
	free(delays);
	return result;
}
