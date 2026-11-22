/* Adaptive frames in flight: the pacer's choice between a floor of one frame in flight and a ceiling of two, frame
by frame, from what recent frames needed (design: agent-specs 2026-11-22 adaptive frames in flight, revision 3).

One frame in flight shows input a frame sooner but holds the next frame's present until the frame before it is
shown, so a frame whose GPU work and present do not fit one frame period drops the rate to the next divisor of the
display's (the iPad Air 4 at 60: 30 frames a second). A device already under its rate loses nothing to one in flight
and gains the shorter delay (the A10X iPad Pro in a 128-player game).

The rule, decided from presented frames only:

- enter the floor, once the retry delay has passed, when either
  - under rate: over a span at the ceiling, fewer than FRAME_PACER_ADAPT_UNDER_RATE of the rate in force was
    shown, or
  - fits: the worst GPU time plus the worst span from slot grant to present, each over the last
    FRAME_PACER_ADAPT_WINDOW frames with a valid value, plus the lead, is within FRAME_PACER_ADAPT_FIT of the period
    in force. Neither measure contains a slot wait, so it reads the same at one and at two.
- leave the floor on two slot-caused misses within FRAME_PACER_ADAPT_MISS_SPAN seconds (a frame that waited for
  its slot and came over one and a half periods after the frame before; under rate, one and a half of the interval
  the ceiling showed), or, for an under-rate stay, when a
  span at the floor shows more than FRAME_PACER_ADAPT_RATE_LOSS less than the rate seen at the ceiling.

Set from fixed-one against fixed-two traces (debug.present_times) on 2026-11-22:
- iPad Air 4, 32 players, 60 Hz: GPU 14 ms at the median and 16 to 21 at the 90th percentile, grant to present
  0.3 ms. One in flight showed 30 frames a second, two 60; the fit test (GPU worst plus grant plus 3 ms lead within
  90% of 16.7 ms) keeps it at two.
- A10X iPad Pro, 128 players: GPU-bound (GPU 38 to 41 ms at the median, the second slot in use for 88 to 90% of
  frames at two), and still one in flight is the better trade: 19.3 to 19.8 frames a second against 20.3 to 20.6 at
  two (5% fewer), with controller to shown 66 to 115 ms against 122 to 139. The queue off ran 18.2 at 94 to 117 ms,
  so two in flight is the arm that breaks the latency rule. A second slot in use therefore does not keep a device
  under its rate at two (the design's idle-slot test is dropped); the rate-loss exit decides, with a tolerance
  above that 5% and a span long enough that a 19-frame second's noise does not trip it.
- the retry delay gates every entry: FRAME_PACER_ADAPT_RETRY_FIRST, doubling after each exit up to
  FRAME_PACER_ADAPT_RETRY_MOST; it resets on a change of period or after FRAME_PACER_ADAPT_RETRY_CALM seconds at
  the floor without an exit.
- frames marked as transition (shown while the slots drain after a change) count toward nothing.

Pure: no clocks or locks; host_frame_pacer.c feeds it under the pacer lock and applies the limit it returns.
frame_pacer_adapt_probe.c (tools/ios_test.py) holds the cases. */
#pragma once

#define FRAME_PACER_ADAPT_WINDOW 60
#define FRAME_PACER_ADAPT_MINIMUM_SAMPLES 30
#define FRAME_PACER_ADAPT_FIT 0.90
#define FRAME_PACER_ADAPT_UNDER_RATE 0.95
#define FRAME_PACER_ADAPT_RATE_LOSS 0.10
#define FRAME_PACER_ADAPT_MISS_SPAN 0.5
#define FRAME_PACER_ADAPT_RETRY_FIRST 2.0
#define FRAME_PACER_ADAPT_RETRY_MOST 60.0
#define FRAME_PACER_ADAPT_RETRY_CALM 60.0
/* (the rate tests' span: two seconds, about 40 frames at the A10X's 19) */
#define FRAME_PACER_ADAPT_SPAN 2.0
/* a period that moved by more than this share is a new rate in force; less is the display link's jitter */
#define FRAME_PACER_ADAPT_PERIOD_CHANGE 0.02
/* an interval this long is a load, a pause or the background, not a frame (as frame_pacer_shown_add) */
#define FRAME_PACER_ADAPT_GAP 0.25

enum frame_pacer_adapt_reason
{
	FRAME_PACER_ADAPT_NONE,
	FRAME_PACER_ADAPT_FITS,        /* entered the floor: the work fits */
	FRAME_PACER_ADAPT_UNDER,       /* entered the floor: under rate at the ceiling */
	FRAME_PACER_ADAPT_MISSES,      /* left the floor: slot-caused misses */
	FRAME_PACER_ADAPT_RATE_LOST,   /* left the floor: an under-rate stay lost rate */
};

