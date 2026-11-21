/* The dense layout of a batched quad draw reads, corner for corner, the vertex bytes and colors the game's own
draws read (one draw a range, each with its color), and uploads only the vertices drawn. So does the span layout it
replaced, whenever no two ranges share a vertex (a span has one color a vertex). Ranges are random: out of order,
far apart, touching, repeated, with gaps between them. (port/linux/src/quad_ranges.h; tools/test_quad_ranges.py runs this.) */
#include <stdio.h>
#include <stdlib.h>

#include "../../linux/src/quad_ranges.h"

enum { BUFFER_VERTICES = 40960, MAXIMUM_RANGES = 256 };

static unsigned long state = 0x2545f491UL;

static unsigned long next(unsigned long below)
{
	state = state * 1664525UL + 1013904223UL;
	return ((state >> 8) & 0xffffffUL) % below;
}

/* what a draw reads at each index: the vertex's bytes and its color */
static int expand(const unsigned short *indices, unsigned long index_count, const unsigned char *vertices,
	unsigned long stride, const float *values, unsigned char *out)
{
	unsigned long index;

	for (index = 0; index < index_count; index++)
	{
		memcpy(out, vertices + indices[index] * stride, stride);
		memcpy(out + stride, values + indices[index] * 4, 4 * sizeof(float));
		out += stride + 4 * sizeof(float);
	}
	return 0;
}

/* what the game's own draws read: each range's quads from the buffer, with the range's color */
static void expand_game(const struct quad_ranges *ranges, const unsigned char *source, unsigned long stride,
	const unsigned char (*colors)[4], unsigned char *out)
{
	unsigned long range, corner;

	for (range = 0; range < ranges->count; range++)
	{
		float color[4] = { colors[range][0] / 255.0f, colors[range][1] / 255.0f, colors[range][2] / 255.0f,
			colors[range][3] / 255.0f };

		for (corner = 0; corner < (unsigned long)ranges->quads[range] * 6; corner++)
		{
			unsigned long vertex = ranges->first[range] + corner / 6 * 4 + quad_ranges_corners[corner % 6];

			memcpy(out, source + vertex * stride, stride);
			memcpy(out + stride, color, sizeof(color));
			out += stride + sizeof(color);
		}
	}
}

