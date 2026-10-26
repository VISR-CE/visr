/*
HALO_UI_POINTER.H

The mouse in the menus of the desktop builds. While a menu is up, the mouse
is released and its pointer shows (port/linux/src/sdl_platform.c); each frame
the menus (source/interface/ui_widget.c) ask where it is, in their own 640x480
coordinates (port/linux/src/d3d8_device.c undoes the letterbox, the scale and the
widescreen centering), and what it did since.
*/

#ifndef HALO_UI_POINTER_H
#define HALO_UI_POINTER_H

struct halo_ui_pointer
{
	short x, y;
	/* where the latest left click was */
	short click_x, click_y;
	unsigned char moved;
	unsigned char left_clicks;
	unsigned char right_clicks;
	/* whole wheel notches, away from the user positive */
	signed char wheel_steps;
	/* a finger: how far from an item (in menu units) a touch still counts as
	on it, when none contains it; 0 for a mouse */
	short radius;
};

/* frees the mouse for the menus while menus_active, and captures it again
for aiming when not; while menus are active returns nonzero and what the
pointer did since the last call. On iOS a touch or pointer device on the
host (halo_host_pointer.h) is the source; 0 where the host has none (Apple TV,
Vision Pro until its spatial source, a Mac without one). */
int halo_ui_pointer_update(int menus_active, struct halo_ui_pointer *pointer);

/* the menus act on the pointer: kind 1 the focus moved, 2 something was pressed
(halo_host_pointer.h's host_ui_pointer_feedback_kind); a no-op where there is no
host to feel it */
void halo_ui_pointer_feedback(int kind);

#endif
