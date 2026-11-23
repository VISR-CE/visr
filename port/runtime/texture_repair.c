/* Apache-2.0. See texture_repair.h. */
#include "texture_repair.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Two stored steps in 8-bit levels, per channel (R, G, B): 2 * 255 / 31 = 16.5 and 2 * 255 / 63 = 8.1, rounded up. */
static const int flag_range[3] = { 17, 9, 17 };
/* One stored step, in 8-bit levels */
static const double step_levels[3] = { 255.0 / 31.0, 255.0 / 63.0, 255.0 / 31.0 };
static const unsigned channel_bits[3] = { 5, 6, 5 };

unsigned texture_repair_expand(unsigned bits, unsigned stored)
{
	return bits == 5 ? (stored << 3) | (stored >> 2) : (stored << 2) | (stored >> 4);
}

void texture_repair_rounding_interval(unsigned bits, unsigned stored, int *low, int *high)
{
	const int m = (1 << bits) - 1, s = (int)stored;
	int v;

	/* round(v * m / 255) == s, ties excluded: (2s - 1) * 255 < 2 v m < (2s + 1) * 255 */
	for (v = 0; v <= 255 && 2 * v * m <= (2 * s - 1) * 255; v++)
		;
	*low = v;
	for (v = 255; v >= 0 && 2 * v * m >= (2 * s + 1) * 255; v--)
		;
	*high = v;
}

/* ---------- the shared core */

struct plane
{
	unsigned width, height;
	int wrap;
	const uint8_t *rgba;   /* the plain decode */
	uint8_t *low, *high;   /* per texel, per channel: the allowed 8-bit range */
};

static int opaque(const struct plane *p, size_t texel)
{
	return p->rgba[texel * 4 + 3] == 255;
}

static int neighbor(const struct plane *p, int x, int y, int dx, int dy, size_t *texel)
{
	int nx = x + dx, ny = y + dy;

	if (nx < 0 || nx >= (int)p->width)
	{
		if (!p->wrap)
			return 0;
		nx = (nx + (int)p->width) % (int)p->width;
	}
	if (ny < 0 || ny >= (int)p->height)
	{
		if (!p->wrap)
			return 0;
		ny = (ny + (int)p->height) % (int)p->height;
	}
	*texel = (size_t)ny * p->width + (size_t)nx;
	return 1;
}

/* the source index along one axis for offset d from position i, or -1 outside a level that does not tile */
static int axis_index(int i, int d, int size, int wrap)
{
	int n = i + d;

	if (n >= 0 && n < size)
		return n;
	return wrap ? (n % size + size) % size : -1;
}

/* flags[t] = 1 for an opaque texel whose neighborhood spans at most two stored steps in every channel. The window
minimum and maximum come from a pass along rows and then one along columns (a transparent texel counts for neither). */
static int compute_flags(const struct plane *p, uint8_t *flags)
{
	const int radius = TEXTURE_REPAIR_WINDOW_RADIUS, width = (int)p->width, height = (int)p->height;
	const size_t count = (size_t)width * height;
	uint8_t *row_min = malloc(count * 3), *row_max = malloc(count * 3);
	int x, y, d, channel;

	if (!row_min || !row_max)
	{
		free(row_min);
		free(row_max);
		return -1;
	}
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			for (channel = 0; channel < 3; channel++)
			{
				int low = 255, high = 0;

				for (d = -radius; d <= radius; d++)
				{
					int n = axis_index(x, d, width, p->wrap);
					const uint8_t *texel;

					if (n < 0)
						continue;
					texel = p->rgba + ((size_t)y * width + (size_t)n) * 4;
					if (texel[3] != 255)
						continue;
					if (texel[channel] < low)
						low = texel[channel];
					if (texel[channel] > high)
						high = texel[channel];
				}
				row_min[((size_t)y * width + x) * 3 + channel] = (uint8_t)low;
				row_max[((size_t)y * width + x) * 3 + channel] = (uint8_t)high;
			}
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			size_t center = (size_t)y * width + (size_t)x;
			int ok = opaque(p, center);

			for (channel = 0; ok && channel < 3; channel++)
			{
				int low = 255, high = 0;

				for (d = -radius; d <= radius; d++)
				{
					int n = axis_index(y, d, height, p->wrap);
					size_t at;

					if (n < 0)
						continue;
					at = ((size_t)n * width + (size_t)x) * 3 + channel;
					if (row_min[at] < low)
						low = row_min[at];
					if (row_max[at] > high)
						high = row_max[at];
				}
				if (high - low > flag_range[channel])
					ok = 0;
			}
			flags[center] = (uint8_t)ok;
		}
	free(row_min);
	free(row_max);
	return 0;
}

