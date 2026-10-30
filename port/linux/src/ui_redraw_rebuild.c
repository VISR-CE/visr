/*
UI_REDRAW_REBUILD.C

A picture of the menus rebuilt at a larger size (ui_redraw.h): smooth where
the picture is smooth, and with its edges drawn again about one output pixel
wide where it has edges. No state; the menus' redraw (ui_redraw.c) and its test
(port/ios/tests/ui_redraw_probe.c) call it.
*/

#include "ui_redraw.h"

#include <math.h>
#include <stdlib.h>

/* contrast (0 to 1, of a channel, premultiplied) below which the four texels
around a point are a fill, a gradient, a faint bevel or compression noise,
kept smooth, and above which (by half as much again) they are an edge, fully
sharpened */
#define EDGE_CONTRAST 0.15f

static float clamp01(float value)
{
	return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

/* Catmull-Rom weights for a point t (0 to 1) past the second of four samples */
static void cubic_weights(float t, float weights[4])
{
	float t2 = t * t, t3 = t2 * t;

	weights[0] = 0.5f * (-t3 + 2.0f * t2 - t);
	weights[1] = 0.5f * (3.0f * t3 - 5.0f * t2 + 2.0f);
	weights[2] = 0.5f * (-3.0f * t3 + 4.0f * t2 + t);
	weights[3] = 0.5f * (t3 - t2);
}

static long clamp_index(long value, long count)
{
	return value < 0 ? 0 : value >= count ? count - 1 : value;
}

int ui_redraw_rebuild(const unsigned char *source, long width, long height, long channels,
	long output_width, long output_height, unsigned char *output)
{
	float *premultiplied;
	float scale_x, scale_y, scale;
	long x, y, index, channel;

	if (!source || !output || width <= 0 || height <= 0 || output_width < width || output_height < height ||
		channels != 4)
		return 0;
	scale_x = (float)output_width / width;
	scale_y = (float)output_height / height;
	scale = scale_x < scale_y ? scale_x : scale_y;
	premultiplied = malloc((size_t)width * height * 4 * sizeof(*premultiplied));
	if (!premultiplied)
		return 0;
	/* (four channels each, premultiplied for filtering) */
	for (index = 0; index < width * height; index++)
	{
		float alpha = source[index * 4 + 3] / 255.0f;

		for (channel = 0; channel < 3; channel++)
			premultiplied[index * 4 + channel] = (source[index * 4 + channel] / 255.0f) * alpha;
		premultiplied[index * 4 + 3] = alpha;
	}
	for (y = 0; y < output_height; y++)
	{
		float source_y = (y + 0.5f) / scale_y - 0.5f;
		long y0 = (long)floorf(source_y);
		float weights_y[4];

		cubic_weights(source_y - y0, weights_y);
		for (x = 0; x < output_width; x++)
		{
			float source_x = (x + 0.5f) / scale_x - 0.5f;
			long x0 = (long)floorf(source_x);
			float weights_x[4], smooth[4] = { 0 }, result[4];
			const float *corners[4];
			long low = 0, high = 0, driver = 0, row, column, corner;
			float range = 0.0f;

			cubic_weights(source_x - x0, weights_x);
			for (row = 0; row < 4; row++)
			{
				const float *line = premultiplied + clamp_index(y0 - 1 + row, height) * width * 4;

				for (column = 0; column < 4; column++)
				{
					const float *texel = line + clamp_index(x0 - 1 + column, width) * 4;
					float weight = weights_y[row] * weights_x[column];

					for (channel = 0; channel < 4; channel++)
						smooth[channel] += texel[channel] * weight;
				}
			}
			/* the four texels around the point, and the channel that differs
			most among them */
			for (corner = 0; corner < 4; corner++)
			{
				corners[corner] = premultiplied + (clamp_index(y0 + corner / 2, height) * width +
					clamp_index(x0 + corner % 2, width)) * 4;
			}
			for (channel = 0; channel < 4; channel++)
			{
				float minimum = corners[0][channel], maximum = minimum;

				for (corner = 1; corner < 4; corner++)
				{
					if (corners[corner][channel] < minimum)
						minimum = corners[corner][channel];
					if (corners[corner][channel] > maximum)
						maximum = corners[corner][channel];
				}
				/* (no ringing: the smooth value stays between the four
				texels around it, which a cubic beside an edge overshoots) */
				if (smooth[channel] < minimum)
					smooth[channel] = minimum;
				if (smooth[channel] > maximum)
					smooth[channel] = maximum;
				if (maximum - minimum > range)
				{
					range = maximum - minimum;
					driver = channel;
				}
			}
			for (corner = 1; corner < 4; corner++)
			{
				if (corners[corner][driver] < corners[low][driver])
					low = corner;
				if (corners[corner][driver] > corners[high][driver])
					high = corner;
			}
			for (channel = 0; channel < 4; channel++)
				result[channel] = clamp01(smooth[channel]);
			if (range > EDGE_CONTRAST)
			{
				/* on an edge: where the smooth value falls between the darkest
				and brightest texel, sharpened to about one output pixel (the
				fraction changes by about 1/scale from one output pixel to the
				next across an edge) */
				float fraction = clamp01((smooth[driver] - corners[low][driver]) / range);
				float half_width = 0.5f / scale;
				float edge = clamp01((fraction - (0.5f - half_width)) / (2.0f * half_width));
				float amount = clamp01((range - EDGE_CONTRAST) / (0.5f * EDGE_CONTRAST));

				for (channel = 0; channel < 4; channel++)
				{
					float sharp = corners[low][channel] + (corners[high][channel] - corners[low][channel]) * edge;

					result[channel] = result[channel] + (sharp - result[channel]) * amount;
				}
			}
			/* (premultiplied: no color brighter than its alpha) */
			for (channel = 0; channel < 3; channel++)
			{
				if (result[channel] > result[3])
					result[channel] = result[3];
			}
			index = y * output_width + x;
			if (result[3] * 255.0f >= 0.5f)
			{
				for (channel = 0; channel < 3; channel++)
					output[index * 4 + channel] = (unsigned char)(clamp01(result[channel] / result[3]) * 255.0f + 0.5f);
				output[index * 4 + 3] = (unsigned char)(result[3] * 255.0f + 0.5f);
			}
			else
			{
				/* clear: the color of the nearest texel of the four that has
				one (the pictures keep their edge's color under clear texels,
				so that filtering does not darken the edge) */
				const unsigned char *nearest = source + (clamp_index((long)floorf(source_y + 0.5f), height) * width +
					clamp_index((long)floorf(source_x + 0.5f), width)) * 4;

				output[index * 4 + 0] = nearest[0];
				output[index * 4 + 1] = nearest[1];
				output[index * 4 + 2] = nearest[2];
				output[index * 4 + 3] = 0;
			}
		}
	}
	free(premultiplied);
	return 1;
}
