/*
AUDIO_BUS.H

The mixer's rate bus (dsound_sdl.c, audio.rate_bus). A voice playing at exactly its
source rate (22,050 or 44,100 Hz) has a fixed step of 147/320 or 147/160 source
frames an output frame, so all such voices of one rate share one grid of source
frames, and because the resampling filter F is linear and a voice's gain ramps
linearly across a callback (gain(f) = a + b * f, f the output frame in the
callback), the sum of the voices' filtered, ramped outputs is

	F(sum a_v x_v)(f) + f * F(sum b_v x_v)(f)

so each rate sums its voices' source frames, times their gains, into two
sequences a channel (A with the starting gains, B with the ramps), and filters
each once. The sums are rebuilt for every callback's window, so nothing about a
gain is ever kept in them.

The grid is a running count of output frames (the mixer's clock); output frame
F is at source position F * num / den, an integer frame and a fraction in
den-ths, in 64-bit integers, so it never drifts and any callback size works.
*/

#ifndef AUDIO_BUS_H
#define AUDIO_BUS_H

#include <string.h>

#include "audio_filter.h"

#define AUDIO_BUS_MAXIMUM_FRAMES 1024 /* the longest callback (MIX_CHUNK_FRAMES) */
/* a callback's window of source frames: one a frame at most (a rate at or below the output's), and the filter's
reach on both sides */
#define AUDIO_BUS_WINDOW (AUDIO_BUS_MAXIMUM_FRAMES + RESAMPLER_TAPS + 2)

enum { AUDIO_BUS_LEFT, AUDIO_BUS_RIGHT, AUDIO_BUS_RAMP_LEFT, AUDIO_BUS_RAMP_RIGHT, AUDIO_BUS_SEQUENCES };

struct audio_bus
{
	unsigned numerator, denominator; /* source frames an output frame, in lowest terms */
	long long first;                 /* the grid index of the window's first frame */
	unsigned long length;            /* frames in the window */
	int used, ramped;                /* a voice added to it; a ramp added */
	float sums[AUDIO_BUS_SEQUENCES][AUDIO_BUS_WINDOW];
};

static inline void audio_bus_initialize(struct audio_bus *bus, unsigned numerator, unsigned denominator)
{
	memset(bus, 0, sizeof(*bus));
	bus->numerator = numerator;
	bus->denominator = denominator;
}

/* the integer frame of output frame `clock`'s source position, and its fraction's numerator (of denominator) */
static inline long long audio_bus_frame(const struct audio_bus *bus, unsigned long long clock, unsigned *fraction)
{
	unsigned long long product = clock * bus->numerator;

	*fraction = (unsigned)(product % bus->denominator);
	return (long long)(product / bus->denominator);
}

/* a callback of `frames` output frames from `clock`: the window of source frames the filter reaches */
static inline void audio_bus_begin(struct audio_bus *bus, unsigned long long clock, unsigned long frames)
{
	unsigned fraction;
	long long first = audio_bus_frame(bus, clock, &fraction);
	long long last = audio_bus_frame(bus, clock + frames - 1, &fraction);

	bus->first = first - (RESAMPLER_ZERO_CROSSINGS - 1);
	bus->length = (unsigned long)(last + RESAMPLER_ZERO_CROSSINGS - bus->first + 1);
	bus->used = bus->ramped = 0;
}

/* the sums are cleared when the first voice is added: a rate with no voice costs nothing */
static inline void audio_bus_use(struct audio_bus *bus)
{
	if (!bus->used)
	{
		int sequence;

		for (sequence = 0; sequence < AUDIO_BUS_SEQUENCES; sequence++)
			memset(bus->sums[sequence], 0, bus->length * sizeof(float));
		bus->used = 1;
	}
}

/* one source frame of a voice, `left` and `right` its two channels (equal for a mono voice), at grid index
`index` (inside the window), with its gains at the callback's start and its ramps */
static inline void audio_bus_add(struct audio_bus *bus, long long index, float left, float right,
	float gain_left, float gain_right, float ramp_left, float ramp_right, int ramping)
{
	unsigned long slot = (unsigned long)(index - bus->first);

	bus->sums[AUDIO_BUS_LEFT][slot] += gain_left * left;
	bus->sums[AUDIO_BUS_RIGHT][slot] += gain_right * right;
	if (ramping)
	{
		bus->sums[AUDIO_BUS_RAMP_LEFT][slot] += ramp_left * left;
		bus->sums[AUDIO_BUS_RAMP_RIGHT][slot] += ramp_right * right;
		bus->ramped = 1;
	}
}

/* the bus's output for the callback added into `output` (frames of stereo float): `phases` is the filter's
phase table (RESAMPLER_TABLE_STEPS + 1 rows) */
static inline void audio_bus_finish(struct audio_bus *bus, unsigned long long clock, unsigned long frames,
	const float (*phases)[RESAMPLER_TAPS], unsigned table_steps, float *output)
{
	unsigned long frame;

	if (!bus->used)
		return;
	for (frame = 0; frame < frames; frame++)
	{
		unsigned fraction;
		long long index = audio_bus_frame(bus, clock + frame, &fraction);
		double position = (double)fraction * (double)table_steps / (double)bus->denominator;
		unsigned long row = (unsigned long)position;
		float blend = (float)(position - (double)row);
		float blended[RESAMPLER_TAPS];
		unsigned long start = (unsigned long)(index - (RESAMPLER_ZERO_CROSSINGS - 1) - bus->first);
		float left, right;

		audio_filter_blend(phases[row], phases[row + 1], blend, blended);
		left = audio_filter_dot(bus->sums[AUDIO_BUS_LEFT] + start, blended);
		right = audio_filter_dot(bus->sums[AUDIO_BUS_RIGHT] + start, blended);
		if (bus->ramped)
		{
			left += (float)frame * audio_filter_dot(bus->sums[AUDIO_BUS_RAMP_LEFT] + start, blended);
			right += (float)frame * audio_filter_dot(bus->sums[AUDIO_BUS_RAMP_RIGHT] + start, blended);
		}
		output[frame * 2] += left;
		output[frame * 2 + 1] += right;
	}
}

