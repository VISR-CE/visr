/* Runtime spinner classification shared by menu rendering and pointer dispatch. */
#ifndef HALO_MENU_ROW_H
#define HALO_MENU_ROW_H
#include <wchar.h>
#include <stdlib.h>
#include <math.h>

enum halo_menu_row_kind { HALO_ROW_STATIC, HALO_ROW_TOGGLE, HALO_ROW_ENUM, HALO_ROW_RANGE };
struct halo_menu_row { enum halo_menu_row_kind kind; int on_index; };
typedef wchar_t const *(*halo_menu_row_string)(void *context, int index);

static inline int halo_menu_row_word(wchar_t const *text, char const *word)
{
	if (!text) return 0;
	for (; *word; text++, word++)
	{
		unsigned int ch = *text;
		if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
		if (ch != (unsigned char)*word) return 0;
	}
	return !*text;
}

static inline struct halo_menu_row halo_menu_row_classify(int count, halo_menu_row_string string, void *context)
{
	struct halo_menu_row row = { HALO_ROW_ENUM, -1 };
	int index;
	double previous = 0;
	int suffix = -1; /* every label shares one unit suffix: none, "x" or "%" */
	if (count == 1) { row.kind = HALO_ROW_STATIC; return row; }
	if (count == 2)
	{
		static char const *const pairs[][2] = {{"ON", "OFF"}, {"YES", "NO"}, {"ENABLED", "DISABLED"}};
		for (index = 0; index < 3; index++)
		{
			int on;
			for (on = 0; on < 2; on++)
				if (halo_menu_row_word(string(context, on), pairs[index][0]) &&
					halo_menu_row_word(string(context, 1 - on), pairs[index][1]))
				{ row.kind = HALO_ROW_TOGGLE; row.on_index = on; return row; }
		}
	}
	if (count < 4) return row;
	/* a range may start with a word for nothing ("NONE", "OFF") below its numbers: NONE, 1, 2, 3, 4 is a range of
	five, its first value zero. Any other word ("SAME", "AUTO", "DISPLAY") is a choice of its own, not an end of
	the range, so such a list stays a list of choices. */
	index = halo_menu_row_word(string(context, 0), "NONE") || halo_menu_row_word(string(context, 0), "OFF") ? 1 : 0;
	for (; index < count; index++)
	{
		/* Parse ASCII numbers directly: guest wchar_t is UTF-16, host wchar_t may not be. */
		char number[96], *end;
		wchar_t const *text = string(context, index);
		int length = 0;
		double value;
		int unit = 0;
		if (!text) return row;
		while (text[length] && length < (int)sizeof(number) - 1)
		{
			if (text[length] > 127) return row;
			number[length] = (char)text[length];
			length++;
		}
		if (text[length] || !length) return row;
		number[length] = 0;
		value = strtod(number, &end);
		if (end != number && (*end == 'x' || *end == 'X') && !end[1]) unit = 1;
		else if (end != number && *end == '%' && !end[1]) unit = 2;
		else if (end == number || *end) return row;
		if (!isfinite(value) || (suffix >= 0 && value <= previous) || (suffix >= 0 && unit != suffix)) return row;
		/* (the numbers after a leading word for nothing are above it) */
		if (suffix < 0 && index && value <= 0) return row;
		suffix = unit;
		previous = value;
	}
	row.kind = HALO_ROW_RANGE;
	return row;
}
/* how an option row is drawn and acted on for an input (halo_menu_pointer_controls: 0 none, 1 mouse, 2 touch): a
controller and a keyboard step by the left and right arrows; a mouse and a finger see none and act on the row
itself, with a switch, a dropdown or a slider. The renderer draws from this and nothing else. */
struct halo_menu_row_look { int arrows, toggle, dropdown, slider; };

static inline int halo_menu_row_arrows_shown(int pointer_controls)
{
	return pointer_controls == 0;
}

static inline struct halo_menu_row_look halo_menu_row_look(enum halo_menu_row_kind kind, int has_dropdown,
	int pointer_controls)
{
	struct halo_menu_row_look look;

	look.arrows = halo_menu_row_arrows_shown(pointer_controls);
	look.toggle = pointer_controls && kind == HALO_ROW_TOGGLE;
	look.dropdown = pointer_controls && has_dropdown;
	look.slider = pointer_controls && kind == HALO_ROW_RANGE;
	return look;
}

#endif
