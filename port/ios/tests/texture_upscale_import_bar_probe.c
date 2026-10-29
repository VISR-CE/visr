/* Apache-2.0. The import phase's bar and its "ready to start" mark (port/runtime/texture_upscale_import_bar.h). */
#include "texture_upscale_import_bar.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int near(double a, double b) { return fabs(a - b) < 1e-9; }

int main(void)
{
	/* the whole set and the main menu as measured: 103351424 units, the menu 15820352 of them */
	const unsigned long long all = 103351424ull, menu = 15820352ull;
	struct texture_upscale_import_inputs in = {0};
	double previous;

	/* nothing yet: 0, and no mark */
	assert(near(texture_upscale_import_fraction(&in), 0));
	assert(texture_upscale_import_tick(&in) < 0);
	/* the model loaded: its share; the catalog by maps */
	in.model_loaded = 1;
	assert(near(texture_upscale_import_fraction(&in), TEXTURE_UPSCALE_IMPORT_MODEL_SHARE));
	in.maps = 20;
	in.maps_done = 10;
	assert(near(texture_upscale_import_fraction(&in), TEXTURE_UPSCALE_IMPORT_MODEL_SHARE + TEXTURE_UPSCALE_IMPORT_CATALOG_SHARE / 2));
	assert(texture_upscale_import_tick(&in) < 0);   /* (not known before the catalog) */
	/* the catalog before the model (the loading picture was made on an earlier launch): the catalog's share alone */
	{
		struct texture_upscale_import_inputs early = {0, 1, 20, 20, 0, all, 1, 0, menu};
		assert(near(texture_upscale_import_fraction(&early), TEXTURE_UPSCALE_IMPORT_CATALOG_SHARE));
	}
	/* the catalog built: the setup is full, the mark is where the menu's units end */
	in.catalog_ready = 1;
	in.units = all;
	in.menu_known = 1;
	in.menu_units = menu;
	assert(near(texture_upscale_import_fraction(&in), TEXTURE_UPSCALE_IMPORT_SETUP_SHARE));
	assert(near(texture_upscale_import_tick(&in),
		TEXTURE_UPSCALE_IMPORT_SETUP_SHARE + (1 - TEXTURE_UPSCALE_IMPORT_SETUP_SHARE) * (double)menu / all));
	/* half the menu made: the bar is before the mark, the mark has not moved */
	previous = texture_upscale_import_tick(&in);
	in.ready_units = in.menu_ready_units = menu / 2;
	assert(texture_upscale_import_fraction(&in) < previous);
	assert(near(texture_upscale_import_tick(&in), previous));
	/* the menu made: the bar reaches the mark exactly */
	in.ready_units = in.menu_ready_units = menu;
	assert(near(texture_upscale_import_fraction(&in), previous));
	/* a phase that resumes with other maps' textures already made: the mark is past them */
	{
		struct texture_upscale_import_inputs resumed = {1, 1, 20, 20, 40000000ull, all, 1, 0, menu};
		assert(near(texture_upscale_import_tick(&resumed),
			TEXTURE_UPSCALE_IMPORT_SETUP_SHARE + (1 - TEXTURE_UPSCALE_IMPORT_SETUP_SHARE) * (40000000.0 + menu) / all));
	}
	/* every texture made: the whole bar */
	in.ready_units = all;
	assert(near(texture_upscale_import_fraction(&in), 1));
	/* no menu map to make: the mark sits at the end of the setup */
	{
		struct texture_upscale_import_inputs none = {1, 1, 20, 20, 0, all, 1, 0, 0};
		assert(near(texture_upscale_import_tick(&none), TEXTURE_UPSCALE_IMPORT_SETUP_SHARE));
	}
	/* an empty set does not divide by zero */
	{
		struct texture_upscale_import_inputs empty = {1, 1, 0, 0, 0, 0, 1, 0, 0};
		assert(near(texture_upscale_import_fraction(&empty), TEXTURE_UPSCALE_IMPORT_SETUP_SHARE));
	}
	/* what is shown never goes back (a catalog built again starts its maps at 0) */
	assert(near(texture_upscale_import_shown(0.4, 0.05), 0.4));
	assert(near(texture_upscale_import_shown(0.4, 0.5), 0.5));
	puts("texture_upscale_import_bar_probe: ok");
	return 0;
}
