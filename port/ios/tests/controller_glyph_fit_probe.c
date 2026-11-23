/* Bug class: legend glyph clipped by its box or strip. The raise moves the glyph's box and its
clip together, so a box inside its clip stays inside it, at the full raise. */
#include <assert.h>
#include <stddef.h>
typedef struct {short y0,x0,y1,x1;} rectangle2d;
typedef unsigned long pixel32;
#include "interface/controller_glyphs.h"
int main(void)
{
	for (int top = 0; top < 40; top++)
	for (int margin = 0; margin <= 8; margin++)
	{
		rectangle2d icon = { (short)top, 50, (short)(top + 26), 71 };
		rectangle2d clip = { (short)(top - margin), 0, (short)(top + 26 + margin), 640 };
		rectangle2d box = controller_glyph_legend_raise(&icon), shifted = controller_glyph_legend_raise(&clip);
		assert(box.y0 == icon.y0 - LEGEND_GLYPH_RAISE && box.y1 - box.y0 == 26 && box.x0 == icon.x0 && box.x1 == icon.x1);
		assert(box.x0 >= shifted.x0 && box.x1 <= shifted.x1 && box.y0 >= shifted.y0 && box.y1 <= shifted.y1);
	}
	return 0;
}
