/* Bug class: legend glyph clipped by its box or strip. controller_glyph_fit_bounds
must leave every box whole inside any clip it is drawn under. */
#include <assert.h>
#include <stddef.h>
typedef struct {short y0,x0,y1,x1;} rectangle2d;
typedef unsigned long pixel32;
#include "interface/controller_glyphs.h"
int main(void)
{
	/* the legend's 21x26 icon box, raised 3 units, under clips at and around the strip */
	for (int clip_y0 = 0; clip_y0 <= 40; clip_y0++)
	for (int clip_h = 8; clip_h <= 40; clip_h++)
	for (int clip_w = 8; clip_w <= 40; clip_w += 3)
	for (int raise = 0; raise <= 6; raise++)
	for (int offset = -4; offset <= 4; offset += 2)
	{
		rectangle2d clip = { (short)clip_y0, 100, (short)(clip_y0 + clip_h), (short)(100 + clip_w) };
		rectangle2d box = { (short)(clip_y0 - raise + offset), (short)(100 + offset), (short)(clip_y0 + 26 - raise + offset), (short)(121 + offset) };
		int w = box.x1 - box.x0, h = box.y1 - box.y0;
		controller_glyph_fit_bounds(&box, &clip);
		assert(box.x0 >= clip.x0 && box.x1 <= clip.x1 && box.y0 >= clip.y0 && box.y1 <= clip.y1);
		assert(box.x1 > box.x0 && box.y1 > box.y0);
		if (w <= clip_w && h <= clip_h) assert(box.x1 - box.x0 == w && box.y1 - box.y0 == h); /* never shrunk needlessly */
	}
	/* the legend box: slides (never shrinks) when its clip is as tall as the icon box, and raises fully when there is room */
	for (int clip_top = 0; clip_top < 20; clip_top++)
	{
		rectangle2d icon = { (short)clip_top, 50, (short)(clip_top + 26), 71 };
		rectangle2d tight = { (short)clip_top, 0, (short)(clip_top + 26), 640 }, roomy = { (short)(clip_top - 6), 0, (short)(clip_top + 30), 640 };
		rectangle2d box = controller_glyph_legend_bounds(&icon, &tight);
		assert(box.y0 == tight.y0 && box.y1 == tight.y1 && box.x1 - box.x0 == 21);
		box = controller_glyph_legend_bounds(&icon, &roomy);
		assert(box.y0 == icon.y0 - LEGEND_GLYPH_RAISE && box.y1 - box.y0 == 26);
		box = controller_glyph_legend_bounds(&icon, NULL);
		assert(box.y0 == icon.y0 - LEGEND_GLYPH_RAISE);
	}
	{
		rectangle2d box = { 5, 5, 31, 26 }, same = box;
		controller_glyph_fit_bounds(&box, NULL);
		assert(box.y0 == same.y0 && box.x1 == same.x1);
	}
	return 0;
}
