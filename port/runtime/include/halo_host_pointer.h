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
	/* whole notches: positive when the finger or wheel moves down the screen */
	int32_t wheel_steps;
	float x, y;
	float click_x, click_y;
	float surface_width, surface_height;
	/* how far from an item a touch still counts as on it, in points; 0 for a
	pointer device */
	float radius;
};

_Static_assert(sizeof(struct host_ui_pointer) == 48, "host_ui_pointer crosses between the guest and the host");

/* host_ui_pointer_feedback(kind) */
enum host_ui_pointer_feedback_kind
{
	HOST_UI_POINTER_FEEDBACK_FOCUS = 1,
	HOST_UI_POINTER_FEEDBACK_ACTIVATE = 2
};

#endif
