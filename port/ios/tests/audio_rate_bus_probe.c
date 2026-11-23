/* audio_bus.h: the rate bus's output for scripted voices against the mixer's per-voice path (the reference loop of
mix_voice, one voice at a time, with a gain ramp in closed form; the bus side is audio_bus.h's own voice functions, the ones
dsound_sdl.c calls), on every callback, within a bound that grows with
the voices' gains. The voices start on different callbacks (the bus starts a voice on the grid frame at or after its
first output frame, which the reference matches by starting at that fraction), end mid-callback, ramp across zero, are
mono and stereo at both rates (run also with the clock at 2^34 - 700, 'far', crossing 2^32), and the callbacks are 512, 480, 1,024 and 1 frames. Run with a mutation name
(gain_first, late_start, no_ramp) to see the check fail. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_bus.h"

_Static_assert(sizeof(((struct audio_voice *)0)->history_count) >= 8 && sizeof(((struct audio_voice *)0)->center) >= 8,
	"a voice's frame counts must not wrap on a 32-bit unsigned long");

/* strict C11 on Linux does not define M_PI */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TABLE_STEPS 256
#define VOICES 8
#define CALLBACKS 60

static float phases[TABLE_STEPS + 1][RESAMPLER_TAPS];
static unsigned state = 7u;
static float random_unit(void)
{
	state = state * 1664525u + 1013904223u;
	return (float)(state >> 8) / (float)(1u << 23) - 1.0f;
}

static const char *mutation = "";
static int failures;
#define CHECK(condition, ...) do { if (!(condition)) { failures++; if (failures < 12) { printf("FAIL: " __VA_ARGS__); printf("\n"); } } } while (0)

static void make_phases(void)
{
	unsigned row;
	int tap;

	for (row = 0; row <= TABLE_STEPS; row++)
	{
		double sum = 0.0, offset = (double)row / TABLE_STEPS;

		for (tap = 0; tap < RESAMPLER_TAPS; tap++)
		{
			double x = (double)(tap + 1 - RESAMPLER_ZERO_CROSSINGS) - offset;
			double window = 0.5 + 0.5 * cos(M_PI * x / RESAMPLER_ZERO_CROSSINGS);

			phases[row][tap] = (float)((fabs(x) < 1e-9 ? 1.0 : sin(M_PI * 0.96 * x) / (M_PI * 0.96 * x)) * window);
			sum += phases[row][tap];
		}
		for (tap = 0; tap < RESAMPLER_TAPS; tap++)
			phases[row][tap] = (float)(phases[row][tap] / sum);
	}
}

/* a voice: its sound, the state of the real bus voice and the reference voice's */
struct voice
{
	float *data;
	unsigned long frames, cursor;
	int channels;
	unsigned rate;
	int start_callback;
	struct audio_voice state;
	float current_left, current_right;
};

static int take(void *source, float *frame)
{
	struct voice *voice = source;

	if (voice->cursor >= voice->frames)
		return 0;
	frame[0] = voice->data[voice->cursor * voice->channels];
	frame[1] = voice->data[voice->cursor * voice->channels + voice->channels - 1];
	voice->cursor++;
	return 1;
}

static float reference_gain_error;

/* the per-voice path, a gain ramped in closed form */
static void reference_mix(struct voice *voice, float *output, unsigned long frames, float target_left, float target_right)
{
	struct audio_voice *state = &voice->state;
	double step = (double)voice->rate / 48000.0;
	float ramp_left = (target_left - voice->current_left) / (float)frames, ramp_right = (target_right - voice->current_right) / (float)frames;
	float accumulated_left = voice->current_left;
	unsigned long frame;

	for (frame = 0; frame < frames; frame++)
	{
		double position = state->phase * TABLE_STEPS;
		unsigned long row = (unsigned long)position;
		float blend = (float)(position - (double)row), sample_left, sample_right;
		float left = voice->current_left + (float)frame * ramp_left, right = voice->current_right + (float)frame * ramp_right;

		while ((long)(state->history_count - state->center) <= RESAMPLER_ZERO_CROSSINGS)
		{
			float *slot = state->history[state->history_count % RESAMPLER_HISTORY];

			if (!take(voice, slot))
				slot[0] = slot[1] = 0.0f;
			state->history_count++;
		}
		audio_filter_reference(state->history, state->center + 1 - RESAMPLER_ZERO_CROSSINGS, phases[row], phases[row + 1], blend,
			&sample_left, &sample_right);
		if (voice->channels == 1)
		{
			output[frame * 2] += sample_left * left;
			output[frame * 2 + 1] += sample_left * right;
		}
		else
		{
			output[frame * 2] += sample_left * left;
			output[frame * 2 + 1] += sample_right * right;
		}
		if (fabsf(accumulated_left - left) > reference_gain_error)
			reference_gain_error = fabsf(accumulated_left - left);
		accumulated_left += ramp_left;
		state->phase += step;
		while (state->phase >= 1.0)
		{
			state->phase -= 1.0;
			state->center++;
		}
	}
	voice->current_left = target_left;
	voice->current_right = target_right;
}