/* whether two texels may blend: both opaque, and within two steps in every channel */
static int may_blend(const struct plane *p, size_t center, size_t t)
{
	int channel;

	if (!opaque(p, t) || !opaque(p, center))
		return 0;
	for (channel = 0; channel < 3; channel++)
		if (abs((int)p->rgba[t * 4 + channel] - (int)p->rgba[center * 4 + channel]) > flag_range[channel])
			return 0;
	return 1;
}

/* links[t] bit 0: t may blend with the texel to its right; bit 1: with the texel below (both texels' own flags are
tested when blending) */
static void compute_links(const struct plane *p, uint8_t *links)
{
	int x, y;

	for (y = 0; y < (int)p->height; y++)
		for (x = 0; x < (int)p->width; x++)
		{
			size_t center = (size_t)y * p->width + (size_t)x, t;

			links[center] = 0;
			if (neighbor(p, x, y, 1, 0, &t) && may_blend(p, center, t))
				links[center] |= 1;
			if (neighbor(p, x, y, 0, 1, &t) && may_blend(p, center, t))
				links[center] |= 2;
		}
}

/* one pass of a three-tap mean along rows (horizontal) or columns, over flagged texels, joined only by links */
static void smooth_pass(const struct plane *p, const uint8_t *flags, const uint8_t *links, const float *from, float *to,
	int horizontal)
{
	const int width = (int)p->width, height = (int)p->height, bit = horizontal ? 1 : 2;
	int x, y, channel;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			size_t center = (size_t)y * width + (size_t)x, before, after;
			float sum[3];
			int count = 1, has_before, has_after;

			for (channel = 0; channel < 3; channel++)
				sum[channel] = from[center * 3 + channel];
			if (!flags[center])
			{
				for (channel = 0; channel < 3; channel++)
					to[center * 3 + channel] = sum[channel];
				continue;
			}
			if (horizontal)
			{
				before = (size_t)y * width + (size_t)(x ? x - 1 : width - 1);
				after = (size_t)y * width + (size_t)(x + 1 < width ? x + 1 : 0);
			}
			else
			{
				before = (size_t)(y ? y - 1 : height - 1) * width + (size_t)x;
				after = (size_t)(y + 1 < height ? y + 1 : 0) * width + (size_t)x;
			}
			/* (a link across the edge exists only when the level tiles) */
			has_before = (links[before] & bit) != 0 && (horizontal ? (x > 0 || p->wrap) : (y > 0 || p->wrap));
			has_after = (links[center] & bit) != 0;
			if (has_before)
			{
				for (channel = 0; channel < 3; channel++)
					sum[channel] += from[before * 3 + channel];
				count++;
			}
			if (has_after)
			{
				for (channel = 0; channel < 3; channel++)
					sum[channel] += from[after * 3 + channel];
				count++;
			}
			for (channel = 0; channel < 3; channel++)
				to[center * 3 + channel] = sum[channel] / (float)count;
		}
}

static int stepped(const struct plane *p, size_t a, size_t b)
{
	int channel;

	for (channel = 0; channel < 3; channel++)
	{
		double difference = fabs((double)p->rgba[a * 4 + channel] - (double)p->rgba[b * 4 + channel]);

		if (difference >= 0.75 * step_levels[channel] && difference <= 1.25 * step_levels[channel])
			return 1;
	}
	return 0;
}

