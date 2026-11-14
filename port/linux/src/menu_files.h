/*
MENU_FILES.H

The menus' files (port/assets/menus: their XML, and the PNGs of their
bitmaps), embedded by tools/embed_assets.py; menu_files.c reads them, and
those in a menus folder beside config.toml, for the game (halo_menus.h).
*/

#ifndef MENU_FILES_H
#define MENU_FILES_H

#include <stddef.h>

/* an embedded file, by its path in port/assets/menus */
struct menu_file_embedded
{
	const char *path;
	const unsigned int *data;
	unsigned int size;
};

extern const struct menu_file_embedded menu_files_embedded[];
extern const unsigned int menu_files_embedded_count;

/* the GL texture standing for a menu bitmap's D3D texture (data: its Data),
decoded from its PNG on first use, and its mip levels; 0 if it is none */
unsigned int menu_art_texture(unsigned long data, unsigned long *levels);

/* the picture a menu bitmap's D3D texture (data: its Data) is drawn from,
BGRA, rows top first, which the caller frees, and its size; 0 if it is none
or could not be read (the menus' redraw, ui_redraw.h); *hand_made: one of
the port's own pictures, not the player's saved from ui.map */
int menu_art_pixels(unsigned long data, unsigned char **bgra, unsigned long *width, unsigned long *height,
	int *hand_made);

/* Changes when registering or forgetting art can replace a cached GPU texture. */
unsigned long menu_art_generation(void);

/* a language's text by ID, for halo_menus_use_language: sorted by id with no duplicates */
struct halo_menu_translation
{
	const char *id;
	const char *text;
};

/* The language the menu files are read in: NULL, "" or "en" is English (inline text, never looked up),
"en-XA" the pseudo-locale, anything else the table (sorted by id; a missing or malformed table or a missing ID
logs once and uses the inline English). halo_menus_load calls it with debug.language before reading. */
void halo_menus_use_language(const char *name, const struct halo_menu_translation *table, size_t count);

#endif
