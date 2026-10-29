/* Apache-2.0. The import phase's "Upscaling textures" bar (host_import.m, host_tv_import.m) and the progress iOS shows
for its continued processing: one fraction over the setup and the textures, and where on it "Start playing" appears.
Header-only, so a host probe can check it without the app.

Before the first texture is counted the upscaler loads its model and lists every map's textures (the catalog); on a
phone that takes several seconds and adds no texture units, so a bar of units alone sits at 0%. The setup gets a fixed
share at the start of the bar: the model when it has loaded, the catalog by maps classified. The textures fill the
rest by work units (input texels, texture_cache_progress), advancing as each texture is published. Measured on an M6
Mac (2026-10-28): model 2.4 s, catalog about 6 s, textures 75 s; the shares round the setup up, since a phone's
storage makes the catalog relatively slower. */
#pragma once

#define TEXTURE_UPSCALE_IMPORT_MODEL_SHARE 0.03
#define TEXTURE_UPSCALE_IMPORT_CATALOG_SHARE 0.07
#define TEXTURE_UPSCALE_IMPORT_SETUP_SHARE (TEXTURE_UPSCALE_IMPORT_MODEL_SHARE + TEXTURE_UPSCALE_IMPORT_CATALOG_SHARE)

struct texture_upscale_import_inputs
{
	int model_loaded;                  /* the generator was made during this phase (or the phase found it made) */
	int catalog_ready;                 /* the catalog is built: maps_done and maps no longer matter */
	unsigned maps_done, maps;          /* maps classified so far, of the maps found, while the catalog is built */
	unsigned long long ready_units, units;            /* the whole set */
	int menu_known;                                    /* the catalog lists the main menu's map (or found none) */
	unsigned long long menu_ready_units, menu_units;  /* the main menu's textures */
};

static inline double texture_upscale_import_clamp(double value)
{
	return value < 0 ? 0 : value > 1 ? 1 : value;
}

/* where the textures' part of the bar is when ready of total units are done */
static inline double texture_upscale_import_texture_position(unsigned long long ready, unsigned long long total)
{
	return TEXTURE_UPSCALE_IMPORT_SETUP_SHARE +
		(1 - TEXTURE_UPSCALE_IMPORT_SETUP_SHARE) * (total ? texture_upscale_import_clamp((double)ready / total) : 0);
}

/* the bar's fill, 0 to 1 */
static inline double texture_upscale_import_fraction(const struct texture_upscale_import_inputs *in)
{
	double catalog = in->catalog_ready ? 1 : in->maps ? texture_upscale_import_clamp((double)in->maps_done / in->maps) : 0;
	double setup = (in->model_loaded ? TEXTURE_UPSCALE_IMPORT_MODEL_SHARE : 0) + TEXTURE_UPSCALE_IMPORT_CATALOG_SHARE * catalog;

	/* (the textures' part counts only once the setup is done: units are not known before the catalog) */
	if (!in->catalog_ready || !in->model_loaded)
		return setup;
	return texture_upscale_import_texture_position(in->ready_units, in->units);
}

/* Where "Start playing" appears: the main menu's textures are made first, so the whole set reaches that point when
   the units made outside the menu so far plus all of the menu's are ready. -1 while it is not known (no catalog yet).
   A menu with nothing to make puts it at the end of the setup. */
static inline double texture_upscale_import_tick(const struct texture_upscale_import_inputs *in)
{
	unsigned long long outside, at;

	if (!in->catalog_ready || !in->menu_known)
		return -1;
	outside = in->ready_units > in->menu_ready_units ? in->ready_units - in->menu_ready_units : 0;
	at = outside + in->menu_units;
	return texture_upscale_import_texture_position(at, in->units);
}

/* What the bar shows: never less than it showed before. A catalog built again (the app came back to the foreground)
   or a model made again must not move it backward. */
static inline double texture_upscale_import_shown(double previous, double now)
{
	return now > previous ? now : previous;
}