static int repair(const struct plane *p, uint8_t *out, struct texture_repair_stats *stats)
{
	const size_t count = (size_t)p->width * p->height;
	uint8_t *flags = calloc(count ? count : 1, 1), *links = calloc(count ? count : 1, 1);
	float *a = malloc((count ? count : 1) * 3 * sizeof(float)), *b = malloc((count ? count : 1) * 3 * sizeof(float));
	struct texture_repair_stats local = { 0, 0, 0, 0 };
	size_t texel;
	int pass, channel, x, y;

	if (!flags || !links || !a || !b)
	{
		free(flags);
		free(links);
		free(a);
		free(b);
		return -1;
	}
	if (compute_flags(p, flags))
	{
		free(flags);
		free(links);
		free(a);
		free(b);
		return -1;
	}
	compute_links(p, links);
	for (texel = 0; texel < count; texel++)
	{
		for (channel = 0; channel < 3; channel++)
			a[texel * 3 + channel] = (float)p->rgba[texel * 4 + channel];
		local.texels += opaque(p, texel);
		local.flagged += flags[texel];
	}
	for (y = 0; y < (int)p->height; y++)
		for (x = 0; x < (int)p->width; x++)
		{
			size_t t, here = (size_t)y * p->width + (size_t)x;

			if (!flags[here])
				continue;
			if (neighbor(p, x, y, 1, 0, &t) && flags[t] && stepped(p, here, t))
				local.stepped_pairs++;
			if (neighbor(p, x, y, 0, 1, &t) && flags[t] && stepped(p, here, t))
				local.stepped_pairs++;
		}
	for (pass = 0; pass < TEXTURE_REPAIR_PASSES; pass++)
	{
		smooth_pass(p, flags, links, a, b, 1);
		smooth_pass(p, flags, links, b, a, 0);
	}
	memcpy(out, p->rgba, count * 4);
	for (texel = 0; texel < count; texel++)
	{
		if (!flags[texel])
			continue;
		for (channel = 0; channel < 3; channel++)
		{
			int v = (int)floorf(a[texel * 3 + channel] + 0.5f), lo = p->low[texel * 3 + channel], hi = p->high[texel * 3 + channel];

			v = v < lo ? lo : v > hi ? hi : v;
			out[texel * 4 + channel] = (uint8_t)v;
		}
		if (memcmp(out + texel * 4, p->rgba + texel * 4, 3))
			local.changed++;
	}
	free(flags);
	free(links);
	free(a);
	free(b);
	if (stats)
		*stats = local;
	return 0;
}

/* ---------- R5G6B5 */

static int stored_of(const uint8_t *rgba, int channel, unsigned *stored)
{
	unsigned v = *rgba, s = channel == 1 ? v >> 2 : v >> 3;

	*stored = s;
	return texture_repair_expand(channel_bits[channel], s) == v;
}

int texture_repair_r5g6b5(const uint8_t *rgba, unsigned width, unsigned height, int wrap, uint8_t *out,
	struct texture_repair_stats *stats)
{
	struct plane p;
	const size_t count = (size_t)width * height;
	size_t texel;
	int channel, result = -1;
	uint8_t *low, *high;

	if (!rgba || !out || !width || !height)
		return -1;
	low = malloc(count * 3);
	high = malloc(count * 3);
	if (!low || !high)
		goto done;
	for (texel = 0; texel < count; texel++)
	{
		if (rgba[texel * 4 + 3] != 255)
			goto done;
		for (channel = 0; channel < 3; channel++)
		{
			unsigned stored;
			int l, h;

			if (!stored_of(rgba + texel * 4 + channel, channel, &stored))
				goto done;
			texture_repair_rounding_interval(channel_bits[channel], stored, &l, &h);
			low[texel * 3 + channel] = (uint8_t)l;
			high[texel * 3 + channel] = (uint8_t)h;
		}
	}
	p.width = width;
	p.height = height;
	p.wrap = wrap;
	p.rgba = rgba;
	p.low = low;
	p.high = high;
	result = repair(&p, out, stats);
done:
	free(low);
	free(high);
	return result;
}

/* exact rational rounding of a 8-bit value to a depth: the stored value, or -1 on a tie */
static int round_to_depth(unsigned bits, int value)
{
	const int m = (1 << bits) - 1, numerator = 2 * value * m + 255;

	return numerator % 510 == 0 ? -1 : numerator / 510;
}

long texture_repair_verify_r5g6b5(const uint8_t *original_rgba, const uint8_t *repaired_rgba, unsigned width,
	unsigned height, int wrap)
{
	struct plane p;
	size_t count = (size_t)width * height, texel;
	long violations = 0;
	uint8_t *flags;
	int channel;

	if (!original_rgba || !repaired_rgba || !width || !height)
		return -1;
	flags = calloc(count, 1);
	if (!flags)
		return -1;
	memset(&p, 0, sizeof(p));
	p.width = width;
	p.height = height;
	p.wrap = wrap;
	p.rgba = original_rgba;
	if (compute_flags(&p, flags))
	{
		free(flags);
		return -1;
	}
	for (texel = 0; texel < count; texel++)
	{
		int bad = 0;

		for (channel = 0; channel < 3; channel++)
		{
			unsigned stored = channel == 1 ? original_rgba[texel * 4 + channel] >> 2 : original_rgba[texel * 4 + channel] >> 3;

			if (flags[texel])
				bad |= round_to_depth(channel_bits[channel], repaired_rgba[texel * 4 + channel]) != (int)stored;
		}
		if (!flags[texel])
			bad |= memcmp(original_rgba + texel * 4, repaired_rgba + texel * 4, 4) != 0;
		bad |= repaired_rgba[texel * 4 + 3] != original_rgba[texel * 4 + 3];
		violations += bad;
	}
	free(flags);
	return violations;
}

