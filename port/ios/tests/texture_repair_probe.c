/* Apache-2.0. Property tests for the texture repair filter (port/runtime/texture_repair.c) on synthetic gradients: no
stock map data. tools/test_texture_repair.py builds and runs it. */
#include "texture_repair.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned random_state = 12345;
static unsigned next_random(void)
{
	random_state = random_state * 1664525u + 1013904223u;
	return random_state >> 8;
}

/* exact rational rounding, independent of the library: the stored value, or -1 on a tie */
static int reference_round(int bits, int value)
{
	const int m = (1 << bits) - 1, numerator = 2 * value * m + 255;

	return numerator % 510 == 0 ? -1 : numerator / 510;
}

static void test_intervals(void)
{
	int bits, stored, v;

	for (bits = 5; bits <= 6; bits++)
		for (stored = 0; stored < (1 << bits); stored++)
		{
			int low, high, found = 0, first = -1, last = -1;

			texture_repair_rounding_interval((unsigned)bits, (unsigned)stored, &low, &high);
			for (v = 0; v < 256; v++)
				if (reference_round(bits, v) == stored)
				{
					found++;
					if (first < 0)
						first = v;
					last = v;
				}
			assert(found && first == low && last == high && found == high - low + 1);
			/* the upload's value is inside its own interval */
			assert((int)texture_repair_expand((unsigned)bits, (unsigned)stored) >= low);
			assert((int)texture_repair_expand((unsigned)bits, (unsigned)stored) <= high);
		}
}

static void put565(uint8_t *texel, int r, int g, int b)
{
	texel[0] = (uint8_t)texture_repair_expand(5, (unsigned)r);
	texel[1] = (uint8_t)texture_repair_expand(6, (unsigned)g);
	texel[2] = (uint8_t)texture_repair_expand(5, (unsigned)b);
	texel[3] = 255;
}

static int quantize(double value, int bits)
{
	const int m = (1 << bits) - 1;
	int s = (int)(value * m / 255.0 + 0.5);

	return s < 0 ? 0 : s > m ? m : s;
}

static int max_adjacent_difference(const uint8_t *rgba, unsigned width, unsigned x0, unsigned x1, unsigned y, int channel)
{
	int worst = 0;
	unsigned x;

	for (x = x0; x + 1 < x1; x++)
	{
		int d = abs((int)rgba[((size_t)y * width + x + 1) * 4 + channel] - (int)rgba[((size_t)y * width + x) * 4 + channel]);

		if (d > worst)
			worst = d;
	}
	return worst;
}

static unsigned distinct(const uint8_t *rgba, unsigned width, unsigned x0, unsigned x1, unsigned y, int channel)
{
	unsigned seen[256] = { 0 }, count = 0, x;

	for (x = x0; x < x1; x++)
		if (!seen[rgba[((size_t)y * width + x) * 4 + channel]]++)
			count++;
	return count;
}

#define W 128
#define H 96

/* a slow falloff on the left, a cliff, and random detail on the right */
static void make_lightmap(uint8_t *rgba)
{
	unsigned x, y;

	for (y = 0; y < H; y++)
		for (x = 0; x < W; x++)
		{
			uint8_t *texel = rgba + ((size_t)y * W + x) * 4;

			if (x >= 100)
				put565(texel, (int)(next_random() % 32), (int)(next_random() % 64), (int)(next_random() % 32));
			else
			{
				double fall = 30.0 + 0.45 * x + 0.2 * y + (x >= 64 && y >= 64 ? 70.0 : 0.0);

				put565(texel, quantize(fall, 5), quantize(fall * 0.9 + 3.0, 6), quantize(fall * 0.6 + 10.0, 5));
			}
		}
}

