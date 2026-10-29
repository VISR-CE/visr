/*
HALO_MENUS.H

The menus written in XML (port/assets/menus, and a menus folder beside
config.toml), as the platform layer reads them (port/linux/src/menu_files.c)
for the game, which builds them into widget tags when ui.map loads
(port/linux/game/menu_tags.c). Plain C89 types, for both: each element keeps
its attributes as written, and the file and line it came from; the game reads
their meaning.

A file holds <menus> with <bitmap> (holding <frame>s), <strings> (holding
<string>s) and <widget> elements; a <widget> holds <widget> and <child>
(its children), <on> (an event handler), <data> (a game data input),
<conditional> and <replace> elements. A <patch> changes a list of the
map's own (the Xbox look's, in ui.map): its <row>, <remove> and <replace>
elements add, take out and swap the list's rows. port/assets/menus/README.md
describes them.
*/

#ifndef HALO_MENUS_H
#define HALO_MENUS_H

#define HALO_MENU_NONE (-1L)
/* the port's event handler and game data functions (menu_functions.c) are
numbered from here, clear of the Xbox's (0 to 101 and 0 to 40) and the PC's
(to 189 and 58) */
#define PC_MENU_FUNCTION_BASE 256

/* a frame: a PNG drawn at width by height, or the map's bitmap's frame
(map: its bitmap group tag, index: the bitmap's number in it), drawn as the
map has it or, with a width and height, scaled to that size, x and y into
the widget */
struct halo_menu_frame
{
	char const *png;
	/* a PNG of the player's, made from their own map's art and kept in the menus folder, drawn instead of png
	where it is there (the shipped redraw is png, the placeholder where there is none) */
	char const *cache;
	char const *map;
	long index;
	long width, height;
	long x, y;
};

struct halo_menu_bitmap
{
	char const *name;
	/* its frames: frame_count from first_frame in the frames */
	long first_frame, frame_count;
	char const *file;
	long line;
};

struct halo_menu_strings
{
	char const *name;
	/* its strings: count from first in the strings */
	long first, count;
	char const *file;
	long line;
};

struct halo_menu_handler
{
	char const *event, *run, *script, *open, *replace, *close, *widget, *focus, *reload, *sound, *otherwise, *label;
	long back, branch;
	long next;
	char const *file;
	long line;
};

struct halo_menu_input
{
	char const *input;
	long next;
	char const *file;
	long line;
};

/* a child: a <widget> inside its parent (nested), or one named (<child>) */
struct halo_menu_child
{
	long nested;
	char const *widget;
	long x, y;
	char const *controller;
	long next;
	char const *file;
	long line;
};

struct halo_menu_conditional
{
	char const *widget;
	long if_failed;
	long next;
	char const *file;
	long line;
};

struct halo_menu_replace
{
	char const *search, *function;
	long next;
	char const *file;
	long line;
};

struct halo_menu_widget
{
	char const *name, *type, *controller, *flags, *bitmap, *text, *strings, *values, *setting, *font, *color,
		*align, *description, *string_list, *list_flags, *text_flags, *header_bitmap, *footer_bitmap,
		*header_bounds, *footer_bounds;
	long x, y, left, top, width, height, text_x, text_y, auto_close, auto_close_fade, string_index;
	long has_width, has_height;
	/* a column list's rows shown at once (0: all) */
	long visible_rows;
	/* indices in their arrays, or HALO_MENU_NONE */
	long parent, first_child, first_handler, first_input, first_conditional, first_replace;
	char const *file;
	long line;
};

/* a patch of one of the map's lists: its rows, in the file's order, from
first_row in the patch rows */
struct halo_menu_patch
{
	char const *list, *screen;
	/* the y the rows stay above (footer=), else the screen's a_butn's */
	long footer, has_footer;
	long first_row, row_count;
	char const *file;
	long line;
};

enum
{
	HALO_MENU_PATCH_ROW,
	HALO_MENU_PATCH_REMOVE,
	HALO_MENU_PATCH_REPLACE,
};

/* a <row> (added after or before a row), <remove> (child) or <replace>
(child): a row's look is model's, the map's widget by its path (a row's
default: the row it is put by; a replacement's: the row it replaces) */
struct halo_menu_patch_row
{
	long kind;
	char const *after, *before, *child, *model;
	char const *name, *text, *run, *open, *created, *description, *picture;
	/* a <row> that is one of our widgets as it is (widget=) rather than a copy */
	char const *widget;
	/* a row's own picture (a bitmap of ours), instead of its model's */
	char const *bitmap;
	/* with widget=: the strings (separated by |) the list's description text shows for the row's values */
	char const *help;
	long pitch, has_pitch;
	char const *file;
	long line;
};

struct halo_menus
{
	struct halo_menu_bitmap *bitmaps;
	long bitmap_count;
	struct halo_menu_frame *frames;
	long frame_count;
	struct halo_menu_strings *string_lists;
	long string_list_count;
	char const **strings;
	long string_count;
	struct halo_menu_widget *widgets;
	long widget_count;
	struct halo_menu_child *children;
	long child_count;
	struct halo_menu_handler *handlers;
	long handler_count;
	struct halo_menu_input *inputs;
	long input_count;
	struct halo_menu_conditional *conditionals;
	long conditional_count;
	struct halo_menu_replace *replaces;
	long replace_count;
	struct halo_menu_patch *patches;
	long patch_count;
	struct halo_menu_patch_row *patch_rows;
	long patch_row_count;
	/* the main menu: <menus root="..."> (the last given), else "main_menu" */
	char const *root;
};

/* the menus, read from the files (once; the same each call after); NULL if
there are none, or a file has an error (logged) */
struct halo_menus const *halo_menus_load(void);
/* text as UTF-16, its terminator included, "\n" written as the game's line
breaks ("\r\n"): the number of characters written (at most capacity) */
long halo_menus_utf16(char const *utf8, unsigned short *out, long capacity);
/* logs a problem in a file's element */
void halo_menus_log(char const *file, long line, char const *message, char const *detail);

/* menu art: the D3D texture (as the game's bitmap holds it) a frame's PNG is
drawn for, until halo_menus_art_forget */
void halo_menus_art_register(void const *texture, char const *png);
/* The player's cache of the map's pictures, which no map but ui.map has (a frame's cache): files in the menus folder.
exists: 1 if the file is there; write_png saves width by height RGBA bytes (rows top first) as the file, 1 if it
could be; art_extract saves a map bitmap's texture (its hardware format) as one, 1 if it could be (xbox_textures.c) */
int halo_menus_cache_exists(char const *path);
int halo_menus_cache_write_png(char const *path, unsigned char const *rgba, unsigned long width, unsigned long height);
int halo_menus_art_extract(void const *hardware_format, char const *cache_path);
/* saves the pictures the loaded menus asked for (menu_tags.c), when menus draw */
void pc_menu_art_extract(void);
void halo_menus_art_forget(void);

#endif
