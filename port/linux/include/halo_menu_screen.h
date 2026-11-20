/* The screen as the menus see it (port/linux/src/d3d8_device.c, halo_screen_menu_screen_rect): in menu units,
   the 640x480 design's own coordinates, where the whole screen reaches and where its safe area (clear of a camera
   cutout or rounded corner) lies, for the draw going on now. Full-screen fills reach the screen's rectangle; controls
   at the edges stay inside the safe one. */
#ifndef HALO_MENU_SCREEN_H
#define HALO_MENU_SCREEN_H

struct halo_menu_screen
{
	/* the whole screen, rounded outward */
	long x0, y0, x1, y1;
	/* its safe area, rounded inward */
	long safe_x0, safe_y0, safe_x1, safe_y1;
};

/* zoomed: the menu window's draw (its safe-area zoom and minimum-scale crop: menu_window_geometry), as the
   widgets, their pointer and the in-game keyboard see it; 0 for a draw outside it (the fade) */
void halo_screen_menu_screen_rect(int zoomed, struct halo_menu_screen *screen);

#endif