static void test_lightmap_gradient(void)
{
	static uint8_t source[W * H * 4], repaired[W * H * 4];
	struct texture_repair_stats stats;
	unsigned x, y, detail_changed = 0;
	int before, after;

	make_lightmap(source);
	assert(texture_repair_r5g6b5(source, W, H, 0, repaired, &stats) == 0);
	/* a repair, not a repaint: every texel rounds back to what was stored, every unflagged texel is untouched */
	assert(texture_repair_verify_r5g6b5(source, repaired, W, H, 0) == 0);
	assert(stats.changed > 0);
	assert(stats.flagged * 2 > stats.texels);
	assert(texture_repair_worth_storing(&stats, 1));
	/* the steps are gone: plateaus 8 levels apart become ramps */
	before = max_adjacent_difference(source, W, 8, 56, 20, 0);
	after = max_adjacent_difference(repaired, W, 8, 56, 20, 0);
	assert(before >= 8);
	assert(after <= 3);
	assert(distinct(repaired, W, 8, 56, 20, 0) > 2 * distinct(source, W, 8, 56, 20, 0));
	before = max_adjacent_difference(source, W, 8, 56, 20, 1);
	after = max_adjacent_difference(repaired, W, 8, 56, 20, 1);
	assert(before >= 4);
	assert(after <= 2);
	/* the cliff (a 70 level jump) is not smoothed across */
	assert(abs((int)repaired[((size_t)80 * W + 62) * 4] - (int)source[((size_t)80 * W + 62) * 4]) <= 4);
	assert(abs((int)repaired[((size_t)80 * W + 66) * 4] - (int)source[((size_t)80 * W + 66) * 4]) <= 4);
	assert(repaired[((size_t)80 * W + 66) * 4] > repaired[((size_t)80 * W + 62) * 4] + 40);
	/* the detail region is byte-identical */
	for (y = 0; y < H; y++)
		for (x = 104; x < W; x++)
			detail_changed += memcmp(source + ((size_t)y * W + x) * 4, repaired + ((size_t)y * W + x) * 4, 4) != 0;
	assert(detail_changed == 0);
}

static void test_flat_and_rejects(void)
{
	static uint8_t flat[16 * 16 * 4], repaired[16 * 16 * 4];
	struct texture_repair_stats stats;
	unsigned i;

	for (i = 0; i < 256; i++)
		put565(flat + i * 4, 7, 20, 3);
	assert(texture_repair_r5g6b5(flat, 16, 16, 1, repaired, &stats) == 0);
	assert(stats.changed == 0 && !memcmp(flat, repaired, sizeof(flat)));
	assert(!texture_repair_worth_storing(&stats, 1));
	/* not an expansion of an R5G6B5 value: refused, not guessed at */
	flat[0] = 1;
	assert(texture_repair_r5g6b5(flat, 16, 16, 1, repaired, NULL) == -1);
	flat[0] = (uint8_t)texture_repair_expand(5, 7);
	flat[3] = 0;
	assert(texture_repair_r5g6b5(flat, 16, 16, 1, repaired, NULL) == -1);
}

static void test_wrap(void)
{
	/* a tiling ramp: red climbs across the whole width and wraps; the seam must obey the same property */
	static uint8_t source[64 * 8 * 4], repaired[64 * 8 * 4];
	unsigned x, y;

	for (y = 0; y < 8; y++)
		for (x = 0; x < 64; x++)
			put565(source + ((size_t)y * 64 + x) * 4, quantize(40.0 + (x < 32 ? x : 63 - x) * 0.7, 5), 20, 10);
	assert(texture_repair_r5g6b5(source, 64, 8, 1, repaired, NULL) == 0);
	assert(texture_repair_verify_r5g6b5(source, repaired, 64, 8, 1) == 0);
}

/* a DXT1 block: endpoints and sixteen 2-bit indices (all the same, or a row pattern) */
static void put_block(uint8_t *block, unsigned c0, unsigned c1, unsigned long bits)
{
	block[0] = (uint8_t)c0;
	block[1] = (uint8_t)(c0 >> 8);
	block[2] = (uint8_t)c1;
	block[3] = (uint8_t)(c1 >> 8);
	block[4] = (uint8_t)bits;
	block[5] = (uint8_t)(bits >> 8);
	block[6] = (uint8_t)(bits >> 16);
	block[7] = (uint8_t)(bits >> 24);
}

