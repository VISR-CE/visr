/* A run of quad lists drawn as one indexed draw (halo_draw_quad_ranges, d3d8_device.c): which vertices the draw
uploads and how its indices and per-range colors are laid out.

Two layouts give the same triangles from the same vertex bytes and colors, in the same order:
- dense: only the vertices the ranges draw, one range after another in draw order. The upload grows with what is
  drawn.
- span: every vertex from the lowest range's first to the highest range's last, indexed from the lowest. The upload
  grows with how far apart the ranges sit in their vertex buffer, whatever they draw; kept to compare
  (display.decal_dense_upload = false) and as the reference quad_ranges_mismatches and the test check against.

Pure functions, so tools/test_quad_ranges.py can run them on a development machine. */
#ifndef QUAD_RANGES_H
#define QUAD_RANGES_H

#include <string.h>

/* the most vertices a draw with 16-bit indices can address */
#define QUAD_RANGES_MAXIMUM_VERTICES 65535UL

struct quad_ranges
{
	const unsigned int *first; /* each range's first vertex in the source buffer */
	const unsigned int *quads; /* each range's quads (four vertices each) */
	unsigned long count;
};

/* whether a draw of drawn vertices uses the dense layout: when it is wanted and the vertices fit 16-bit indices
(a longer run is drawn as a span if its span fits, and not at all otherwise) */
static inline int quad_ranges_dense(int wanted, unsigned long drawn)
{
	return wanted && drawn <= QUAD_RANGES_MAXIMUM_VERTICES;
}

/* the corners of a quad's two triangles */
static const unsigned char quad_ranges_corners[6] = { 0, 1, 2, 0, 2, 3 };

/* the vertices the ranges draw */
static inline unsigned long quad_ranges_vertex_count(const struct quad_ranges *ranges)
{
	unsigned long range, total = 0;

	for (range = 0; range < ranges->count; range++)
		total += (unsigned long)ranges->quads[range] * 4;
	return total;
}

/* the span layout's first vertex and its length in vertices */
static inline void quad_ranges_span(const struct quad_ranges *ranges, unsigned long *low, unsigned long *span)
{
	unsigned long range, lowest = ~0UL, highest = 0;

	for (range = 0; range < ranges->count; range++)
	{
		unsigned long first = ranges->first[range], end = first + (unsigned long)ranges->quads[range] * 4;

		if (first < lowest)
			lowest = first;
		if (end > highest)
			highest = end;
	}
	*low = ranges->count ? lowest : 0;
	*span = ranges->count ? highest - lowest : 0;
}

/* where a layout puts range's first vertex: dense, after the ranges before it; span, at its distance from low */
static inline unsigned long quad_ranges_base(const struct quad_ranges *ranges, int dense, unsigned long low,
	unsigned long range, unsigned long drawn_before)
{
	return dense ? drawn_before : ranges->first[range] - low;
}

/* dense: copies the ranges' vertices of a stream (stride bytes each, the buffer starting at vertex 0) one after
another into out */
static inline void quad_ranges_gather(const struct quad_ranges *ranges, const unsigned char *source,
	unsigned long stride, unsigned char *out)
{
	unsigned long range, at = 0;

	for (range = 0; range < ranges->count; range++)
	{
		unsigned long bytes = (unsigned long)ranges->quads[range] * 4 * stride;

		memcpy(out + at, source + ranges->first[range] * stride, bytes);
		at += bytes;
	}
}

/* each range's color (red, green, blue and alpha bytes) as four floats for each of its vertices, at the layout's
place for them; a span's vertices outside every range are left as they were (no index reads them) */
static inline void quad_ranges_colors(const struct quad_ranges *ranges, int dense, unsigned long low,
	const unsigned char (*colors)[4], float *values)
{
	unsigned long range, drawn = 0;

	for (range = 0; range < ranges->count; range++)
	{
		unsigned long base = quad_ranges_base(ranges, dense, low, range, drawn), vertex;
		unsigned long vertices = (unsigned long)ranges->quads[range] * 4;

		for (vertex = base; vertex < base + vertices; vertex++)
		{
			values[vertex * 4 + 0] = colors[range][0] / 255.0f;
			values[vertex * 4 + 1] = colors[range][1] / 255.0f;
			values[vertex * 4 + 2] = colors[range][2] / 255.0f;
			values[vertex * 4 + 3] = colors[range][3] / 255.0f;
		}
		drawn += vertices;
	}
}

/* the triangles' indices, the ranges' quads in order (six for each quad); returns how many */
static inline unsigned long quad_ranges_indices(const struct quad_ranges *ranges, int dense, unsigned long low,
	unsigned short *indices)
{
	unsigned long range, drawn = 0, count = 0;

	for (range = 0; range < ranges->count; range++)
	{
		unsigned long base = quad_ranges_base(ranges, dense, low, range, drawn), quad;
		int corner;

		for (quad = 0; quad < ranges->quads[range]; quad++)
		{
			for (corner = 0; corner < 6; corner++)
				indices[count++] = (unsigned short)(base + quad * 4 + quad_ranges_corners[corner]);
		}
		drawn += (unsigned long)ranges->quads[range] * 4;
	}
	return count;
}

/* the check (debug.decal_dense_check): how many of the corners a dense draw's indices read differ from what
the game's own draws read for the same corner, which is the source buffer's vertex at the range's own index and
the range's color. vertices is the gathered stream (NULL: colors only), values the dense colors (NULL: vertices
only) */
static inline unsigned long quad_ranges_mismatches(const struct quad_ranges *ranges, const unsigned short *indices,
	const unsigned char *source, unsigned long stride, const unsigned char *vertices,
	const unsigned char (*colors)[4], const float *values)
{
	unsigned long range, at = 0, mismatches = 0;

	for (range = 0; range < ranges->count; range++)
	{
		unsigned long corners = (unsigned long)ranges->quads[range] * 6, corner;
		float color[4];

		if (values)
		{
			color[0] = colors[range][0] / 255.0f;
			color[1] = colors[range][1] / 255.0f;
			color[2] = colors[range][2] / 255.0f;
			color[3] = colors[range][3] / 255.0f;
		}
		for (corner = 0; corner < corners; corner++, at++)
		{
			unsigned long original = ranges->first[range] + corner / 6 * 4 + quad_ranges_corners[corner % 6];
			unsigned long read = indices[at];

			if ((vertices && memcmp(vertices + read * stride, source + original * stride, stride)) ||
				(values && memcmp(values + read * 4, color, sizeof(color))))
				mismatches++;
		}
	}
	return mismatches;
}

#endif
