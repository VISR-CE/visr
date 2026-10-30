/* ui_redraw_rebuild (port/linux/src/ui_redraw_rebuild.c): a two-texel border
stays two texels wide and sharp, a flat fill and a gentle gradient pass
through unchanged. */
#include "ui_redraw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void check(int condition, const char *what)
{
	if (!condition)
	{
		fprintf(stderr, "ui_redraw_probe: %s\n", what);
		failures++;
	}
}

int main(void)
{
	enum { WIDTH = 16, HEIGHT = 8, SCALE = 4 };
	static unsigned char frame[WIDTH * HEIGHT * 4], out[WIDTH * HEIGHT * 4 * SCALE * SCALE];
	long x, y, sharp = 0, soft = 0;

	/* a border two texels wide (blue) at columns 0-1 over a navy fill */
	for (y = 0; y < HEIGHT; y++)
	{
		for (x = 0; x < WIDTH; x++)
		{
			unsigned char *texel = frame + (y * WIDTH + x) * 4;
			int border = x < 2;

			texel[0] = border ? 41 : 0;
			texel[1] = border ? 150 : 13;
			texel[2] = border ? 255 : 33;
			texel[3] = 255;
		}
	}
	check(ui_redraw_rebuild(frame, WIDTH, HEIGHT, 4, WIDTH * SCALE, HEIGHT * SCALE, out), "rebuild failed");
	/* along a middle row: the border's color through output column 7, the
	fill's from column 8, with at most one column between */
	y = HEIGHT * SCALE / 2;
	for (x = 0; x < WIDTH * SCALE; x++)
	{
		unsigned char green = out[(y * WIDTH * SCALE + x) * 4 + 1];

		if (green == 150 || green == 13)
			sharp++;
		else
			soft++;
		if (x < 7)
			check(green >= 145, "the border keeps its color and width");
		if (x > 8 && x < WIDTH * SCALE - 1)
			check(green <= 18, "the fill keeps its color");
	}
	check(soft <= 2, "the edge is at most one output pixel soft");
	/* a flat fill and a gentle gradient: unchanged */
	for (y = 0; y < HEIGHT; y++)
	{
		for (x = 0; x < WIDTH; x++)
		{
			unsigned char *texel = frame + (y * WIDTH + x) * 4;

			texel[0] = texel[1] = texel[2] = (unsigned char)(100 + x);
			texel[3] = 255;
		}
	}
	check(ui_redraw_rebuild(frame, WIDTH, HEIGHT, 4, WIDTH * SCALE, HEIGHT * SCALE, out), "rebuild failed");
	for (x = SCALE * 2; x < (WIDTH - 2) * SCALE; x++)
	{
		int value = out[((HEIGHT * SCALE / 2) * WIDTH * SCALE + x) * 4];
		float expected = 100.0f + (x + 0.5f) / SCALE - 0.5f;

		check(value >= expected - 1.0f && value <= expected + 1.0f, "a gradient stays smooth");
	}
	check(!ui_redraw_rebuild(frame, WIDTH, HEIGHT, 1, WIDTH * SCALE, HEIGHT * SCALE, out), "refuses one channel");
	check(!ui_redraw_rebuild(NULL, WIDTH, HEIGHT, 4, WIDTH * SCALE, HEIGHT * SCALE, out), "refuses no source");
	(void)sharp;
	if (failures)
		return 1;
	printf("ui_redraw_probe: ok\n");
	return 0;
}
