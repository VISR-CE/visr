/* audio_filter.h: the fast filter agrees with the reference one within AUDIO_FILTER_FAST_BOUND at every history
position (wrapped or not), phase and blend; a mono voice's two channels are the same bits, and the bits of a stereo
voice whose channels are equal; and the check can tell a wrong answer from a rounding difference. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_filter.h"

/* strict C11 on Linux does not define M_PI */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned state = 12345u;
static float random_unit(void)
{
	state = state * 1664525u + 1013904223u;
	return (float)(state >> 8) / (float)(1u << 23) - 1.0f; /* -1 to 1 */
}

static float history[RESAMPLER_HISTORY][2];
static float mono_history[RESAMPLER_HISTORY][2];
static float rows[4][RESAMPLER_TAPS];

static int failures;
#define CHECK(condition, ...) do { if (!(condition)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

/* weights shaped as the mixer's: a windowed sinc around the middle tap, summing to 1 */
static void make_row(float *row, float offset)
{
	double sum = 0.0;
	int tap;

	for (tap = 0; tap < RESAMPLER_TAPS; tap++)
	{
		double x = (double)(tap + 1 - RESAMPLER_ZERO_CROSSINGS) - offset;
		double window = 0.5 + 0.5 * cos(M_PI * x / RESAMPLER_ZERO_CROSSINGS);

		row[tap] = (float)((fabs(x) < 1e-9 ? 1.0 : sin(M_PI * 0.96 * x) / (M_PI * 0.96 * x)) * window);
		sum += row[tap];
	}
	for (tap = 0; tap < RESAMPLER_TAPS; tap++)
		row[tap] = (float)(row[tap] / sum);
}

int main(void)
{
	float worst = 0.0f;
	unsigned long shifted_cases = 0, shifted_caught = 0;
	unsigned long first;
	int round, row, index;

	for (row = 0; row < 4; row++)
		make_row(rows[row], row * 0.25);
	for (round = 0; round < 40; round++)
	{
		for (index = 0; index < RESAMPLER_HISTORY; index++)
		{
			history[index][0] = random_unit();
			history[index][1] = random_unit();
			mono_history[index][0] = mono_history[index][1] = history[index][0];
			if (round % 4 == 0) /* full-scale squares, the worst for rounding */
				history[index][0] = history[index][1] = mono_history[index][0] = mono_history[index][1] = (random_unit() < 0 ? -1.0f : 1.0f);
		}
		for (first = 0; first < RESAMPLER_HISTORY + 3; first++)
			for (row = 0; row < 3; row++)
			{
				float blend = (float)(round % 8) / 8.0f;
				float reference_left, reference_right, fast_left, fast_right, mono_left, mono_right, equal_left, equal_right;
				float reference_serial = 0.0f, shifted_left, shifted_right, off_left, off_right;
				int tap;

				audio_filter_voice(0, history, first, rows[row], rows[row + 1], blend, 1, &reference_left, &reference_right);
				audio_filter_voice(1, history, first, rows[row], rows[row + 1], blend, 1, &fast_left, &fast_right);
				audio_filter_voice(1, mono_history, first, rows[row], rows[row + 1], blend, 0, &mono_left, &mono_right);
				audio_filter_voice(1, mono_history, first, rows[row], rows[row + 1], blend, 1, &equal_left, &equal_right);
				audio_filter_voice(0, mono_history, first, rows[row], rows[row + 1], blend, 0, &off_left, &off_right);
				/* the reference sums the taps of the history where the mixer reads them (to a rounding: the compiler may fuse) */
				for (tap = 0; tap < RESAMPLER_TAPS; tap++)
				{
					float weight = rows[row][tap] + (rows[row + 1][tap] - rows[row][tap]) * blend;

					reference_serial += history[(first + (unsigned long)tap) % RESAMPLER_HISTORY][0] * weight;
				}
				CHECK(fabsf(reference_serial - reference_left) <= 1.0e-6f, "reference left differs from the plain loop at %lu", first);
				CHECK(fabsf(fast_left - reference_left) <= AUDIO_FILTER_FAST_BOUND, "fast left %g vs %g at %lu", fast_left, reference_left, first);
				CHECK(fabsf(fast_right - reference_right) <= AUDIO_FILTER_FAST_BOUND, "fast right %g vs %g at %lu", fast_right, reference_right, first);
				CHECK(memcmp(&mono_left, &mono_right, sizeof(float)) == 0, "mono channels differ at %lu", first);
				CHECK(memcmp(&mono_left, &equal_left, sizeof(float)) == 0 && memcmp(&mono_left, &equal_right, sizeof(float)) == 0,
					"mono is not the stereo bits of equal channels at %lu", first);
				/* the setting off: a mono voice's channels are the reference's, two of one value */
				CHECK(memcmp(&off_left, &off_right, sizeof(float)) == 0, "off: mono channels differ at %lu", first);
				CHECK(fabsf(mono_left - off_left) <= AUDIO_FILTER_FAST_BOUND, "mono fast %g vs off %g at %lu", mono_left, off_left, first);
				if (fabsf(fast_left - reference_left) > worst)
					worst = fabsf(fast_left - reference_left);
				/* one frame off: a wrong answer, far outside the bound */
				audio_filter_voice(1, history, first + 1, rows[row], rows[row + 1], blend, 1, &shifted_left, &shifted_right);
				shifted_cases++;
				if (fabsf(shifted_left - fast_left) > 1000.0f * AUDIO_FILTER_FAST_BOUND)
					shifted_caught++;
			}
	}
	/* the shifted frames of random data differ by far more than a rounding in nearly every case */
	CHECK(shifted_caught * 10 >= shifted_cases * 9, "a shifted filter passes the bound in %lu of %lu cases", shifted_cases - shifted_caught, shifted_cases);
	printf("audio filter probe: worst fast difference %.3g (bound %.3g), %d failures\n", worst, (double)AUDIO_FILTER_FAST_BOUND, failures);
	return failures ? 1 : 0;
}
