/*
HALO_UI_POINTER_MAP.H

A point on the surface that shows the game's picture, in the menus' own
640x480 coordinates (halo_ui_pointer.h). Pure arithmetic, shared by the desktop
build (a mouse in the window), the iOS guest (a finger on the view, in points)
and the probe that checks them against the presenter's fit.

The picture is fit into the drawable, keeping its shape and centered (the
letterboxed display blit at presentation), drawn at the screen's width, and the
menus' 640 columns are centered in that width (halo_screen_ui_offset).
*/

#ifndef HALO_UI_POINTER_MAP_H
#define HALO_UI_POINTER_MAP_H

#include <math.h>

struct halo_ui_pointer_geometry
{
	/* the surface the point is on, in the units the point is in (window
	points, view points), and the drawable's pixels */
	int window_width, window_height;
	int pixel_width, pixel_height;
	/* the picture's shape (the back buffer's GL size) and its pixels */
	int picture_gl_width, picture_gl_height;
	int picture_width, picture_height;
	/* the width the game draws (halo_screen_width) */
	int screen_width;
};

/* *x and *y are -1 where the geometry is unusable */
static inline void halo_ui_pointer_map(const struct halo_ui_pointer_geometry *g, float window_x, float window_y,
	short *x, short *y)
{
	int width, height, left, top;
	float screen_x, screen_y;

	*x = *y = -1;
	if (!g || g->window_width <= 0 || g->window_height <= 0 || g->pixel_width <= 0 || g->pixel_height <= 0 ||
		g->picture_gl_width <= 0 || g->picture_gl_height <= 0 || g->picture_width <= 0 || g->picture_height <= 0)
		return;
	width = g->pixel_width;
	height = (int)((long)g->pixel_width * g->picture_gl_height / g->picture_gl_width);
	if (height > g->pixel_height)
	{
		height = g->pixel_height;
		width = (int)((long)g->pixel_height * g->picture_gl_width / g->picture_gl_height);
	}
	if (width <= 0 || height <= 0)
		return;
	left = (g->pixel_width - width) / 2;
	top = (g->pixel_height - height) / 2;
	screen_x = (window_x * g->pixel_width / g->window_width - left) * (float)g->picture_width / (float)width;
	screen_y = (window_y * g->pixel_height / g->window_height - top) * (float)g->picture_height / (float)height;
	*x = (short)floorf(screen_x - (float)(g->screen_width - 640) / 2.0f);
	*y = (short)floorf(screen_y);
}

/* a fraction of the picture (u from its left, v from its top) in the menus'
coordinates, for a surface that shows the picture whole at its own shape (the
Vision Pro's, halo_host_pointer.h's picture): the picture is the screen's width
by 480 lines, the menus' 640 columns centered in it. -1 outside the picture's
-0.5 to 1.5 or with no width */
static inline void halo_ui_pointer_map_picture(float u, float v, int screen_width, short *x, short *y)
{
	*x = *y = -1;
	if (screen_width <= 0 || !(u >= -0.5f && u <= 1.5f && v >= -0.5f && v <= 1.5f))
		return;
	*x = (short)floorf(u * (float)screen_width - (float)(screen_width - 640) / 2.0f);
	*y = (short)floorf(v * 480.0f);
}

/* a length on the surface (points) in menu units, at the picture's scale */
static inline float halo_ui_pointer_length(const struct halo_ui_pointer_geometry *g, float surface_length)
{
	int width;

	if (!g || g->window_width <= 0 || g->pixel_width <= 0 || g->picture_gl_width <= 0 || g->picture_gl_height <= 0 ||
		g->pixel_height <= 0)
		return 0.0f;
	width = g->pixel_width;
	if ((long)g->pixel_width * g->picture_gl_height / g->picture_gl_width > g->pixel_height)
		width = (int)((long)g->pixel_height * g->picture_gl_width / g->picture_gl_height);
	if (width <= 0)
		return 0.0f;
	return surface_length * g->pixel_width / g->window_width * (float)g->picture_width / (float)width;
}

#endif
