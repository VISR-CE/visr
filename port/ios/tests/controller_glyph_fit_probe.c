/* Bug classes: legend glyph clipped by its box or strip, and legend glyph off its caption's cap height.
Placement is checked as drawn: the box and clip the caller hands controller_glyph_blit, cropped the way it
crops. The legend's 21x26 icon box sits LEGEND_GLYPH_RAISE (3, measured against the 22-unit caption's
capitals) units below the cap-height center, so the visible glyph must be the whole box, centered 3 up. */
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
typedef struct {short y0,x0,y1,x1;} rectangle2d;
typedef unsigned long pixel32;
#include "interface/controller_glyphs.h"
int main(void)
{
	for (int slot = 0; slot < 4; slot++)
	for (int top = 0; top < 40; top++)
	for (int margin = 0; margin <= 8; margin += 4)
	{
		int x = 40 + slot * 100;
		rectangle2d icon = { (short)top, (short)x, (short)(top + 26), (short)(x + 21) };
		/* the caller's clip is the strip: as tall as the icon box, or taller */
		rectangle2d clip = { (short)(top - margin), 0, (short)(top + 26 + margin), 640 };
		rectangle2d box = controller_glyph_legend_raise(&icon), shifted = controller_glyph_legend_raise(&clip);
		rectangle2d shown = controller_glyph_visible_rect(&box, &shifted);
		int cap_center = (icon.y0 + icon.y1) / 2 - 3, shown_center = (shown.y0 + shown.y1) / 2;
		assert(shown.x0 == box.x0 && shown.x1 == box.x1 && shown.y0 == box.y0 && shown.y1 == box.y1); /* not cropped */
		assert(abs(shown_center - cap_center) <= 1);
	}
	return 0;
}
