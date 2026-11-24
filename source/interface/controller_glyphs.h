/* port: controller_glyph_draw preserves the caller's design-space rectangle.
A zero result leaves the game's art in charge; nonzero includes touch hiding. */
#ifndef HALO_INTERFACE_CONTROLLER_GLYPHS_H
#define HALO_INTERFACE_CONTROLLER_GLYPHS_H
#include "menu_geometry.h"
/* hud_draw.c uses alpha_multiply_add; menus use ordinary alpha blending. */
enum { _controller_glyph_blend_menu = 0, _controller_glyph_blend_hud = 7 };
int controller_glyph_draw(short player, short semantic, short physical,
	rectangle2d const *bounds, rectangle2d const *clip, pixel32 color, short blend);
int controller_glyph_hidden(short player, short physical);
int controller_glyph_ready(short player, short semantic, short physical, rectangle2d const *bounds);
struct bitmap_data;
void controller_glyph_blit(struct bitmap_data *bitmap, rectangle2d const *bounds,
	rectangle2d const *source, rectangle2d const *clip, pixel32 color, short blend);
void controller_glyphs_dispose(void);
/* A legend glyph's box is raised MENU_BAR_GLYPH_RAISE units (menu_geometry.h) onto its caption's capitals. The
clip it is drawn under (the window, moved by the widget's placement offset) rises with it, so the raise never
pushes an edge past the clip, where controller_glyph_blit crops the texture. */
static inline rectangle2d controller_glyph_legend_raise(rectangle2d const *rect)
{
	rectangle2d raised = *rect;
	raised.y0 -= MENU_BAR_GLYPH_RAISE;
	raised.y1 -= MENU_BAR_GLYPH_RAISE;
	return raised;
}
/* what controller_glyph_blit leaves visible: the box cropped to the clip */
static inline rectangle2d controller_glyph_visible_rect(rectangle2d const *box, rectangle2d const *clip)
{
	rectangle2d shown = *box;
	if (!clip) return shown;
	if (shown.x0 < clip->x0) shown.x0 = clip->x0;
	if (shown.x1 > clip->x1) shown.x1 = clip->x1;
	if (shown.y0 < clip->y0) shown.y0 = clip->y0;
	if (shown.y1 > clip->y1) shown.y1 = clip->y1;
	return shown;
}
#endif
