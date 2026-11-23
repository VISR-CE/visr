/* Portrait field of view modes, a live A/B for the debug chord. Pure arithmetic
with no game headers, so a probe can test it on its own. All take the tangent of
half the vertical angle the game's camera already has (BASE_TANGENT is the
nominal 4:3 one, 0.75 * tan 35 degrees) and the view's width / height, and return
the tangent of half the vertical angle to use.
  0 keep vertical: the game's vertical angle, the horizontal one narrows
  1 horizontal minimum: widen the vertical angle until the horizontal is 60 degrees
  2 horizontal 70: the 4:3 view's horizontal angle across the width
  3 match landscape: 73.5 degrees horizontal, the 16:9 landscape view's width
Modes 2 and 3 scale with the camera's own angle, so a zoom keeps its ratio. Mode 3
on a phone passes 90 degrees vertical; the rasterizer camera only needs it under
180 (render_cameras.c), and the game's own 90-degree checks see the observer's
angle before this override. */
#ifndef PORTRAIT_FOV_H
#define PORTRAIT_FOV_H

#include <math.h>
#include <string.h>

#define PORTRAIT_FOV_MODE_COUNT 4
#define PORTRAIT_FOV_BASE_TANGENT 0.525f /* 0.75 * tan(35 degrees), the 4:3 view's */

static inline float portrait_fov_tangent(int mode, float aspect, float base_tangent)
{
	const float scale = base_tangent / PORTRAIT_FOV_BASE_TANGENT;
	float minimum;

	if (aspect <= 0.0f)
		return base_tangent;
	switch (mode)
	{
	case 1:
		minimum = 0.57735027f / aspect; /* tan(30 degrees) */
		return base_tangent < minimum ? minimum : base_tangent;
	case 2:
		return 0.70020754f / aspect * scale; /* tan(35 degrees) */
	case 3:
		return 0.74673580f / aspect * scale; /* tan(36.75 degrees) */
	default:
		return base_tangent;
	}
}

/* The mode debug.portrait_field_of_view names. Empty or unset is the default, 1 (horizontal
minimum: a 60 degree horizontal angle at least, the chosen default); "hold" keeps the game's vertical
angle; an unknown name is the default too. */
static inline int portrait_fov_mode_from_name(const char *name)
{
	if (name && strcmp(name, "hold") == 0)
		return 0;
	if (name && strcmp(name, "horizontal_70") == 0)
		return 2;
	if (name && strcmp(name, "match_landscape") == 0)
		return 3;
	return 1;
}

/* the degrees across the width and the height for a half-vertical tangent */
static inline void portrait_fov_degrees(float vertical_tangent, float aspect, float *horizontal, float *vertical)
{
	const float to_degrees = 57.29577951f;

	*horizontal = 2.0f * atanf(vertical_tangent * aspect) * to_degrees;
	*vertical = 2.0f * atanf(vertical_tangent) * to_degrees;
}

#endif
