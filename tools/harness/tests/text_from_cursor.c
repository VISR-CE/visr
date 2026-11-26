/*
TEXT_FROM_CURSOR.C (test)

The real draw_unicode_string_compute_bounds_from_cursor (draw_string.c), with
the real layout it measures with (draw_unicode_string), over a fake font of
fixed glyphs. A piece of text before or after an icon is measured in its
widget's box and drawn in the box the function gives: the drawn glyphs must be
the measured ones, at the same places, in every justification, wrapping or
not. test_text_from_cursor.py takes the code (under_test.inc) and its
structures (config.inc).
*/

#include <wchar.h>
#include "harness.h"

typedef unsigned short word;
typedef unsigned long pixel32;
typedef struct { short x, y; } point2d;
typedef struct { short x0, y0, x1, y1; } rectangle2d;
typedef struct { real alpha, red, green, blue; } real_argb_color;
struct rasterizer_dynamic_screen_geometry_parameters { int unused; };

#define SHORT_MAX 32767
#define SHORT_MIN (-32768)
#define NUMBER_OF_TEXT_STYLES 5
#define _text_style_plain (-1)
#define display_assert(message, file, line, fatal) ((void)0)
#define system_exit(code) exit(code)
#define text_filter_unicode(string) (string)

static short rectangle2d_width(rectangle2d const *rectangle)
{
	return (short)(rectangle->x1 - rectangle->x0);
}

/* the font: 9 units up, 3 down, 2 between lines, 1 before a line; every glyph 6 wide in a 7 wide cell from its
origin, a space a cell with no glyph, no glyph at all for '\n' (as the disc's fonts) */
struct font_header
{
	short ascending_height;
	short descending_height;
	short leading_height;
	short leading_width;
};
static struct font_header font = { 9, 3, 2, 1 };

#include "config.inc"

static struct font_character glyphs[128];

static struct font_header *styled_font_get(long font_index, short style)
{
	return &font;
}

static struct font_character *font_get_character_by_ascii_code(struct font_header *header, word character)
{
	if (character < 0x20 || character >= 128)
		return NULL;
	glyphs[character].character = character;
	glyphs[character].character_width = 7;
	glyphs[character].bitmap_width = character == ' ' ? 0 : 6;
	glyphs[character].bitmap_height = 12;
	glyphs[character].bitmap_origin_x = 0;
	glyphs[character].bitmap_origin_y = 9;
	return &glyphs[character];
}

typedef void (*draw_character_proc)(struct parse_string_state *state, struct font_header *font,
	struct font_character *character, pixel32 color, short x0, short y0, short x, short y, short dx, short dy);

#include "under_test.inc"

/* the glyphs one pass drew: character and place */
struct drawn { word character; short x, y; };
static struct drawn drawn[2][256];
static int drawn_count[2];
static int pass;

static void record(struct parse_string_state *state, struct font_header *header, struct font_character *character,
	pixel32 color, short x0, short y0, short x, short y, short dx, short dy)
{
	if (drawn_count[pass] < 256)
		drawn[pass][drawn_count[pass]++] = (struct drawn){ character->character, x0, y0 };
}

/* the piece measured in `bounds` from `cursor_x0` and drawn as the icon strings draw it (in the box the function
gives, as rasterizer_draw_unicode_string lays it out); checks the drawn glyphs are the measured ones */
static void check_piece(wchar_t const *text, rectangle2d bounds, short cursor_x0, short justification, unsigned long flags)
{
	rectangle2d draw_bounds;
	rectangle2d cursor_bounds = bounds;
	int index;

	font_drawing_globals.current_justification = justification;
	font_drawing_globals.current_flags = flags;
	cursor_bounds.x0 = cursor_x0;
	draw_unicode_string_compute_bounds_from_cursor(&bounds, text, &draw_bounds, &cursor_bounds);
	/* (the indents the function set stay for the draw, as they do in the game) */
	pass = 0;
	drawn_count[0] = 0;
	draw_unicode_string(record, &bounds, NULL, NULL, 0, text);
	pass = 1;
	drawn_count[1] = 0;
	draw_unicode_string(record, &draw_bounds, NULL, NULL, 0, text);
	CHECK(drawn_count[0] == drawn_count[1], "justification %d, flags %lu: %d glyphs measured, %d drawn (%ls)",
		justification, flags, drawn_count[0], drawn_count[1], text);
	for (index = 0; index < drawn_count[0]; index++)
	{
		CHECK(drawn[0][index].character == drawn[1][index].character && drawn[0][index].x == drawn[1][index].x &&
			drawn[0][index].y == drawn[1][index].y,
			"justification %d, flags %lu: glyph %d '%c' measured at %d,%d, drawn '%c' at %d,%d (%ls)",
			justification, flags, index, drawn[0][index].character, drawn[0][index].x, drawn[0][index].y,
			drawn[1][index].character, drawn[1][index].x, drawn[1][index].y, text);
	}
}

