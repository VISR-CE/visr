#include "menu_scale.h"

void menu_scale_window(float screen_height_points, float content_top, float content_bottom, float *top_unit, float *bottom_unit, float *points_per_unit)
{
	const float padded_top = content_top > MENU_PAD_TOP ? content_top - MENU_PAD_TOP : 0.0f;
	const float padded_bottom = content_bottom + MENU_PAD_BOTTOM < 480.0f ? content_bottom + MENU_PAD_BOTTOM : 480.0f;
	float height = 480.0f;
	float top = 0.0f;

	if (screen_height_points < height)
	{
		height = screen_height_points;
		if (height < padded_bottom - padded_top)
			height = padded_bottom - padded_top;
		top = (padded_top + padded_bottom - height) * 0.5f;
		if (top < 0.0f)
			top = 0.0f;
		if (top + height > 480.0f)
			top = 480.0f - height;
	}
	*top_unit = top;
	*bottom_unit = top + height;
	*points_per_unit = screen_height_points / height;
}