/* ---------- DXT1 */

static unsigned expand565(unsigned value, int channel)
{
	return channel == 0 ? texture_repair_expand(5, value >> 11) : channel == 1 ? texture_repair_expand(6, (value >> 5) & 63) :
		texture_repair_expand(5, value & 31);
}

/* one block's four palette colors (8-bit RGB) and whether it has the punch-through texel */
static int block_palette(const uint8_t *block, int palette[4][3], double spacing[3])
{
	unsigned c0 = block[0] | (block[1] << 8), c1 = block[2] | (block[3] << 8);
	int channel, four = c0 > c1;

	for (channel = 0; channel < 3; channel++)
	{
		int e0 = (int)expand565(c0, channel), e1 = (int)expand565(c1, channel);

		palette[0][channel] = e0;
		palette[1][channel] = e1;
		if (four)
		{
			palette[2][channel] = (e0 * 2 + e1) / 3;
			palette[3][channel] = (e0 + e1 * 2) / 3;
			spacing[channel] = abs(e0 - e1) / 3.0;
		}
		else
		{
			palette[2][channel] = (e0 + e1) / 2;
			palette[3][channel] = 0;
			spacing[channel] = abs(e0 - e1) / 2.0;
		}
	}
	return four;
}

void texture_repair_dxt1_decode(const uint8_t *blocks, unsigned width, unsigned height, uint8_t *out)
{
	const unsigned blocks_x = (width + 3) / 4, blocks_y = (height + 3) / 4;
	unsigned bx, by, i;

	for (by = 0; by < blocks_y; by++)
		for (bx = 0; bx < blocks_x; bx++)
		{
			const uint8_t *block = blocks + ((size_t)by * blocks_x + bx) * 8;
			int palette[4][3], channel;
			double spacing[3];
			unsigned long bits = block[4] | (block[5] << 8) | ((unsigned long)block[6] << 16) | ((unsigned long)block[7] << 24);
			int four = block_palette(block, palette, spacing);

			for (i = 0; i < 16; i++)
			{
				unsigned x = bx * 4 + (i & 3), y = by * 4 + (i >> 2), index = (bits >> (i * 2)) & 3;
				uint8_t *texel;

				if (x >= width || y >= height)
					continue;
				texel = out + ((size_t)y * width + x) * 4;
				for (channel = 0; channel < 3; channel++)
					texel[channel] = (uint8_t)palette[index][channel];
				texel[3] = !four && index == 3 ? 0 : 255;
			}
		}
}

/* the limit for one channel: strictly within max(half the spacing, half the endpoint step) of the palette value */
static void dxt1_limit(int value, double spacing, int channel, int *low, int *high)
{
	const double limit = fmax(spacing / 2.0, step_levels[channel] / 2.0);
	int l = (int)floor((double)value - limit) + 1, h = (int)ceil((double)value + limit) - 1;

	*low = l < 0 ? 0 : l > value ? value : l;
	*high = h > 255 ? 255 : h < value ? value : h;
}

int texture_repair_dxt1(const uint8_t *blocks, unsigned width, unsigned height, int wrap, uint8_t *out,
	struct texture_repair_stats *stats)
{
	const unsigned blocks_x = (width + 3) / 4, blocks_y = (height + 3) / 4;
	const size_t count = (size_t)width * height;
	uint8_t *plain, *low, *high;
	struct plane p;
	unsigned bx, by, i;
	int result;

