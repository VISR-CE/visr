/* Portrait field of view modes on the iPhone Duo's inner display (width over height about 0.705). */
#include "../../../source/main/portrait_fov.h"
#include <assert.h>
#include <math.h>

static int within(float a, float b, float tolerance) { return fabsf(a - b) < tolerance; }

static void check(int mode, float aspect, float base, float horizontal, float vertical)
{
	float h, v;

	portrait_fov_degrees(portrait_fov_tangent(mode, aspect, base), aspect, &h, &v);
	assert(within(h, horizontal, 0.3f));
	assert(within(v, vertical, 0.3f));
}

int main(void)
{
	const float duo = 0.705f;
	const float base = PORTRAIT_FOV_BASE_TANGENT;

	check(0, duo, base, 40.5f, 55.4f);   /* keep vertical */
	check(1, duo, base, 60.0f, 78.9f);   /* horizontal minimum */
	check(2, duo, base, 70.0f, 89.7f);   /* the 4:3 view's width */
	check(3, duo, base, 73.5f, 93.0f);   /* match landscape, past 90 vertical */

	/* the default (empty or unset) is horizontal minimum, the chosen default; hold is the override */
	assert(portrait_fov_mode_from_name(NULL) == 1);
	assert(portrait_fov_mode_from_name("") == 1);
	assert(portrait_fov_mode_from_name("horizontal_minimum") == 1);
	assert(portrait_fov_mode_from_name("hold") == 0);
	assert(portrait_fov_mode_from_name("horizontal_70") == 2);
	assert(portrait_fov_mode_from_name("match_landscape") == 3);
	/* the minimum never narrows a view that is already wide enough */
	assert(within(portrait_fov_tangent(1, 1.2f, 0.9f), 0.9f, 0.0001f));
	/* zoom keeps its ratio in modes 2 and 3: half the tangent stays half */
	assert(within(portrait_fov_tangent(2, duo, base * 0.5f), portrait_fov_tangent(2, duo, base) * 0.5f, 0.0001f));
	assert(within(portrait_fov_tangent(3, duo, base * 0.5f), portrait_fov_tangent(3, duo, base) * 0.5f, 0.0001f));
	/* an unusable aspect leaves the camera alone */
	assert(within(portrait_fov_tangent(3, 0.0f, base), base, 0.0001f));
	/* modes stay below 180 degrees vertical on a narrow phone (render_cameras.c's check) */
	{
		float h, v;
		portrait_fov_degrees(portrait_fov_tangent(3, 0.45f, base), 0.45f, &h, &v);
		assert(v < 179.0f);
	}
	return 0;
}
