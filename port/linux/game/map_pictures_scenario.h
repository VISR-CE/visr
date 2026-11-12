/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAP_PICTURES_SCENARIO_H
#define MAP_PICTURES_SCENARIO_H
#include <stddef.h>
#include <stdint.h>

/* The stock maps a launch render covers, in menu order: the campaign, then multiplayer. */
int map_pictures_scenario_count(void);
const char *map_pictures_scenario_name(int index);
/* The map's photo key ("stock:a10"), its file hash as lowercase hex and its saved
framing's revision (0 without one). 0 if the key or the hash cannot be had. */
int map_pictures_scenario_identity(int index, char *key, size_t size, char hash_hex[65], uint32_t *revision);
/* 1 while a non-menu map is loaded and its game is running. */
int map_pictures_scenario_in_game(void);
#endif
