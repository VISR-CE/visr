/* Apache-2.0. Menu photographs use bpf; logos, difficulty icons and marines keep raw S4g. */
#include "texture_policy.h"
#include "menu_picture_upscaled.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const pictures[] = {
	"ui\\shell\\bitmaps\\mp_map_grafix",
	"ui\\shell\\bitmaps\\sp_levels",
	"ui\\shell\\main_menu\\difficulty_select\\difficulty_options",
	"ui\\shell\\main_menu\\difficulty_select\\difficulty_options_small",
	"ui\\shell\\main_menu\\halo_logo",
	"ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\color_edit\\player_color_marine_large",
	"ui\\shell\\bitmaps\\colors_sm"
};

static struct texture_policy_decision classify(const char *tag, int index, int format, int type, unsigned flags)
{
	struct texture_policy_catalog *catalog = texture_policy_catalog_new();
	struct texture_policy_bitmap bitmap = {0};
	struct texture_policy_decision *decisions, result;

	assert(catalog);
	bitmap.tag = tag;
	bitmap.index = index;
	bitmap.hash = 0x1234;
	bitmap.has_hash = 1;
	bitmap.width = bitmap.height = 64;
	bitmap.format = format;
	bitmap.type = type;
	bitmap.flags = flags;
	assert(!texture_policy_add_bitmap(catalog, &bitmap));
	assert(texture_policy_classify(catalog, &texture_policy_embedded, &decisions, NULL, NULL) == 1);
	result = decisions[0];
	free(decisions);
	texture_policy_catalog_free(catalog);
	return result;
}

int main(void)
{
	const int formats[] = {0, 1, 2, 3, 6, 8, 9, 10, 11, 14, 15, 16, 17};
	struct texture_policy_decision decision;

	assert(!menu_picture_upscaled_tag(NULL));
	assert(!menu_picture_upscaled_tag("ui\\shell\\bitmaps\\mp_map_grafix_extra"));
	for (size_t picture = 0; picture < sizeof(pictures) / sizeof(pictures[0]); picture++)
	{
		assert(menu_picture_upscaled_tag(pictures[picture]));
		for (size_t format = 0; format < sizeof(formats) / sizeof(formats[0]); format++)
			for (int frame = 0; frame < 4; frame += 3)
			{
				decision = classify(pictures[picture], frame, formats[format], 0, 1);
				assert(decision.kind == (picture < 2 ? TEXTURE_POLICY_SURFACE : TEXTURE_POLICY_GRAPHIC));
				assert(decision.result == (picture < 2 ? TEXTURE_POLICY_BPF : TEXTURE_POLICY_S4G));
				assert(decision.treatment == TEXTURE_POLICY_COLOR);
				assert(decision.pinned && decision.scale == 4 && !decision.wrap);
			}
	}
	for (size_t picture = 7; picture < sizeof(menu_picture_upscaled_tags) / sizeof(menu_picture_upscaled_tags[0]); picture++)
		for (int frame = 0; frame < 8; frame++)
		{
			decision = classify(menu_picture_upscaled_tags[picture], frame, 16, 0, 1);
			assert(decision.result == TEXTURE_POLICY_BPF && decision.pinned && decision.scale == 4);
			assert(decision.treatment == TEXTURE_POLICY_COLOR && !decision.wrap);
		}
	assert(menu_picture_upscaled_tag("ui\\shell\\bitmaps\\arrow_big_left"));
	assert(menu_picture_upscaled_tag("ui\\shell\\bitmaps\\arrow_big_right"));
	for (int frame = 0; frame < 2; frame++)
	{
		decision = classify("ui\\shell\\bitmaps\\arrow_big_left", frame, 15, 0, 1);
		assert(decision.result == TEXTURE_POLICY_BPF && decision.scale == 4);
		decision = classify("ui\\shell\\bitmaps\\arrow_big_right", frame, 15, 0, 1);
		assert(decision.result == TEXTURE_POLICY_BPF && decision.scale == 4);
	}
	decision = classify(pictures[0], 0, 13, 0, 1);
	assert(decision.result == TEXTURE_POLICY_ORIGINAL);
	decision = classify(pictures[0], 0, 11, 0, 1 | 16);
	assert(decision.result == TEXTURE_POLICY_ORIGINAL);
	decision = classify(pictures[0], 0, 11, 0, 0);
	assert(decision.result == TEXTURE_POLICY_ORIGINAL);
	decision = classify(pictures[0], 0, 11, 1, 1);
	assert(decision.result == TEXTURE_POLICY_ORIGINAL);
	decision = classify("ui\\shell\\bitmaps\\list_field", 0, 11, 0, 1);
	assert(decision.result == TEXTURE_POLICY_ORIGINAL && !decision.pinned);
	decision = classify("ui\\shell\\bitmaps\\list_field", 0, 16, 0, 1);
	assert(decision.result == TEXTURE_POLICY_S4G && !decision.pinned && decision.scale == 4);
	decision = classify("ui\\shell\\bitmaps\\mp_map_grafix_extra", 0, 11, 0, 1);
	assert(decision.result == TEXTURE_POLICY_ORIGINAL && !decision.pinned);
	decision = classify("ui\\hud\\bitmaps\\hud_sweeper", 0, 16, 0, 1);
	assert(decision.result == TEXTURE_POLICY_ORIGINAL);
	puts("menu picture upscaling probe: passed");
	return 0;
}
