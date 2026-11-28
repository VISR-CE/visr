/* The game's own maps: the one table every list of them reads (the server browser, the menus, map sharing, photo
framing, the upscaler's order, benchmark titles). A map is listed by its file name (lowercase, without folder or
extension), its menu name, what it is (STOCK_MAP_*), the checksum in the header of Custom Edition's copy of it (version
609, as installed; 0 for a map Custom Edition does not ship), and its frame in the menus' level pictures
(ui\shell\bitmaps\mp_map_grafix; -1 for none). Header-only and standalone, so the game, the native host, map sharing's
policy (map_share_policy.c) and the probes and tests compile the same rows. */
#pragma once
#include <stddef.h>
#include <stdint.h>

enum
{
	/* the Xbox disc's multiplayer maps (Custom Edition ships them too) */
	STOCK_MAP_XBOX_MULTIPLAYER = 1,
	/* Halo PC's own multiplayer maps (and Custom Edition's), not on the Xbox */
	STOCK_MAP_PC_MULTIPLAYER = 2,
	/* the campaign levels, in the campaign's order */
	STOCK_MAP_CAMPAIGN = 4,
	/* the menus' map (ui.map) */
	STOCK_MAP_INTERFACE = 8,
};

struct stock_map
{
	const char *file;
	const char *display_name;
	unsigned flags;
	uint32_t custom_edition_checksum;
	int picture_frame;
};

/* the table, `*count` rows: the menus' map, the campaign in order, then the multiplayer maps */
static inline const struct stock_map *stock_maps(int *count)
{
	static const struct stock_map maps[] =
	{
		{ "ui", "Main menu", STOCK_MAP_INTERFACE, 0x73EE7229UL, -1 },
		{ "a10", "The Pillar of Autumn", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "a30", "Halo", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "a50", "The Truth and Reconciliation", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "b30", "The Silent Cartographer", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "b40", "Assault on the Control Room", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "c10", "343 Guilty Spark", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "c20", "The Library", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "c40", "Two Betrayals", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "d20", "Keyes", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "d40", "The Maw", STOCK_MAP_CAMPAIGN, 0, -1 },
		{ "beavercreek", "Battle Creek", STOCK_MAP_XBOX_MULTIPLAYER, 0x07B3876AUL, 0 },
		{ "sidewinder", "Sidewinder", STOCK_MAP_XBOX_MULTIPLAYER, 0xBD95CF55UL, 1 },
		{ "damnation", "Damnation", STOCK_MAP_XBOX_MULTIPLAYER, 0x0FBA059DUL, 2 },
		{ "ratrace", "Rat Race", STOCK_MAP_XBOX_MULTIPLAYER, 0xF7F8E14CUL, 3 },
		{ "prisoner", "Prisoner", STOCK_MAP_XBOX_MULTIPLAYER, 0x43B81A8BUL, 4 },
		{ "hangemhigh", "Hang 'Em High", STOCK_MAP_XBOX_MULTIPLAYER, 0xA7C8B9C6UL, 5 },
		{ "chillout", "Chill Out", STOCK_MAP_XBOX_MULTIPLAYER, 0x93C53C27UL, 6 },
		{ "carousel", "Derelict", STOCK_MAP_XBOX_MULTIPLAYER, 0x9C301A08UL, 7 },
		{ "boardingaction", "Boarding Action", STOCK_MAP_XBOX_MULTIPLAYER, 0xF4DEEF94UL, 8 },
		{ "bloodgulch", "Blood Gulch", STOCK_MAP_XBOX_MULTIPLAYER, 0x7B309554UL, 9 },
		{ "wizard", "Wizard", STOCK_MAP_XBOX_MULTIPLAYER, 0xCF3359B1UL, 10 },
		{ "putput", "Chiron TL34", STOCK_MAP_XBOX_MULTIPLAYER, 0xAF2F0B84UL, 11 },
		{ "longest", "Longest", STOCK_MAP_XBOX_MULTIPLAYER, 0xC8F48FF6UL, 12 },
		{ "icefields", "Ice Fields", STOCK_MAP_PC_MULTIPLAYER, 0x5EC1DEB7UL, -1 },
		{ "deathisland", "Death Island", STOCK_MAP_PC_MULTIPLAYER, 0x1DF8C97FUL, -1 },
		{ "dangercanyon", "Danger Canyon", STOCK_MAP_PC_MULTIPLAYER, 0xC410CD74UL, -1 },
		{ "infinity", "Infinity", STOCK_MAP_PC_MULTIPLAYER, 0x0E7F7FE7UL, -1 },
		{ "timberland", "Timberland", STOCK_MAP_PC_MULTIPLAYER, 0x54446470UL, -1 },
		{ "gephyrophobia", "Gephyrophobia", STOCK_MAP_PC_MULTIPLAYER, 0xD2872165UL, -1 },
	};

	*count = (int)(sizeof(maps) / sizeof(maps[0]));
	return maps;
}

/* the row of the map whose file name is `length` characters of `name` (ASCII, in any case), or NULL */
static inline const struct stock_map *stock_map_named(const char *name, size_t length)
{
	int count;
	const struct stock_map *maps = stock_maps(&count);
	int index;

	for (index = 0; name && index < count; index++)
	{
		const char *file = maps[index].file;
		size_t position;

		for (position = 0; position < length && file[position]; position++)
		{
			char character = name[position];

			if (character >= 'A' && character <= 'Z')
				character = (char)(character - 'A' + 'a');
			if (character != file[position])
				break;
		}
		if (position == length && !file[position])
			return &maps[index];
	}
	return NULL;
}

/* the row of the map Custom Edition's copy of which has this header checksum, or NULL */
static inline const struct stock_map *stock_map_with_checksum(uint32_t checksum)
{
	int count;
	const struct stock_map *maps = stock_maps(&count);
	int index;

	for (index = 0; checksum && index < count; index++)
	{
		if (maps[index].custom_edition_checksum == checksum)
			return &maps[index];
	}
	return NULL;
}

/* the row of the map with this frame of the menus' level pictures, or NULL */
static inline const struct stock_map *stock_map_with_picture(int frame)
{
	int count;
	const struct stock_map *maps = stock_maps(&count);
	int index;

	for (index = 0; frame >= 0 && index < count; index++)
	{
		if (maps[index].picture_frame == frame)
			return &maps[index];
	}
	return NULL;
}