	if (!blocks || !out || !width || !height)
		return -1;
	plain = malloc(count * 4);
	low = calloc(count, 3);
	high = calloc(count, 3);
	if (!plain || !low || !high)
	{
		free(plain);
		free(low);
		free(high);
		return -1;
	}
	texture_repair_dxt1_decode(blocks, width, height, plain);
	for (by = 0; by < blocks_y; by++)
		for (bx = 0; bx < blocks_x; bx++)
		{
			const uint8_t *block = blocks + ((size_t)by * blocks_x + bx) * 8;
			int palette[4][3], channel;
			double spacing[3];
			unsigned long bits = block[4] | (block[5] << 8) | ((unsigned long)block[6] << 16) | ((unsigned long)block[7] << 24);

			block_palette(block, palette, spacing);
			for (i = 0; i < 16; i++)
			{
				unsigned x = bx * 4 + (i & 3), y = by * 4 + (i >> 2), index = (bits >> (i * 2)) & 3;
				size_t texel;

				if (x >= width || y >= height)
					continue;
				texel = (size_t)y * width + x;
				for (channel = 0; channel < 3; channel++)
				{
					int l, h;

					dxt1_limit(palette[index][channel], spacing[channel], channel, &l, &h);
					low[texel * 3 + channel] = (uint8_t)l;
					high[texel * 3 + channel] = (uint8_t)h;
				}
			}
		}
	p.width = width;
	p.height = height;
	p.wrap = wrap;
	p.rgba = plain;
	p.low = low;
	p.high = high;
	result = repair(&p, out, stats);
	free(plain);
	free(low);
	free(high);
	return result;
}

long texture_repair_verify_dxt1(const uint8_t *blocks, const uint8_t *repaired_rgba, unsigned width, unsigned height,
	int wrap)
{
	const unsigned blocks_x = (width + 3) / 4, blocks_y = (height + 3) / 4;
	const size_t count = (size_t)width * height;
	uint8_t *plain, *flags;
	struct plane p;
	unsigned bx, by, i;
	long violations = 0;

	if (!blocks || !repaired_rgba || !width || !height)
		return -1;
	plain = malloc(count * 4);
	flags = calloc(count, 1);
	if (!plain || !flags)
	{
		free(plain);
		free(flags);
		return -1;
	}
	texture_repair_dxt1_decode(blocks, width, height, plain);
	memset(&p, 0, sizeof(p));
	p.width = width;
	p.height = height;
	p.wrap = wrap;
	p.rgba = plain;
	if (compute_flags(&p, flags))
	{
		free(plain);
		free(flags);
		return -1;
	}
	for (by = 0; by < blocks_y; by++)
		for (bx = 0; bx < blocks_x; bx++)
		{
			const uint8_t *block = blocks + ((size_t)by * blocks_x + bx) * 8;
			unsigned c0 = block[0] | (block[1] << 8), c1 = block[2] | (block[3] << 8);
			unsigned long bits = block[4] | (block[5] << 8) | ((unsigned long)block[6] << 16) | ((unsigned long)block[7] << 24);

			for (i = 0; i < 16; i++)
			{
				unsigned x = bx * 4 + (i & 3), y = by * 4 + (i >> 2), index = (bits >> (i * 2)) & 3;
				size_t texel;
				int channel, bad = 0, four = c0 > c1;
				double e[2][3];

				if (x >= width || y >= height)
					continue;
				texel = (size_t)y * width + x;
				if (!four && index == 3)
				{
					violations += memcmp(plain + texel * 4, repaired_rgba + texel * 4, 4) != 0;
					continue;
				}
				for (channel = 0; channel < 3; channel++)
				{
					e[0][channel] = expand565(c0, channel);
					e[1][channel] = expand565(c1, channel);
				}
				for (channel = 0; channel < 3; channel++)
				{
					/* the palette color the texel had, from the original endpoints alone */
					double a = e[0][channel], b = e[1][channel], value, spacing, limit, difference;

					value = index == 0 ? a : index == 1 ? b : four ? floor((index == 2 ? 2 * a + b : a + 2 * b) / 3.0) :
						floor((a + b) / 2.0);
					spacing = four ? fabs(a - b) / 3.0 : fabs(a - b) / 2.0;
					limit = fmax(spacing / 2.0, step_levels[channel] / 2.0);
					difference = fabs((double)repaired_rgba[texel * 4 + channel] - value);
					if (flags[texel])
						bad |= difference != 0.0 && difference >= limit;
				}
				if (!flags[texel])
					bad |= memcmp(plain + texel * 4, repaired_rgba + texel * 4, 4) != 0;
				bad |= repaired_rgba[texel * 4 + 3] != 255;
				violations += bad;
			}
		}
	free(plain);
	free(flags);
	return violations;
}

int texture_repair_worth_storing(const struct texture_repair_stats *stats, int lightmap)
{
	if (!stats || !stats->changed)
		return 0;
	if (lightmap)
		return 1;
	return stats->flagged * 4 >= stats->texels && stats->stepped_pairs * 32 >= stats->flagged;
}
