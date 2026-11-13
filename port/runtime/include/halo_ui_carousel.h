/*
HALO_UI_CAROUSEL.H

The three-card spinner's nonwrapping window, geometry, and input rules.
Scroll is in items; halo_ui_drag offsets are in menu units. The widget keeps
its own binding and focus, so rendering a peek never changes the focused item.
*/
#ifndef HALO_UI_CAROUSEL_H
#define HALO_UI_CAROUSEL_H

#include "halo_ui_drag.h"

#define HALO_UI_CAROUSEL_PITCH 167.0f
#define HALO_UI_CAROUSEL_CARD_WIDTH 158.0f
#define HALO_UI_CAROUSEL_FRAME_WIDTH 640.0f
/* Half of a 65-unit peek: the outer half of each peek fades to nothing. */
#define HALO_UI_CAROUSEL_FADE_WIDTH 32.0f

static inline int halo_ui_carousel_max_first(int count)
{
	return count > 3 ? count - 3 : 0;
}

static inline int halo_ui_carousel_clamp_first(int first, int count)
{
	int maximum = halo_ui_carousel_max_first(count);
	return first < 0 ? 0 : first > maximum ? maximum : first;
}

/* An unfilled child is unbound rather than wrapping around to item zero. */
static inline int halo_ui_carousel_bound_item(int first, int slot, int count)
{
	int item = halo_ui_carousel_clamp_first(first, count) + slot;
	return slot >= 0 && slot < 3 && item < count ? item : -1;
}

static inline int halo_ui_carousel_settled_first(float offset, int count)
{
	return halo_ui_carousel_clamp_first((int)roundf(offset / HALO_UI_CAROUSEL_PITCH), count);
}

static inline float halo_ui_carousel_origin_x(int count)
{
	int full = count > 3 ? 3 : count > 0 ? count : 0;
	float span = full ? (full - 1) * HALO_UI_CAROUSEL_PITCH + HALO_UI_CAROUSEL_CARD_WIDTH : 0.0f;
	return (HALO_UI_CAROUSEL_FRAME_WIDTH - span) * 0.5f;
}

static inline float halo_ui_carousel_item_x(int item, float scroll, int count)
{
	return halo_ui_carousel_origin_x(count) + (item - (count > 3 ? scroll : 0.0f)) * HALO_UI_CAROUSEL_PITCH;
}

/* [begin, end) includes every card intersecting the frame, including peeks.
   Strict edges exclude cards that only touch the frame without visible pixels. */
static inline void halo_ui_carousel_visible_range(float scroll, int count, int *begin, int *end)
{
	float origin = halo_ui_carousel_origin_x(count);
	int low, high;
	if (count <= 3)
	{
		*begin = 0;
		*end = count > 0 ? count : 0;
		return;
	}
	low = (int)floorf(scroll - (origin + HALO_UI_CAROUSEL_CARD_WIDTH) / HALO_UI_CAROUSEL_PITCH) + 1;
	high = (int)ceilf(scroll + (HALO_UI_CAROUSEL_FRAME_WIDTH - origin) / HALO_UI_CAROUSEL_PITCH);
	*begin = low < 0 ? 0 : low > count ? count : low;
	*end = high < 0 ? 0 : high > count ? count : high;
}

/* Opacity at menu x inside a frame [x0, x1): 0 at either edge, rising linearly
   to 1 over HALO_UI_CAROUSEL_FADE_WIDTH. Linear, so a quad split at the fade
   breakpoints interpolates it exactly from its corner colors. */
static inline float halo_ui_carousel_fade(float x, float x0, float x1)
{
	float edge = x - x0 < x1 - x ? x - x0 : x1 - x;
	return edge <= 0.0f ? 0.0f : edge >= HALO_UI_CAROUSEL_FADE_WIDTH ? 1.0f : edge / HALO_UI_CAROUSEL_FADE_WIDTH;
}

/* A settled drag keeps its old focus whenever that item is still full. */
static inline int halo_ui_carousel_focus_in_window(int focus, int first, int count)
{
	int last;
	if (count <= 0)
		return -1;
	first = halo_ui_carousel_clamp_first(first, count);
	last = first + 2 < count ? first + 2 : count - 1;
	return focus < first ? first : focus > last ? last : focus;
}

/* The caller supplies an item focus, not the focused child slot. */
static inline int halo_ui_carousel_controller_move(struct halo_ui_drag *drag, int *focus, int *first,
	int count, int direction)
{
	int next, new_first;
	if (!direction || count <= 0)
		return 0;
	next = *focus + (direction > 0 ? 1 : -1);
	if (next < 0 || next >= count)
		return 0;
	new_first = halo_ui_carousel_clamp_first(*first, count);
	if (next < new_first)
		new_first = next;
	else if (next > new_first + 2)
		new_first = next - 2;
	new_first = halo_ui_carousel_clamp_first(new_first, count);
	*focus = next;
	if (new_first != *first)
	{
		*first = new_first;
		halo_ui_drag_glide(drag, new_first * HALO_UI_CAROUSEL_PITCH);
	}
	return 1;
}

enum halo_ui_carousel_tap
{
	HALO_UI_CAROUSEL_TAP_NONE,
	HALO_UI_CAROUSEL_TAP_ACTIVATE,
	HALO_UI_CAROUSEL_TAP_PEEK_LEFT,
	HALO_UI_CAROUSEL_TAP_PEEK_RIGHT
};

/* Call only for a confirmed tap. The caller checks the cards' vertical band.
   A partially clipped card scrolls toward its side; a full card activates. */
static inline enum halo_ui_carousel_tap halo_ui_carousel_tap(float x, float scroll, int count, int *item)
{
	int begin, end;
	*item = -1;
	if (x < 0.0f || x >= HALO_UI_CAROUSEL_FRAME_WIDTH)
		return HALO_UI_CAROUSEL_TAP_NONE;
	halo_ui_carousel_visible_range(scroll, count, &begin, &end);
	for (int index = begin; index < end; index++)
	{
		float left = halo_ui_carousel_item_x(index, scroll, count);
		if (x < left || x >= left + HALO_UI_CAROUSEL_CARD_WIDTH)
			continue;
		*item = index;
		if (left < 0.0f)
			return HALO_UI_CAROUSEL_TAP_PEEK_LEFT;
		if (left + HALO_UI_CAROUSEL_CARD_WIDTH > HALO_UI_CAROUSEL_FRAME_WIDTH)
			return HALO_UI_CAROUSEL_TAP_PEEK_RIGHT;
		return HALO_UI_CAROUSEL_TAP_ACTIVATE;
	}
	return HALO_UI_CAROUSEL_TAP_NONE;
}

#endif
