/* SPDX-License-Identifier: Apache-2.0 */
/* The game side of the launch map picture render (port/linux/src/map_pictures_render.c):
which maps, their keys, hashes and saved framings, and whether a map's game runs. */
#ifdef HALO_IOS
#include "cseries.h"
#include "game/game.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "map_pictures_scenario.h"
#include "photo_framing.h"
#include "map_share.h"
#include <stdio.h>
#include <string.h>

static const char *const map_pictures_scenarios[] = {
	"levels\\a10\\a10", "levels\\a30\\a30", "levels\\a50\\a50", "levels\\b30\\b30", "levels\\b40\\b40",
	"levels\\c10\\c10", "levels\\c20\\c20", "levels\\c40\\c40", "levels\\d20\\d20", "levels\\d40\\d40",
	"levels\\test\\beavercreek\\beavercreek", "levels\\test\\bloodgulch\\bloodgulch",
	"levels\\test\\boardingaction\\boardingaction", "levels\\test\\carousel\\carousel",
	"levels\\test\\chillout\\chillout", "levels\\test\\damnation\\damnation",
	"levels\\test\\hangemhigh\\hangemhigh", "levels\\test\\longest\\longest",
	"levels\\test\\prisoner\\prisoner", "levels\\test\\putput\\putput",
	"levels\\test\\ratrace\\ratrace", "levels\\test\\sidewinder\\sidewinder",
	"levels\\test\\wizard\\wizard",
};

int map_pictures_scenario_count(void)
{
	return (int)(sizeof(map_pictures_scenarios) / sizeof(map_pictures_scenarios[0]));
}

const char *map_pictures_scenario_name(int index)
{
	return index >= 0 && index < map_pictures_scenario_count() ? map_pictures_scenarios[index] : NULL;
}

int map_pictures_scenario_identity(int index, char *key, size_t size, char hash_hex[65], uint32_t *revision)
{
	const char *name = map_pictures_scenario_name(index);
	char path[256];
	byte hash[32];
	struct photo_shot shot;
	int saved = 0, i;

	*revision = 0;
	if (!name || !photo_map_key(name, 0, NULL, key, size) || strncmp(key, "stock:", 6)) return 0;
	if (snprintf(path, sizeof(path), "d:\\maps\\%s.map", key + 6) >= (int)sizeof(path) ||
		!map_share_map_hash(path, hash)) return 0;
	for (i = 0; i < 32; i++) sprintf(hash_hex + i * 2, "%02x", hash[i]);
	hash_hex[64] = 0;
	/* the same file photo mode saves to (photo_mode.c) */
	if (snprintf(path, sizeof(path), "d:\\framings\\%s.json", key) < (int)sizeof(path) &&
		photo_fragment_read(path, key, &shot, &saved) && saved > 0)
		*revision = (uint32_t)saved;
	return 1;
}

int map_pictures_scenario_in_game(void)
{
	struct scenario *scenario;

	if (!game_time_initialized() || !game_in_progress()) return 0;
	scenario = global_scenario_try_and_get();
	return scenario && scenario->type != _scenario_type_main_menu;
}
#endif