/* one presented frame, as the pacer saw it (seconds; 0 or less where unknown) */
struct frame_pacer_adapt_frame
{
	double at;               /* when it was shown */
	double interval;         /* since the frame before was shown; 0: none */
	double period;           /* the frame period in force: the display's refresh times the share of the rate in force */
	double gpu;              /* its GPU time */
	double grant_to_present; /* slot grant to the present; below 0: unknown (0 is a present at once) */
	double lead;             /* the compositor's lead the late start keeps */
	int slot_waited;         /* found every slot taken and waited */
	int transition;          /* shown while the slots drained after a change: counts toward nothing */
};

struct frame_pacer_adapt
{
	int floor, ceiling, limit;
	double gpu[FRAME_PACER_ADAPT_WINDOW], grant[FRAME_PACER_ADAPT_WINDOW];
	int gpu_count, gpu_next, grant_count, grant_next;
	double span_start;          /* the rate test's second began; 0: not yet */
	int span_frames;
	double rate_at_ceiling;     /* frames a second the last full second at the ceiling showed; 0: none */
	int under_rate_stay;        /* the floor was entered under rate */
	double miss_at[2];          /* the last two slot-caused misses at the floor */
	double retry_delay, retry_at, floor_since, period;
	unsigned long switches;
	enum frame_pacer_adapt_reason reason; /* of the latest switch */
};

/* floor and ceiling as the settings give them; a floor at or above the ceiling never moves */
static inline void frame_pacer_adapt_reset(struct frame_pacer_adapt *adapt, int floor, int ceiling)
{
	*adapt = (struct frame_pacer_adapt){ .floor = floor, .ceiling = ceiling > floor ? ceiling : floor };
	adapt->limit = adapt->ceiling;
	adapt->retry_delay = FRAME_PACER_ADAPT_RETRY_FIRST;
}

static inline void frame_pacer_adapt_push(double *ring, int *count, int *next, double value)
{
	ring[*next] = value;
	*next = (*next + 1) % FRAME_PACER_ADAPT_WINDOW;
	if (*count < FRAME_PACER_ADAPT_WINDOW)
		(*count)++;
}

static inline double frame_pacer_adapt_worst(const double *ring, int count)
{
	double worst = 0.0;

	for (int i = 0; i < count; i++)
		if (ring[i] > worst)
			worst = ring[i];
	return worst;
}

/* whether the work after the grant fits the period: -1 too few samples to say, 0 no, 1 yes */
static inline int frame_pacer_adapt_fits(const struct frame_pacer_adapt *adapt, double period, double lead)
{
	if (adapt->gpu_count < FRAME_PACER_ADAPT_MINIMUM_SAMPLES || adapt->grant_count < FRAME_PACER_ADAPT_MINIMUM_SAMPLES ||
		period <= 0.0)
		return -1;
	return frame_pacer_adapt_worst(adapt->gpu, adapt->gpu_count) + frame_pacer_adapt_worst(adapt->grant, adapt->grant_count) +
		(lead > 0.0 ? lead : 0.0) <= FRAME_PACER_ADAPT_FIT * period;
}

static inline void frame_pacer_adapt_switch(struct frame_pacer_adapt *adapt, int limit, double now,
	enum frame_pacer_adapt_reason reason)
{
	adapt->limit = limit;
	adapt->reason = reason;
	adapt->switches++;
	adapt->span_start = 0.0;
	adapt->miss_at[0] = adapt->miss_at[1] = 0.0;
	if (limit == adapt->floor)
	{
		adapt->floor_since = now;
		adapt->under_rate_stay = reason == FRAME_PACER_ADAPT_UNDER;
	}
	else
	{
		adapt->retry_at = now + adapt->retry_delay;
		adapt->retry_delay = adapt->retry_delay * 2.0 < FRAME_PACER_ADAPT_RETRY_MOST ? adapt->retry_delay * 2.0 :
			FRAME_PACER_ADAPT_RETRY_MOST;
		adapt->under_rate_stay = 0;
	}
}

