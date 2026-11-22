/*
HALO_MENU_META.H

What the menus' data says about a widget, one record for each tag it speaks of (port/linux/game/menu_tags.c
keeps them; pc_menu_meta finds one). The engine reads a widget's part in a screen here and nowhere else: never
off the tag's name, its place in a list or its picture.

The generator and the patches say these in the menus' files (a widget's row="tall", a list's visible_rows=);
menu_tags.c notes them as it builds the tags.
*/

#ifndef HALO_MENU_META_H
#define HALO_MENU_META_H

/* what the widget is in its screen */
enum halo_menu_role
{
	HALO_MENU_ROLE_NONE,
	HALO_MENU_ROLE_ROW,        /* a row of an option list */
	HALO_MENU_ROLE_TITLE,      /* the screen's title: text (title_strings), drawn with the title element's look */
	HALO_MENU_ROLE_BAR_ACTION, /* an action of the button bar */
	HALO_MENU_ROLE_ROW_ACTION  /* an action bound to a row or a panel, outside the bar */
};

/* an action's button, which is also its place in the bar: left to right X, Y, B, A */
enum halo_menu_slot
{
	HALO_MENU_SLOT_NONE,
	HALO_MENU_SLOT_X,
	HALO_MENU_SLOT_Y,
	HALO_MENU_SLOT_B,
	HALO_MENU_SLOT_A
};

/* where an action shows: a screen-wide one on every input, select and back not on touch (a tap on the item, and
the title's chevron), a pointer-only one in B's place on touch and nowhere with a controller */
enum halo_menu_scope
{
	HALO_MENU_SCOPE_NONE,
	HALO_MENU_SCOPE_GLOBAL,
	HALO_MENU_SCOPE_SELECT,
	HALO_MENU_SCOPE_BACK,
	HALO_MENU_SCOPE_POINTER
};

/* what an action's key does while a text field is being typed in: acts, or is a letter (the keyboard's E is X), so
only a tap or a click on the action runs it */
enum halo_menu_typing
{
	HALO_MENU_TYPING_KEY,
	HALO_MENU_TYPING_POINTER
};

/* what draws focus, for the one focus rule (ui_widget.c's ui_widget_port_draws_focus): a row of a list (its selected
frame, white text, plasma) draws it on every input, touch too; a carousel card's outline is not drawn on touch */
enum halo_menu_focus_item
{
	HALO_MENU_FOCUS_ROW,
	HALO_MENU_FOCUS_CARD
};

/* whether a focused item of the kind draws as focused (ui_widget.c) */
int ui_widget_port_draws_focus(enum halo_menu_focus_item item, int focused);

struct halo_menu_meta
{
	long tag_index;
	unsigned char role;        /* enum halo_menu_role */
	unsigned char slot;        /* enum halo_menu_slot */
	unsigned char scope;       /* enum halo_menu_scope */
	unsigned char typing;      /* enum halo_menu_typing: a bar action's key while a text field is being typed in */
	unsigned char tall;        /* a standard tall row: the tall art, the standard tap band, the standard value kinds */
	short visible_rows;        /* a list that shows a window of its rows: how many (0: all of them) */
	long title_strings;        /* a title's text: a string list tag, one string for each frame of its picture (NONE: not a title) */
};

/* the record of a tag; one of nothing (every field 0, its tag and title_strings NONE) for a tag the menus say nothing of, so a
caller reads a field without a test */
struct halo_menu_meta const *pc_menu_meta(long tag_index);

#endif
