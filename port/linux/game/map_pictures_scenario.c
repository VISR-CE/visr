/* SPDX-License-Identifier: Apache-2.0 */
/* The game side of the launch map picture render (port/linux/src/map_pictures_render.c):
which maps, their keys, hashes and saved framings, and whether a map's game runs. */
#include "map_pictures_scenario.h"
#include "../../runtime/stock_maps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HALO_IOS
#include "cseries.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "main/main.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "photo_framing.h"
#include "map_share.h"
#endif

/* The maps a launch render covers, from the stock table (stock_maps.h): the campaign levels in order, then the Xbox
disc's multiplayer maps in alphabetical order (the order the renders have always come in), each as its scenario's tag
path. */
#define MAP_PICTURES_SCENARIO_LIMIT 32
#define MAP_PICTURES_SCENARIO_PATH 64
static const unsigned map_pictures_flags[] = { STOCK_MAP_CAMPAIGN, STOCK_MAP_XBOX_MULTIPLAYER };
static char map_pictures_scenarios[MAP_PICTURES_SCENARIO_LIMIT][MAP_PICTURES_SCENARIO_PATH];
static int map_pictures_scenarios_listed = -1, map_pictures_campaign_count;

static int map_pictures_scenario_compare(const void *first, const void *second)
{
	return strcmp((const char *)first, (const char *)second);
}

static int map_pictures_scenarios_list(void)
{
	int count, row, kind;
	const struct stock_map *maps = stock_maps(&count);

	if (map_pictures_scenarios_listed >= 0) return map_pictures_scenarios_listed;
	map_pictures_scenarios_listed = 0;
	for (kind = 0; kind < (int)(sizeof(map_pictures_flags) / sizeof(map_pictures_flags[0])); kind++)
	{
		for (row = 0; row < count && map_pictures_scenarios_listed < MAP_PICTURES_SCENARIO_LIMIT; row++)
		{
			if (!(maps[row].flags & map_pictures_flags[kind])) continue;
			snprintf(map_pictures_scenarios[map_pictures_scenarios_listed], MAP_PICTURES_SCENARIO_PATH,
				map_pictures_flags[kind] == STOCK_MAP_CAMPAIGN ? "levels\\%s\\%s" : "levels\\test\\%s\\%s",
				maps[row].file, maps[row].file);
			map_pictures_scenarios_listed++;
		}
		if (map_pictures_flags[kind] == STOCK_MAP_CAMPAIGN) map_pictures_campaign_count = map_pictures_scenarios_listed;
	}
	qsort(map_pictures_scenarios[map_pictures_campaign_count], (size_t)(map_pictures_scenarios_listed - map_pictures_campaign_count),
		MAP_PICTURES_SCENARIO_PATH, map_pictures_scenario_compare);
	return map_pictures_scenarios_listed;
}

int map_pictures_scenario_count(void)
{
	return map_pictures_scenarios_list();
}

const char *map_pictures_scenario_name(int index)
{
	return index >= 0 && index < map_pictures_scenario_count() ? map_pictures_scenarios[index] : NULL;
}

/* The scenario list keeps campaign menu order; import prepares multiplayer first. */
int map_pictures_scenario_import_index(int position)
{
	int count = map_pictures_scenario_count(), multiplayer_count = count - map_pictures_campaign_count;

	if (position < 0 || position >= count) return -1;
	return position < multiplayer_count ? map_pictures_campaign_count + position : position - multiplayer_count;
}

#ifdef HALO_IOS
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
	/* the same sources photo mode loads from (photo_mode.c): the data folder's fragment, then the bundle */
	if (snprintf(path, sizeof(path), "d:\\framings\\%s.json", key) < (int)sizeof(path) &&
		photo_framing_lookup(path, "d:\\map-framings.json", key, &shot, &saved) != PHOTO_FRAMING_NONE && saved > 0)
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

int map_pictures_scenario_in_cinematic(void)
{
	return map_pictures_scenario_in_game() && cinematic_in_progress();
}

long map_pictures_scenario_ticks(void)
{
	return game_time_initialized() ? game_time_get() : 0;
}

static int fast_forwarding;

/* Runs the game as fast as its local tick cap allows (7 ticks a frame) with nothing drawn,
or stops it. Game speed goes through the engine's own tick path, so the scripts, BSP
switches and sounds the cinematic starts run as in play. Stopping leaves the game paused
(speed 0); photo mode's capture sets its own speed. */
void map_pictures_scenario_fast_forward(int on)
{
	fast_forwarding = on;
	if (!game_time_initialized()) return;
	game_time_set_speed(on ? 120.0f : 0.0f);
	debug_no_drawing = on ? TRUE : FALSE;
}

int map_pictures_scenario_fast_forwarding(void)
{
	return fast_forwarding;
}

/* After each tick (main_tick_end): the fast-forward ends on the tick the cinematic does, so
the AI and scripts that start after it have not moved when photo mode freezes the scene. */
boolean map_pictures_scenario_tick_end(void)
{
	if (!fast_forwarding || cinematic_in_progress()) return FALSE;
	map_pictures_scenario_fast_forward(0);
	return TRUE;
}
#endif