int main(void)
{
	static unsigned char source[BUFFER_VERTICES * 24], gathered[65536 * 24];
	static float span_values[BUFFER_VERTICES * 4], dense_values[65536 * 4];
	static unsigned short span_indices[65536 / 4 * 6], dense_indices[65536 / 4 * 6];
	static unsigned char span_read[65536 / 4 * 6 * 40], dense_read[65536 / 4 * 6 * 40], game_read[65536 / 4 * 6 * 40];
	static unsigned char used[BUFFER_VERTICES], read[65536];
	unsigned long trial, byte;

	for (byte = 0; byte < sizeof(source); byte++)
		source[byte] = (unsigned char)next(256);
	/* which layout a draw takes: dense when wanted and its vertices fit 16-bit indices */
	if (!quad_ranges_dense(1, QUAD_RANGES_MAXIMUM_VERTICES) || quad_ranges_dense(1, QUAD_RANGES_MAXIMUM_VERTICES + 1) ||
		quad_ranges_dense(0, 4) || !quad_ranges_dense(1, 4))
	{
		printf("the dense layout is chosen for the wrong draws\n");
		return 1;
	}
	for (trial = 0; trial < 2000; trial++)
	{
		unsigned int first[MAXIMUM_RANGES], quads[MAXIMUM_RANGES];
		unsigned char colors[MAXIMUM_RANGES][4];
		struct quad_ranges ranges = { first, quads, 1 + next(trial % 7 ? 24 : MAXIMUM_RANGES) };
		unsigned long stride = trial % 3 ? 16 : 24, range, low, span, drawn, span_count, dense_count, each;
		int shared = 0;

		for (range = 0; range < ranges.count; range++)
		{
			quads[range] = 1 + (unsigned int)next(trial % 5 ? 3 : 40);
			/* a repeat of the range before it now and then, as a decal drawn twice would be */
			first[range] = range && !next(9) ? first[range - 1] :
				(unsigned int)next(BUFFER_VERTICES - quads[range] * 4 + 1);
			if (range && first[range] == first[range - 1])
				quads[range] = quads[range - 1];
			for (each = 0; each < 4; each++)
				colors[range][each] = (unsigned char)next(256);
		}
		drawn = quad_ranges_vertex_count(&ranges);
		quad_ranges_span(&ranges, &low, &span);
		if (drawn > QUAD_RANGES_MAXIMUM_VERTICES || span > QUAD_RANGES_MAXIMUM_VERTICES)
			continue;
		memset(used, 0, sizeof(used));
		for (range = 0; range < ranges.count; range++)
		{
			for (each = first[range]; each < first[range] + quads[range] * 4; each++)
				shared |= used[each]++;
		}
		expand_game(&ranges, source, stride, (const unsigned char (*)[4])colors, game_read);

		/* the span layout, as the draw was before: the game's vertices where the game put them */
		quad_ranges_colors(&ranges, 0, low, (const unsigned char (*)[4])colors, span_values);
		span_count = quad_ranges_indices(&ranges, 0, low, span_indices);
		expand(span_indices, span_count, source + low * stride, stride, span_values, span_read);

		/* the dense layout */
		quad_ranges_gather(&ranges, source, stride, gathered);
		quad_ranges_colors(&ranges, 1, 0, (const unsigned char (*)[4])colors, dense_values);
		dense_count = quad_ranges_indices(&ranges, 1, 0, dense_indices);
		expand(dense_indices, dense_count, gathered, stride, dense_values, dense_read);

		if (dense_count != span_count || dense_count != drawn / 4 * 6)
		{
			printf("trial %lu: %lu dense indices, %lu span indices, %lu vertices drawn\n", trial, dense_count,
				span_count, drawn);
			return 1;
		}
		if (memcmp(game_read, dense_read, dense_count * (stride + 4 * sizeof(float))))
		{
			printf("trial %lu: the dense draw reads other bytes than the game's own draws\n", trial);
			return 1;
		}
		if (!shared && memcmp(game_read, span_read, span_count * (stride + 4 * sizeof(float))))
		{
			printf("trial %lu: the span draw reads other bytes than the game's own draws\n", trial);
			return 1;
		}
		/* drawn vertices only: no index lies past the drawn count, and every dense vertex is read by one */
		memset(read, 0, drawn);
		for (each = 0; each < dense_count; each++)
		{
			if (dense_indices[each] >= drawn)
			{
				printf("trial %lu: index %u past the %lu vertices drawn\n", trial, dense_indices[each], drawn);
				return 1;
			}
			read[dense_indices[each]] = 1;
		}
		for (each = 0; each < drawn; each++)
		{
			if (!read[each])
			{
				printf("trial %lu: dense vertex %lu is uploaded and never read\n", trial, each);
				return 1;
			}
		}
		if (quad_ranges_mismatches(&ranges, dense_indices, source, stride, gathered,
			(const unsigned char (*)[4])colors, dense_values))
		{
			printf("trial %lu: the check reports a mismatch on a correct draw\n", trial);
			return 1;
		}
		/* the check sees one wrong byte and one wrong color */
		gathered[next(drawn * stride)] ^= 0x40;
		if (!quad_ranges_mismatches(&ranges, dense_indices, source, stride, gathered, NULL, NULL))
		{
			printf("trial %lu: the check missed a changed vertex byte\n", trial);
			return 1;
		}
		dense_values[next(drawn * 4)] += 2.0f;
		if (!quad_ranges_mismatches(&ranges, dense_indices, NULL, 0, NULL, (const unsigned char (*)[4])colors,
			dense_values))
		{
			printf("trial %lu: the check missed a changed color\n", trial);
			return 1;
		}
	}
	printf("quad ranges: the dense and span layouts read the bytes the game's own draws read\n");
	return 0;
}
