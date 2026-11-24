/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAP_PICTURES_SCENARIO_H
#define MAP_PICTURES_SCENARIO_H
#include <stddef.h>
#include <stdint.h>

/* The stock maps a launch render covers, in menu order: the campaign, then multiplayer. */
int map_pictures_scenario_count(void);
const char *map_pictures_scenario_name(int index);
/* The scenario table index at an import position: multiplayer, then campaign in order; -1 outside the list. */
int map_pictures_scenario_import_index(int position);
/* The map's photo key ("stock:a10"), its file hash as lowercase hex and its saved
framing's revision and content fingerprint (photo_shot_hash; both 0 without a framing). 0 if the key or the
hash cannot be had. */
int map_pictures_scenario_identity(int index, char *key, size_t size, char hash_hex[65], uint32_t *revision,
	uint32_t *framing_hash);
/* 1 while a network game or lobby exists. */
int map_pictures_scenario_network_active(void);
/* 1 while a non-menu map is loaded and its game is running. */
int map_pictures_scenario_in_game(void);
/* 1 while a cinematic (a campaign map's opening cutscene) is playing. */
int map_pictures_scenario_in_cinematic(void);
/* The game's tick count (0 before a game runs). */
long map_pictures_scenario_ticks(void);
/* 1: run the game at high speed with drawing off; 0: stop it (paused, drawing on). The
fast-forward also stops by itself on the tick the cinematic ends. */
void map_pictures_scenario_fast_forward(int on);
int map_pictures_scenario_fast_forwarding(void);
#endif
