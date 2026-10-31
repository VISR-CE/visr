/* Apache-2.0. The original menu pictures retained from the player's ui.map, shared by policy and drawing. */
#ifndef MENU_PICTURE_UPSCALED_H
#define MENU_PICTURE_UPSCALED_H

#include <stddef.h>
#include <string.h>

static const char *const menu_picture_upscaled_tags[] = {
	"ui\\shell\\bitmaps\\mp_map_grafix",
	"ui\\shell\\bitmaps\\sp_levels",
	"ui\\shell\\main_menu\\difficulty_select\\difficulty_options",
	"ui\\shell\\main_menu\\difficulty_select\\difficulty_options_small",
	"ui\\shell\\main_menu\\halo_logo",
	"ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\color_edit\\player_color_marine_large",
	"ui\\shell\\bitmaps\\colors_sm",
	"ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\playlist_edit\\gametype_options",
	"ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\profile_options",
	"ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\controller_edit\\config_controller"
};

static inline int menu_picture_upscaled_tag(const char *tag)
{
	size_t index;

	for (index = 0; tag && index < sizeof(menu_picture_upscaled_tags) / sizeof(menu_picture_upscaled_tags[0]); index++)
		if (!strcmp(tag, menu_picture_upscaled_tags[index]))
			return 1;
	return 0;
}

#endif
