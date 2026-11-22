/*
AUDIO_FILTER.H

The mixer's inner loop (dsound_sdl.c, "resampling"): one output frame of a voice
at or below the output rate is the weighted sum of RESAMPLER_TAPS source frames
around its moment, the weights blended between two rows of the phase table.

audio_filter_reference is the loop as it always was: each channel one running
sum in tap order. audio_filter_fast (audio.fast_filter) takes the same weights
and the same frames, adds them into four running sums a channel, and a mono
voice, whose two channels are the same frame, computes only the one. The sums
are formed in a different order, so a result differs from the reference's by
float rounding, within AUDIO_FILTER_FAST_BOUND for frames and weights as the
mixer has them (frames within +-1, weights summing to about 1); a mono voice's
left and right are one value, the same bits the fast path gives a stereo voice
whose channels are equal.
*/

#ifndef AUDIO_FILTER_H
#define AUDIO_FILTER_H

#define RESAMPLER_ZERO_CROSSINGS 24
/* the most a voice's low pass is narrowed for a step above 1 */
#define RESAMPLER_MAXIMUM_STRETCH 2
#define RESAMPLER_TAPS (2 * RESAMPLER_ZERO_CROSSINGS)
/* the frames a voice keeps (a power of two); one more than the taps wraps */
#define RESAMPLER_HISTORY 128

_Static_assert(RESAMPLER_TAPS % 4 == 0, "the fast sums take four taps at a time");
_Static_assert((RESAMPLER_HISTORY & (RESAMPLER_HISTORY - 1)) == 0 && RESAMPLER_HISTORY >= RESAMPLER_TAPS,
	"the history wraps by remainder and holds all the taps");

/* the most the fast sum can differ from the reference's, a sample of a unit-level voice */
#define AUDIO_FILTER_FAST_BOUND 2.0e-6f

static inline float audio_filter_weight(const float *weights, const float *next_weights, float blend, int tap)
{
	return weights[tap] + (next_weights[tap] - weights[tap]) * blend;
}

/* `first` is the history's index of the first tap (it wraps at RESAMPLER_HISTORY) */
static inline void audio_filter_reference(const float (*history)[2], unsigned long first, const float *weights,
	const float *next_weights, float blend, float *left, float *right)
{
	float sample_left = 0.0f, sample_right = 0.0f;
	int tap;

	for (tap = 0; tap < RESAMPLER_TAPS; tap++)
	{
		const float *source = history[(first + (unsigned long)tap) % RESAMPLER_HISTORY];
		float weight = audio_filter_weight(weights, next_weights, blend, tap);

		sample_left += source[0] * weight;
		sample_right += source[1] * weight;
	}
	*left = sample_left;
	*right = sample_right;
}

/* four floats at once (a compiler vector: one NEON register on the Apple chips) */
typedef float audio_filter_vector __attribute__((vector_size(16)));

static inline audio_filter_vector audio_filter_load(const float *pointer)
{
	audio_filter_vector value;

	__builtin_memcpy(&value, pointer, sizeof(value));
	return value;
}

/* for a mono voice *right is *left; the sums are the four lanes a channel (the taps 0, 4, 8 ... in lane 0, and so on),
added as (0 + 1) + (2 + 3) */
static inline void audio_filter_fast(const float (*history)[2], unsigned long first, const float *weights,
	const float *next_weights, float blend, int stereo, float *left, float *right)
{
	float gathered[RESAMPLER_TAPS][2];
	const float (*source)[2];
	audio_filter_vector sum_left = { 0.0f, 0.0f, 0.0f, 0.0f }, sum_right = { 0.0f, 0.0f, 0.0f, 0.0f };
	unsigned long start = first % RESAMPLER_HISTORY;
	int tap;

	if (start + RESAMPLER_TAPS <= RESAMPLER_HISTORY)
		source = history + start;
	else
	{
		for (tap = 0; tap < RESAMPLER_TAPS; tap++)
		{
			gathered[tap][0] = history[(start + (unsigned long)tap) % RESAMPLER_HISTORY][0];
			gathered[tap][1] = history[(start + (unsigned long)tap) % RESAMPLER_HISTORY][1];
		}
		source = (const float (*)[2])gathered;
	}
	for (tap = 0; tap < RESAMPLER_TAPS; tap += 4)
	{
		audio_filter_vector weight = audio_filter_load(weights + tap);
		audio_filter_vector difference = audio_filter_load(next_weights + tap) - weight;
		audio_filter_vector first_pair = audio_filter_load(&source[tap][0]);
		audio_filter_vector second_pair = audio_filter_load(&source[tap + 2][0]);

		weight = weight + difference * blend;
		sum_left += __builtin_shufflevector(first_pair, second_pair, 0, 2, 4, 6) * weight;
		if (stereo)
			sum_right += __builtin_shufflevector(first_pair, second_pair, 1, 3, 5, 7) * weight;
	}
	*left = (sum_left[0] + sum_left[1]) + (sum_left[2] + sum_left[3]);
	*right = stereo ? (sum_right[0] + sum_right[1]) + (sum_right[2] + sum_right[3]) : *left;
}

/* the weights of one output frame, the two rows of the phase table blended (as audio_filter_fast does for each tap) */
static inline void audio_filter_blend(const float *weights, const float *next_weights, float blend, float *blended)
{
	int tap;

	for (tap = 0; tap < RESAMPLER_TAPS; tap += 4)
	{
		audio_filter_vector weight = audio_filter_load(weights + tap);
		audio_filter_vector difference = audio_filter_load(next_weights + tap) - weight;

		weight = weight + difference * blend;
		__builtin_memcpy(blended + tap, &weight, sizeof(weight));
	}
}

/* RESAMPLER_TAPS contiguous frames against blended weights: the four lane sums added as (0 + 1) + (2 + 3) */
static inline float audio_filter_dot(const float *source, const float *blended)
{
	audio_filter_vector sum = { 0.0f, 0.0f, 0.0f, 0.0f };
	int tap;

	for (tap = 0; tap < RESAMPLER_TAPS; tap += 4)
		sum += audio_filter_load(source + tap) * audio_filter_load(blended + tap);
	return (sum[0] + sum[1]) + (sum[2] + sum[3]);
}

/* what the mixer calls: the reference loop, or (fast, audio.fast_filter) the fast one with `stereo` the voice's own
channel count being 2; a mono voice's result is on both channels either way */
static inline void audio_filter_voice(int fast, const float (*history)[2], unsigned long first, const float *weights,
	const float *next_weights, float blend, int stereo, float *left, float *right)
{
	if (fast)
		audio_filter_fast(history, first, weights, next_weights, blend, stereo, left, right);
	else
		audio_filter_reference(history, first, weights, next_weights, blend, left, right);
}

#endif