/* one presented frame; returns the limit for the frames that start from now (reason of a switch in adapt->reason,
with adapt->switches counting them) */
static inline int frame_pacer_adapt_frame(struct frame_pacer_adapt *adapt, const struct frame_pacer_adapt_frame *frame)
{
	double now = frame->at;

	if (adapt->floor >= adapt->ceiling || now <= 0.0 || frame->period <= 0.0)
		return adapt->limit;
	if (adapt->period > 0.0 && (frame->period > adapt->period * (1.0 + FRAME_PACER_ADAPT_PERIOD_CHANGE) ||
		frame->period < adapt->period * (1.0 - FRAME_PACER_ADAPT_PERIOD_CHANGE)))
	{
		/* a new rate in force: the windows and the delay start again, and the limit stays */
		int limit = adapt->limit;
		unsigned long switches = adapt->switches;

		frame_pacer_adapt_reset(adapt, adapt->floor, adapt->ceiling);
		adapt->limit = limit;
		adapt->switches = switches;
		adapt->floor_since = limit == adapt->floor ? now : 0.0;
	}
	adapt->period = frame->period;
	if (frame->transition)
		return adapt->limit;
	if (frame->gpu > 0.0 && frame->gpu < FRAME_PACER_ADAPT_GAP)
		frame_pacer_adapt_push(adapt->gpu, &adapt->gpu_count, &adapt->gpu_next, frame->gpu);
	if (frame->grant_to_present >= 0.0 && frame->grant_to_present < FRAME_PACER_ADAPT_GAP)
		frame_pacer_adapt_push(adapt->grant, &adapt->grant_count, &adapt->grant_next, frame->grant_to_present);
	/* the rate test's second: frames shown after the one that started it, and a gap (a load, a pause) starts it again
	without a measure, so neither a hitch nor the frame at its edge reads as rate */
	if (adapt->span_start <= 0.0 || frame->interval <= 0.0 || frame->interval >= FRAME_PACER_ADAPT_GAP)
	{
		adapt->span_start = now;
		adapt->span_frames = 0;
		return adapt->limit;
	}
	adapt->span_frames++;
	if (adapt->limit == adapt->floor)
	{
		/* leave on slot-caused misses: late against the period, or, in a stay entered under rate, against the interval
		the ceiling showed (every frame there is over one and a half periods) */
		double expected = frame->period;

		if (adapt->under_rate_stay && adapt->rate_at_ceiling > 0.0 && 1.0 / adapt->rate_at_ceiling > expected)
			expected = 1.0 / adapt->rate_at_ceiling;
		if (frame->slot_waited && frame->interval > 1.5 * expected)
		{
			adapt->miss_at[0] = adapt->miss_at[1];
			adapt->miss_at[1] = now;
			if (adapt->miss_at[0] > 0.0 && now - adapt->miss_at[0] <= FRAME_PACER_ADAPT_MISS_SPAN)
			{
				frame_pacer_adapt_switch(adapt, adapt->ceiling, now, FRAME_PACER_ADAPT_MISSES);
				return adapt->limit;
			}
		}
		if (now - adapt->span_start >= FRAME_PACER_ADAPT_SPAN)
		{
			double rate = adapt->span_frames / (now - adapt->span_start);

			adapt->span_start = now;
			adapt->span_frames = 0;
			if (adapt->under_rate_stay && adapt->rate_at_ceiling > 0.0 &&
				rate < adapt->rate_at_ceiling * (1.0 - FRAME_PACER_ADAPT_RATE_LOSS))
			{
				frame_pacer_adapt_switch(adapt, adapt->ceiling, now, FRAME_PACER_ADAPT_RATE_LOST);
				return adapt->limit;
			}
		}
		/* a long calm stay forgives the delay */
		if (adapt->floor_since > 0.0 && now - adapt->floor_since >= FRAME_PACER_ADAPT_RETRY_CALM)
			adapt->retry_delay = FRAME_PACER_ADAPT_RETRY_FIRST;
		return adapt->limit;
	}
	/* at the ceiling: enter the floor? */
	if (now - adapt->span_start >= FRAME_PACER_ADAPT_SPAN)
	{
		double rate = adapt->span_frames / (now - adapt->span_start);
		int fits = frame_pacer_adapt_fits(adapt, frame->period, frame->lead);

		adapt->rate_at_ceiling = rate;
		adapt->span_start = now;
		adapt->span_frames = 0;
		if (now < adapt->retry_at)
			return adapt->limit;
		if (rate < FRAME_PACER_ADAPT_UNDER_RATE / frame->period)
			frame_pacer_adapt_switch(adapt, adapt->floor, now, FRAME_PACER_ADAPT_UNDER);
		else if (fits == 1)
			frame_pacer_adapt_switch(adapt, adapt->floor, now, FRAME_PACER_ADAPT_FITS);
	}
	return adapt->limit;
}