static unsigned color565(int r, int g, int b)
{
	return ((unsigned)r << 11) | ((unsigned)g << 5) | (unsigned)b;
}

static void test_dxt1_decode(void)
{
	uint8_t block[8], out[16 * 4];
	unsigned long bits = 0;
	unsigned i;

	for (i = 0; i < 16; i++)
		bits |= (unsigned long)(i & 3) << (i * 2);
	/* four-color block: c0 > c1 */
	put_block(block, color565(31, 63, 31), color565(0, 0, 0), bits);
	texture_repair_dxt1_decode(block, 4, 4, out);
	assert(out[0] == 255 && out[1] == 255 && out[2] == 255 && out[3] == 255);
	assert(out[4] == 0 && out[5] == 0 && out[6] == 0 && out[7] == 255);
	assert(out[8] == 170 && out[9] == 170 && out[10] == 170);
	assert(out[12] == 85 && out[13] == 85 && out[14] == 85);
	/* three-color block with punch-through: c0 <= c1 */
	put_block(block, color565(0, 0, 0), color565(31, 63, 31), bits);
	texture_repair_dxt1_decode(block, 4, 4, out);
	assert(out[8] == 127 && out[11] == 255);
	assert(out[12] == 0 && out[15] == 0);
}

#define DW 96
#define DH 32

static void test_dxt1_gradient(void)
{
	static uint8_t blocks[(DW / 4) * (DH / 4) * 8], repaired[DW * DH * 4], plain[DW * DH * 4];
	struct texture_repair_stats stats;
	unsigned bx, by, x, y, punch_changed = 0;
	int before, after;

	/* a banded sky: flat blocks (both endpoints equal) one endpoint step apart, 12 texels wide per step */
	for (by = 0; by < DH / 4; by++)
		for (bx = 0; bx < DW / 4; bx++)
		{
			int step = (int)(bx / 3), g = 10 + step;
			uint8_t *block = blocks + ((size_t)by * (DW / 4) + bx) * 8;

			put_block(block, color565(5 + step / 2, g, 8 + step / 2), color565(5 + step / 2, g, 8 + step / 2), 0);
		}
	/* a punch-through block among them: c0 <= c1, index 3 on the first row */
	put_block(blocks + ((size_t)3 * (DW / 4) + 20) * 8, color565(6, 12, 9), color565(6, 12, 9), 0x000000ffUL);
	assert(texture_repair_dxt1(blocks, DW, DH, 0, repaired, &stats) == 0);
	texture_repair_dxt1_decode(blocks, DW, DH, plain);
	assert(texture_repair_verify_dxt1(blocks, repaired, DW, DH, 0) == 0);
	assert(stats.changed > 0 && texture_repair_worth_storing(&stats, 0));
	/* blocks that could not move under a spacing-only limit now ramp */
	before = max_adjacent_difference(plain, DW, 4, 60, 12, 1);
	after = max_adjacent_difference(repaired, DW, 4, 60, 12, 1);
	assert(before >= 4);
	assert(after <= 2);
	assert(distinct(repaired, DW, 4, 60, 12, 1) > distinct(plain, DW, 4, 60, 12, 1));
	/* the punch-through texels are never touched */
	for (y = 12; y < 13; y++)
		for (x = 80; x < 84; x++)
			punch_changed += memcmp(plain + ((size_t)y * DW + x) * 4, repaired + ((size_t)y * DW + x) * 4, 4) != 0;
	assert(punch_changed == 0);
	assert(repaired[((size_t)12 * DW + 80) * 4 + 3] == 0);
}

