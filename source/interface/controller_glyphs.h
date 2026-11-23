/* port: controller_glyph_draw preserves the caller's design-space rectangle.
A zero result leaves the game's art in charge; nonzero includes touch hiding. */
#ifndef HALO_INTERFACE_CONTROLLER_GLYPHS_H
#define HALO_INTERFACE_CONTROLLER_GLYPHS_H
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
/* port: a glyph is drawn whole. Moving its box (the legend's raise onto the
caption's cap height) must never push an edge past the clip it is drawn under,
because controller_glyph_blit crops the texture there. The box keeps its
aspect: it shrinks to the clip if larger, then slides back inside it. */
static inline void controller_glyph_fit_bounds(rectangle2d *bounds, rectangle2d const *clip)
{
	int width, height, clip_width, clip_height;
	if (!clip || clip->x1 <= clip->x0 || clip->y1 <= clip->y0) return;
	width = bounds->x1 - bounds->x0; height = bounds->y1 - bounds->y0;
	clip_width = clip->x1 - clip->x0; clip_height = clip->y1 - clip->y0;
	if (width > clip_width || height > clip_height)
	{
		int cx = (bounds->x0 + bounds->x1) / 2, cy = (bounds->y0 + bounds->y1) / 2;
		if (width * clip_height > height * clip_width) { height = height * clip_width / width; width = clip_width; }
		else { width = width * clip_height / height; height = clip_height; }
		bounds->x0 = (short)(cx - width / 2); bounds->x1 = (short)(bounds->x0 + width);
		bounds->y0 = (short)(cy - height / 2); bounds->y1 = (short)(bounds->y0 + height);
	}
	if (bounds->x0 < clip->x0) { bounds->x1 += clip->x0 - bounds->x0; bounds->x0 = clip->x0; }
	if (bounds->x1 > clip->x1) { bounds->x0 -= bounds->x1 - clip->x1; bounds->x1 = clip->x1; }
	if (bounds->y0 < clip->y0) { bounds->y1 += clip->y0 - bounds->y0; bounds->y0 = clip->y0; }
	if (bounds->y1 > clip->y1) { bounds->y0 -= bounds->y1 - clip->y1; bounds->y1 = clip->y1; }
}
/* design units a legend glyph is raised to sit on its caption's cap height */
#define LEGEND_GLYPH_RAISE 3
/* The legend glyph's box: the icon box raised onto the caption's capitals, slid
back down (not shrunk, so neighbors stay one size) if the raise would cross the
strip's clip. */
static inline rectangle2d controller_glyph_legend_bounds(rectangle2d const *icon, rectangle2d const *clip)
{
	rectangle2d box = *icon;
	box.y0 -= LEGEND_GLYPH_RAISE;
	box.y1 -= LEGEND_GLYPH_RAISE;
	controller_glyph_fit_bounds(&box, clip);
	return box;
}
#endif
