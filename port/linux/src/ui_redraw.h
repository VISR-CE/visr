/*
UI_REDRAW.H

The menus' redraw (display.ui_redraw): the menus' frames, rows, dialogs and
legend pictures, and their text, drawn at the display's resolution from the
player's own ui.map, rebuilt on the device each time the map loads. The game's
widgets still lay out, focus and draw the menus; only the textures they draw
change. The main menu, the loading screens and the HUD are not redrawn.
*/

#ifndef UI_REDRAW_H
#define UI_REDRAW_H

/* A picture rebuilt larger (ui_redraw_rebuild.c): source is width by height
texels of channels bytes each (4: RGBA or BGRA, straight alpha; 1: a
coverage), output is output_width by output_height (at least as large) of the
same. Smooth regions stay smooth; edges are drawn again about one output
pixel wide along the smooth picture's contour, between the texels' own
colors. A coverage is softened a little first, so a small glyph's curves come
out as curves. 0 if it could not be (bad arguments, out of memory). */
int ui_redraw_rebuild(const unsigned char *source, long width, long height, long channels,
	long output_width, long output_height, unsigned char *output);

/* whether display.ui_redraw is on (the committed setting) */
int ui_redraw_enabled(void);

/* Before a screen's widgets draw (render_ui_widgets): whether the redraw is
on for it, by its root widget's tag name: on, unless the setting is off or it
is the main menu. Until ui_redraw_end_screen. */
void ui_redraw_begin_screen(const char *tag_name);
void ui_redraw_end_screen(void);
/* whether the screen drawing now is redrawn */
int ui_redraw_active(void);
/* whether the screen drawing now draws its text from the font tags' own
glyphs (display.ui_redraw_text = "game") */
int ui_redraw_text_from_tags(void);

/* While a redrawn screen draws: a widget's background, its bitmap's tag name,
its texture's resource (the D3D texture's words: Data is [1]) and its size
in menu units. Noted if it is one the redraw draws (the menus' frames, rows,
dialogs and button pictures), so the texture cache gives it the rebuilt
texture from then on. */
void ui_redraw_note(const char *tag_name, const void *hardware_format, long width, long height);

/* Changes when a texture the cache gave may now be another: a texture noted,
the setting turned, the display's scale changed (xbox_textures.c re-resolves
its cached results). */
unsigned long ui_redraw_generation(void);

/* The texture cache (xbox_textures.c): for a texture with this Data, format
word and size word, whether the redraw draws it. 0: no. 1: yes, and *texture
is set (and *levels). 2: yes, but it needs the picture first: the caller
gives its texels (BGRA or RGBA, rows top first, straight alpha) to
ui_redraw_build, then asks again. */
int ui_redraw_texture(unsigned long data, unsigned long format_word, unsigned long size_word,
	unsigned int *texture, unsigned long *levels);
/* the picture of a noted texture, width by height texels; hand_made: one of
the port's own pictures, drawn as it is when it is already twice the frame's
size (a picture the menus saved from the upscaled texture is reduced to the
frame's size and rebuilt). 0 if nothing could be made, and the original is
drawn */
int ui_redraw_build(unsigned long data, const unsigned char *pixels, unsigned long width, unsigned long height,
	int hand_made);

/* On an iPhone (input.ui_touch_target): the least height, in menu units, of
a row's or button's touch target; 0 elsewhere or when off */
long ui_redraw_touch_target_units(void);

/* The map unloaded: forget every texture (their Data may be reused) */
void ui_redraw_forget(void);

#endif