static void test_dxt1_spacing_limit(void)
{
	/* a block whose endpoints are far apart: the limit is half the palette spacing, so a repaired texel stays nearest to
	   its own palette color */
	static uint8_t blocks[4 * 4 * 8], repaired[16 * 16 * 4];
	unsigned i;

	random_state = 99;
	for (i = 0; i < 16; i++)
	{
		unsigned long bits = 0;
		unsigned t;

		for (t = 0; t < 16; t++)
			bits |= (unsigned long)(next_random() & 3) << (t * 2);
		put_block(blocks + i * 8, color565((int)(next_random() % 32), (int)(next_random() % 64), (int)(next_random() % 32)),
			color565((int)(next_random() % 32), (int)(next_random() % 64), (int)(next_random() % 32)), bits);
	}
	assert(texture_repair_dxt1(blocks, 16, 16, 0, repaired, NULL) == 0);
	assert(texture_repair_verify_dxt1(blocks, repaired, 16, 16, 0) == 0);
	/* odd sizes: a level under 4 texels */
	assert(texture_repair_dxt1(blocks, 2, 2, 0, repaired, NULL) == 0);
	assert(texture_repair_verify_dxt1(blocks, repaired, 2, 2, 0) == 0);
}

static void test_verify_catches_repaint(void)
{
	static uint8_t source[W * H * 4], repaired[W * H * 4];

	make_lightmap(source);
	assert(texture_repair_r5g6b5(source, W, H, 0, repaired, NULL) == 0);
	/* one texel moved by two stored steps is a repaint */
	repaired[((size_t)10 * W + 10) * 4] = (uint8_t)(repaired[((size_t)10 * W + 10) * 4] + 20);
	assert(texture_repair_verify_r5g6b5(source, repaired, W, H, 0) > 0);
	/* and a texel changed outside any flagged area */
	repaired[((size_t)10 * W + 10) * 4] = source[((size_t)10 * W + 10) * 4];
	repaired[((size_t)50 * W + 110) * 4 + 1] ^= 4;
	assert(texture_repair_verify_r5g6b5(source, repaired, W, H, 0) > 0);
}

/* the whole-texture call the repair's records and the upscaler's sources share: each level as the level call makes it,
end to end, and the storing rule's answer */
static void test_whole_texture(void)
{
	static uint8_t source[W * H * 4 + (W / 2) * (H / 2) * 4], out[sizeof(source)], level0[W * H * 4];
	size_t offsets[2] = { 0, (size_t)W * H * 4 };
	unsigned x, y;

	make_lightmap(source);
	for (y = 0; y < H / 2; y++)
		for (x = 0; x < W / 2; x++)
			memcpy(source + offsets[1] + ((size_t)y * (W / 2) + x) * 4, source + ((size_t)(y * 2) * W + x * 2) * 4, 4);
	assert(texture_repair_applies(TEXTURE_REPAIR_FORMAT_R5G6B5, W, H));
	assert(texture_repair_applies(TEXTURE_REPAIR_FORMAT_DXT1, 1024, 1024));
	assert(!texture_repair_applies(TEXTURE_REPAIR_FORMAT_DXT1, 2048, 1024));
	assert(!texture_repair_applies(0, W, H));
	assert(texture_repair_texture(0, W, H, 2, source, offsets, NULL, NULL, out, NULL) == -1);
	/* a lightmap whose repair changed anything is stored */
	assert(texture_repair_texture(TEXTURE_REPAIR_FORMAT_R5G6B5, W, H, 2, source, offsets, NULL, NULL, out, NULL) == 1);
	assert(texture_repair_r5g6b5(source, W, H, 0, level0, NULL) == 0);
	assert(!memcmp(out, level0, sizeof(level0)));
	assert(texture_repair_verify_r5g6b5(source + offsets[1], out + offsets[1], W / 2, H / 2, 0) == 0);
}

int main(void)
{
	test_intervals();
	test_lightmap_gradient();
	test_flat_and_rejects();
	test_wrap();
	test_dxt1_decode();
	test_dxt1_gradient();
	test_dxt1_spacing_limit();
	test_verify_catches_repaint();
	test_whole_texture();
	puts("texture repair: ok");
	return 0;
}
