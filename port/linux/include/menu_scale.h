/* Menu units remain independent of display pixels and device classes. */
#ifndef MENU_SCALE_H
#define MENU_SCALE_H

#define MENU_PAD_TOP 18
#define MENU_PAD_BOTTOM 4
#define MENU_CONTENT_TOP 41
#define MENU_CONTENT_BOTTOM 443

/* The caller measures content in the same 480-unit space as widget drawing. */
void menu_scale_window(float screen_height_points, float content_top, float content_bottom, float *top_unit, float *bottom_unit, float *points_per_unit);

#endif
