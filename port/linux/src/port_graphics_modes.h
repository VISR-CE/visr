/*
PORT_GRAPHICS_MODES.H

The graphics modes (display.graphics): "hardcore", "classic" and "enhanced",
lowest first, so a later mode adds a value above "enhanced" without changing
the others. A mode is a layer over the player's own settings and under the
environment (HALO_*): each row below names a setting the mode replaces and
its value in Hardcore and in Classic (NULL: not replaced in that mode, the
setting's own value stands). Enhanced replaces nothing. The stored setting
is untouched, so it applies again when the player switches back.

Read by the game's settings (port_config.c) and by the iOS host, which reads
a few settings from config.toml itself (port/ios/host/host_config.c). Values
are as the environment gives them: strings without quotes.

Every visual addition beyond the original Xbox gets a row, off in Classic and
Hardcore. tools/test_graphics_modes.py checks each row names a real setting.

In head-tracked stereo (Apple Vision Pro, display.stereo = "head") Hardcore
acts as Classic: a stereo view cannot be the Xbox's 4:3 640x480 picture.
*/

#ifndef PORT_GRAPHICS_MODES_H
#define PORT_GRAPHICS_MODES_H

#include <string.h>

enum graphics_mode
{
	GRAPHICS_MODE_HARDCORE,
	GRAPHICS_MODE_CLASSIC,
	GRAPHICS_MODE_ENHANCED,
	NUMBER_OF_GRAPHICS_MODES
};

/* (name, Hardcore's value, Classic's value) */
#define GRAPHICS_MODE_SETTINGS(X) \
	/* picture size and timing: the Xbox's 640x480, 4:3 with bars at the sides, at 30 frames a second */ \
	X("display.screen_width", "640", NULL) \
	X("display.render_height", "480", NULL) \
	X("display.render_scale", "1.0", NULL) \
	X("display.resolution_scaling", "original", NULL) \
	X("display.upscaler", "bilinear", NULL) \
	X("display.dynamic_resolution", "false", NULL) \
	X("display.frame_rate", "30", NULL) \
	X("display.interpolation", "false", NULL) \
	X("display.direct_camera", "false", NULL) \
	X("display.native_hud", "false", NULL) \
	X("display.anti_aliasing", "off", NULL) \
	X("display.anisotropic_filtering", "1", NULL) \
	X("display.effect_resolution", "xbox", NULL) \
	X("display.model_lod", "auto", NULL) \
	/* art: the maps' own textures, HUD bitmaps, fonts and menus, and the original animations */ \
	X("display.upscaled_textures", "false", NULL) \
	X("display.high_res_hud", "false", NULL) \
	X("display.high_res_text", "false", NULL) \
	X("display.ui_redraw", "false", NULL) \
	X("game.enhanced_animations", "false", NULL) \
	X("display.shadow_map_size", "128", NULL) \
	X("debug.shadow_blur", "xbox", NULL) \
	X("debug.split_screen_hud", "true", NULL) \
	/* the Xbox's limits and hard particle edges: more decals, bodies and particles and soft edges in Classic too */ \
	X("display.effect_limits", "xbox", NULL) \
	X("game.garbage_limit", "xbox", NULL) \
	X("display.soft_particles", "false", NULL) \
	/* additions the Xbox never drew: off in Classic too */ \
	X("display.first_person_legs", "false", "false") \
	X("display.shadow_props", "false", "false") \
	X("display.ambient_occlusion", "false", "false") \
	X("display.per_pixel_lighting", "false", "false") \
	/* a profile's cost caps remove original content: none in Hardcore */ \
	X("display.dynamic_light_limit", "0", NULL) \
	X("display.specular_light_limit", "0", NULL) \
	X("display.shadow_minimum_size", "0", NULL) \
	X("display.clutter_distance", "0.0", NULL)

struct graphics_mode_setting
{
	const char *name;
	/* indexed by enum graphics_mode; NULL where the mode leaves the setting alone */
	const char *value[NUMBER_OF_GRAPHICS_MODES];
};

#define GRAPHICS_MODE_ROW(name, hardcore, classic) { name, { hardcore, classic, NULL } },
static const struct graphics_mode_setting graphics_mode_settings[] = { GRAPHICS_MODE_SETTINGS(GRAPHICS_MODE_ROW) };
#undef GRAPHICS_MODE_ROW

#define NUMBER_OF_GRAPHICS_MODE_SETTINGS (sizeof(graphics_mode_settings) / sizeof(graphics_mode_settings[0]))

static const char *const graphics_mode_names[NUMBER_OF_GRAPHICS_MODES] = { "hardcore", "classic", "enhanced" };

/* display.graphics' text (quotes allowed) as a mode; -1 for one this build does not know (a newer build's), which
the caller treats as Enhanced and logs */
static inline int graphics_mode_parse(const char *text)
{
	size_t length;
	int mode;

	if (!text)
		return -1;
	if (*text == '"')
		text++;
	length = strlen(text);
	if (length && text[length - 1] == '"')
		length--;
	for (mode = 0; mode < NUMBER_OF_GRAPHICS_MODES; mode++)
	{
		if (strlen(graphics_mode_names[mode]) == length && !strncmp(text, graphics_mode_names[mode], length))
			return mode;
	}
	return -1;
}

/* the mode a frame draws in: Hardcore acts as Classic in head-tracked stereo */
static inline int graphics_mode_effective(int mode, int head_stereo)
{
	if (mode < 0 || mode >= NUMBER_OF_GRAPHICS_MODES)
		return GRAPHICS_MODE_ENHANCED;
	return mode == GRAPHICS_MODE_HARDCORE && head_stereo ? GRAPHICS_MODE_CLASSIC : mode;
}

/* the value the mode gives a setting, or NULL where it leaves the setting alone */
static inline const char *graphics_mode_value(int mode, const char *name)
{
	size_t index;

	if (mode < 0 || mode >= NUMBER_OF_GRAPHICS_MODES || !name)
		return NULL;
	for (index = 0; index < NUMBER_OF_GRAPHICS_MODE_SETTINGS; index++)
	{
		if (!strcmp(graphics_mode_settings[index].name, name))
			return graphics_mode_settings[index].value[mode];
	}
	return NULL;
}

#endif
