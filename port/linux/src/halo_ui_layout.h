/* Portrait menu arithmetic shared by drawing and pointer mapping. The HUD
   keeps its own layout; these values apply only inside a centered UI span. */
#ifndef HALO_UI_LAYOUT_H
#define HALO_UI_LAYOUT_H

#include <math.h>

static inline float halo_ui_layout_scale(int screen_width)
{
	return screen_width > 0 && screen_width < 640 ? (float)screen_width / 640.0f : 1.0f;
}

static inline float halo_ui_layout_y_offset(int screen_width)
{
	return 240.0f * (1.0f - halo_ui_layout_scale(screen_width));
}

/* The columns left of the 640 design columns centered on a screen that wide:
whole columns, as they are drawn, so an odd width puts the extra column on
the right (negative when the screen is narrower, as a backdrop covering it
crops). The drawing, the screen in menu units and the pointer all take it
from here. */
static inline int halo_ui_layout_margin(int screen_width)
{
	return (screen_width - 640) / 2;
}

/* The screen's center column in the design's units: 320, or 320.5 on an odd
width; portrait fits the design to the width, centered. */
static inline float halo_ui_layout_center(int screen_width)
{
	return screen_width >= 640 ? (float)screen_width * 0.5f - (float)halo_ui_layout_margin(screen_width) : 320.0f;
}

/* UI clears arrive in design coordinates: clipping them to the narrow
viewport first would discard the right side before it can be scaled. */
static inline int halo_ui_layout_clip_rect(float scale, float y_offset, const float rect[4],
	const float viewport[4], float fitted[4])
{
	fitted[0] = fmaxf(rect[0] * scale, viewport[0]);
	fitted[1] = fmaxf(rect[1] * scale + y_offset, viewport[1]);
	fitted[2] = fminf(rect[2] * scale, viewport[2]);
	fitted[3] = fminf(rect[3] * scale + y_offset, viewport[3]);
	return fitted[0] < fitted[2] && fitted[1] < fitted[3];
}

#endif
