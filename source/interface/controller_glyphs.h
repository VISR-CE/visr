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
#endif
