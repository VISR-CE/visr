/*
HALO_HOST_POINTER.H

What the iOS host (port/ios/host/host_menu_pointer.m) tells the guest's menus
about fingers and pointer devices (halo_ui_pointer_update, d3d8_device.c), and
what the menus tell it back. The struct crosses between the guest and the host
as it is: fixed-width fields only.

Positions are on the surface that shows the picture (the game's view, or the
phone's view of the companion mirror), in points, with that surface's size in
the same units. Events accumulate between reads and a read clears them.
*/

#ifndef HALO_HOST_POINTER_H
#define HALO_HOST_POINTER_H

#include <stdint.h>

struct host_ui_pointer
{
	/* the host has a pointer source installed: constant while it is, whether
	or not a finger is down (a frame that reports 0 makes the menus forget
	the presses they have queued) */
	int32_t active;
	int32_t moved;
	int32_t left_clicks;
	int32_t right_clicks;
	/* whole notches: positive down the list, as a wheel turned toward the user and a finger moving up the screen scroll it */
	int32_t wheel_steps;
	float x, y;
	float click_x, click_y;
	float surface_width, surface_height;
	/* how far from an item a touch still counts as on it, in points; 0 for a
	pointer device */
	float radius;
	/* 1: x, y and the click are on a surface that shows the picture whole,
	stretched to its shape (the Vision Pro's menu surfaces, host_menu_gaze.h):
	fractions of the picture times the surface's size, with no letterbox to
	undo; 0: a surface the picture is fit into, keeping its shape */
	int32_t picture;
	/* whole swipes in from the left edge since the last read: BACK on a touch screen */
	int32_t back_swipes;
	/* a finger is down and still a tap candidate (not scrolling, not an edge swipe): the menus time a press-and-hold from it */
	int32_t down;
	/* Persistent finger/pinch or left mouse button state; reads do not clear it. */
	int32_t held;
	/* the pixels of the drawable the picture is fit into on that surface, which the
	surface then shows stretched to its own size: a drawable whose shape lags the
	surface's (a rotation, a move between screens) moves where the picture lands.
	0 when unknown: the surface's own shape */
	float drawable_width, drawable_height;
};

_Static_assert(sizeof(struct host_ui_pointer) == 72, "host_ui_pointer crosses between the guest and the host");

/* host_ui_pointer_feedback(kind) */
enum host_ui_pointer_feedback_kind
{
	HOST_UI_POINTER_FEEDBACK_FOCUS = 1,
	HOST_UI_POINTER_FEEDBACK_ACTIVATE = 2,
	/* a press-and-hold has begun to register (light), and has opened its menu (medium) */
	HOST_UI_POINTER_FEEDBACK_HOLD_START = 3,
	HOST_UI_POINTER_FEEDBACK_HOLD_OPEN = 4
};

#endif