/* ---------- a voice on the bus */

/* the state a voice keeps for resampling, whichever path it is on (sdl_stream.voice): the last RESAMPLER_HISTORY frames
taken (frame index modulo the history), how many were taken, the frame the output is at and how far past it (the
per-voice path's position; a bus voice keeps them where the per-voice path would be, so leaving the bus is clearing
the flag), the frames of silence taken since the packets ran out, and where the voice is in the rate bus's decision.
A bus voice's frame i is the bus's grid frame i + bus_offset. */
struct audio_voice
{
	float history[RESAMPLER_HISTORY][2];
	unsigned long long history_count, center; /* 64 bits: the guest's unsigned long wraps after 27 hours of one voice */
	double phase;
	unsigned long silence;
	int bus_state;
	long long bus_offset;
	unsigned long long bus_next; /* the clock the voice's next callback starts at, for it to stay on the bus */
};

enum { BUS_UNDECIDED, BUS_ON, BUS_OFF };

/* the silence before a voice's first frame: a stretched low pass reaches this far behind */
#define AUDIO_VOICE_LEAD (RESAMPLER_ZERO_CROSSINGS * RESAMPLER_MAXIMUM_STRETCH)

/* a voice starting (or starting over): silence before its first frame, which the output starts at */
static inline void audio_voice_reset(struct audio_voice *voice)
{
	memset(voice->history, 0, sizeof(voice->history));
	voice->history_count = AUDIO_VOICE_LEAD;
	voice->center = voice->history_count;
	voice->phase = 0.0;
	voice->silence = 0;
	voice->bus_state = BUS_UNDECIDED;
}

/* a source of frames: fills frame[0] and frame[1] (one value twice for a mono voice) and returns 1, or returns 0
once there are no more */
typedef int (*audio_bus_take)(void *source, float *frame);

/* at the start of a callback, whether the voice is on the bus (`bus` the one it may play on, NULL if it may not: a
pitched voice, a voice panned towards its sound, the setting off). A voice starts on the bus only fresh, from
audio_voice_reset; once off it stays off until reset; leaving, or being passed over by a callback, clears the flag
and the per-voice path carries on from `center` and `phase`, which the last bus callback left where it would be. */
static inline int audio_bus_voice_prepare(struct audio_voice *voice, const struct audio_bus *bus, unsigned long long clock)
{
	if (voice->bus_state == BUS_ON && (!bus || voice->bus_next != clock))
		voice->bus_state = BUS_OFF;
	else if (voice->bus_state == BUS_UNDECIDED)
	{
		if (bus && voice->history_count == AUDIO_VOICE_LEAD)
		{
			unsigned fraction;
			long long first = audio_bus_frame(bus, clock, &fraction) + (fraction ? 1 : 0);

			/* the voice's first frame on the grid frame at or after output frame 0's position */
			voice->bus_offset = first - (long long)voice->history_count;
			voice->bus_next = clock;
			voice->bus_state = BUS_ON;
		}
		else
			voice->bus_state = BUS_OFF;
	}
	return voice->bus_state == BUS_ON;
}

/* a bus voice's callback: its source frames of the bus's window, taken as the per-voice path takes them, added into the
bus times its gains (`left` and `right` at the callback's start, the ramps to the target); `width` is the filter's
reach (RESAMPLER_ZERO_CROSSINGS) for the silence a voice plays out */
static inline void audio_bus_mix_voice(struct audio_voice *voice, struct audio_bus *bus, unsigned long long clock,
	unsigned long frames, float left, float right, float ramp_left, float ramp_right, long width, audio_bus_take take, void *source)
{
	int active = left != 0.0f || right != 0.0f || ramp_left != 0.0f || ramp_right != 0.0f;
	int ramping = ramp_left != 0.0f || ramp_right != 0.0f;
	long long index, end = bus->first + (long long)bus->length, next;
	unsigned fraction;

	if (active)
		audio_bus_use(bus);
	for (index = bus->first; index < end; index++)
	{
		long long frame_index = index - voice->bus_offset;
		const float *slot;

		if (frame_index < 0)
			continue;
		while ((unsigned long long)frame_index >= voice->history_count)
		{
			float *fresh = voice->history[voice->history_count % RESAMPLER_HISTORY];

			if (take(source, fresh))
				voice->silence = 0;
			else
			{
				fresh[0] = fresh[1] = 0.0f;
				voice->silence++;
			}
			voice->history_count++;
			if (voice->silence > (unsigned long)(2 * width))
				goto taken;
		}
		slot = voice->history[(unsigned long)frame_index % RESAMPLER_HISTORY];
		if (active)
			audio_bus_add(bus, index, slot[0], slot[1], left, right, ramp_left, ramp_right, ramping);
	}
taken:
	/* where the per-voice path would be at the next callback */
	next = audio_bus_frame(bus, clock + frames, &fraction);
	voice->center = (unsigned long long)(next - voice->bus_offset);
	voice->phase = (double)fraction / (double)bus->denominator;
	voice->bus_next = clock + frames;
}

#endif