int main(int argc, char **argv)
{
	const char *case_name = argc > 1 ? argv[1] : "";
	rectangle2d box = { 10, 20, 250, 200 };

	/* SELECT PROFILE's "Choose a Profile\r\n\r\nPress %a-button to select": the piece before the icon, left
	justified in a box that wraps, draws its last line ("Press") */
	CASE("last-line-before-an-icon")
	{
		static wchar_t const text[] = L"Choose a Profile\r\n\r\nPress ";
		int index;
		int found = 0;

		check_piece(text, box, box.x0, _text_justification_left, FLAG(_draw_text_wrap_horizontally_bit));
		for (index = 0; index < drawn_count[1]; index++)
			found += drawn[1][index].character == 'P' && drawn[1][index].y > drawn[1][0].y;
		CHECK(found == 1, "the line before the icon was not drawn (%d)", found);
		return 0;
	}
	/* the class: every piece draws where it was measured, in each justification, wrapping or not, from the box's
	edge or from a cursor (the text after an icon) */
	CASE("drawn-where-measured")
	{
		static wchar_t const *const texts[] =
		{
			L"Press ",
			L" to select",
			L"Choose a Profile\r\n\r\nPress ",
			L"a much longer line than the others\r\nshort\r\nend ",
			L"one line that is long enough to wrap in this box at a space ",
		};
		static short const justifications[] =
			{ _text_justification_left, _text_justification_center, _text_justification_right };
		unsigned int text_index;
		unsigned int justification_index;
		int wrap;
		int from_cursor;

		for (text_index = 0; text_index < sizeof(texts) / sizeof(texts[0]); text_index++)
			for (justification_index = 0; justification_index < 3; justification_index++)
				for (wrap = 0; wrap < 2; wrap++)
					for (from_cursor = 0; from_cursor < 2; from_cursor++)
						check_piece(texts[text_index], box, (short)(box.x0 + (from_cursor ? 61 : 0)),
							justifications[justification_index],
							wrap ? FLAG(_draw_text_wrap_horizontally_bit) : 0);
		return 0;
	}
	/* the cursor goes on from where the piece ends (less the game's 3 units), on its last line */
	CASE("cursor-after-the-piece")
	{
		rectangle2d draw_bounds;
		rectangle2d cursor_bounds = box;

		font_drawing_globals.current_justification = _text_justification_left;
		font_drawing_globals.current_flags = FLAG(_draw_text_wrap_horizontally_bit);
		draw_unicode_string_compute_bounds_from_cursor(&box, L"ab\r\n\r\nPress ", &draw_bounds, &cursor_bounds);
		CHECK(cursor_bounds.x0 == box.x0 + 1 + 6 * 7 - 3, "cursor x %d", cursor_bounds.x0);
		CHECK(cursor_bounds.y1 == box.y0 + 2 * 14 + 9 + 3, "cursor bottom %d", cursor_bounds.y1);
		CHECK(draw_bounds.y1 == cursor_bounds.y1 && draw_bounds.y0 == box.y0, "drawn %d..%d", draw_bounds.y0,
			draw_bounds.y1);
		/* the text after the icon: its first line from the icon's end */
		cursor_bounds = box;
		cursor_bounds.x0 = (short)(box.x0 + 61);
		draw_unicode_string_compute_bounds_from_cursor(&box, L" to select", &draw_bounds, &cursor_bounds);
		CHECK(cursor_bounds.x0 == box.x0 + 61 + 1 + 10 * 7 - 3, "cursor x after the icon %d", cursor_bounds.x0);
		return 0;
	}

	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