int main(int argc, char **argv)
{
	static struct audio_bus buses[2];
	static struct voice voices[VOICES], references[VOICES];
	static float reference_output[1024 * 2], bus_output[1024 * 2];
	static const unsigned long sizes[] = { 512, 480, 1024, 1, 512, 512, 480, 512 };
	unsigned long long clock = 0;
	int callback, index;
	float worst = 0.0f;

	if (argc > 1 && !strcmp(argv[1], "far"))
		clock = (1ULL << 34) - 700; /* the mixer's clock past 2^32 frames (about 25 hours), crossed during the run */
	else if (argc > 1)
		mutation = argv[1];
	make_phases();
	audio_bus_initialize(&buses[0], 147, 320);
	audio_bus_initialize(&buses[1], 147, 160);
	for (index = 0; index < VOICES; index++)
	{
		struct voice *voice = &voices[index];
		unsigned long length = 400 + (unsigned long)(fabsf(random_unit()) * 2600.0f);
		unsigned long item;

		voice->rate = (index % 3 == 2) ? 44100 : 22050;
		voice->channels = (index % 4 == 1 || index == 6) ? 2 : 1;
		voice->frames = length;
		voice->start_callback = index < 3 ? 0 : (int)(fabsf(random_unit()) * 20.0f);
		voice->data = malloc(length * voice->channels * sizeof(float));
		for (item = 0; item < length * voice->channels; item++)
			voice->data[item] = random_unit() * (item % 2 ? 0.7f : 1.0f);
		audio_voice_reset(&voice->state);
	}
	memcpy(references, voices, sizeof(voices));
	for (callback = 0; callback < CALLBACKS; callback++)
	{
		unsigned long frames = sizes[callback % (sizeof(sizes) / sizeof(sizes[0]))];
		unsigned long frame;
		float gain_sum = 0.0f;

		memset(reference_output, 0, sizeof(reference_output));
		memset(bus_output, 0, sizeof(bus_output));
		audio_bus_begin(&buses[0], clock, frames);
		audio_bus_begin(&buses[1], clock, frames);
		for (index = 0; index < VOICES; index++)
		{
			float target_left = random_unit() * 0.5f + 0.5f * (callback % 5 == 3 ? -1.0f : 1.0f), target_right = random_unit() * 0.5f + 0.4f;
			struct voice *voice = &voices[index], *reference = &references[index];

			if (callback < voice->start_callback)
				continue;
			if (callback == voice->start_callback)
			{
				/* the bus starts the voice on the next grid frame; the reference starts at that fraction */
				struct audio_bus *bus = &buses[voice->rate == 44100];
				unsigned fraction;

				audio_bus_frame(bus, clock, &fraction);
				reference->state.center = AUDIO_VOICE_LEAD - (fraction ? 1 : 0);
				reference->state.phase = (double)fraction / (double)bus->denominator;
			}
			if (callback == voice->start_callback + 2)
			{
				/* a ramp across zero, and the gains held */
				target_left = -0.3f;
				voice->current_left = reference->current_left = 0.4f;
			}
			reference_mix(reference, reference_output, frames, target_left, target_right);
			{
				struct audio_bus *bus = &buses[voice->rate == 44100];
				float ramp_left = (target_left - voice->current_left) / (float)frames, ramp_right = (target_right - voice->current_right) / (float)frames;

				CHECK(audio_bus_voice_prepare(&voice->state, bus, clock), "voice %d is not on the bus at callback %d", index, callback);
				if (!strcmp(mutation, "late_start") && callback == voice->start_callback)
					voice->state.bus_offset++;
				if (!strcmp(mutation, "no_ramp"))
					ramp_left = ramp_right = 0.0f;
				if (!strcmp(mutation, "gain_first"))
				{
					voice->current_left += ramp_left * (float)(frames / 2);
					voice->current_right += ramp_right * (float)(frames / 2);
					ramp_left = ramp_right = 0.0f;
				}
				audio_bus_mix_voice(&voice->state, bus, clock, frames, voice->current_left, voice->current_right, ramp_left, ramp_right,
					RESAMPLER_ZERO_CROSSINGS, take, voice);
				voice->current_left = target_left;
				voice->current_right = target_right;
			}
			gain_sum += fabsf(target_left) + fabsf(target_right);
		}
		audio_bus_finish(&buses[0], clock, frames, phases, TABLE_STEPS, bus_output);
		audio_bus_finish(&buses[1], clock, frames, phases, TABLE_STEPS, bus_output);
		for (frame = 0; frame < frames * 2; frame++)
		{
			float difference = fabsf(bus_output[frame] - reference_output[frame]);

			if (difference > worst)
				worst = difference;
			CHECK(difference <= AUDIO_FILTER_FAST_BOUND * (1.0f + gain_sum) * 2.0f, "callback %d frame %lu: bus %g reference %g", callback, frame,
				bus_output[frame], reference_output[frame]);
		}
		clock += frames;
	}
	/* today's ramp, left += ramp every frame, drifts from the closed form by rounding only */
	CHECK(reference_gain_error <= 1024.0f * 6.0e-8f, "accumulated gain ramp differs from the closed form by %g", reference_gain_error);
	printf("audio rate bus probe%s%s: worst difference %.3g, ramp drift %.3g, %d failures\n", *mutation ? " (mutation " : "", *mutation ? mutation : "",
		worst, reference_gain_error, failures);
	return failures ? 1 : 0;
}
