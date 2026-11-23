/* The game's own maps: the one table every list of them reads (the server browser, the menus, map sharing, photo
framing, the upscaler's order, benchmark titles). A map is listed by its file name (lowercase, without folder or
extension), its menu name, what it is (STOCK_MAP_*), the content hash of Custom Edition's copy of it (version 609, as
installed: lowercase hex of map_share_content_hash in map_share_files.c, which leaves the cache header out, so a copy
with an edited header still matches; NULL for a map Custom Edition does not ship), and its frame in the menus' level
pictures (ui\shell\bitmaps\mp_map_grafix; -1 for none). Header-only and standalone, so the game, the native host, map
sharing's policy (map_share_policy.c) and the probes and tests compile the same rows. */
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
	const char *custom_edition_content;
	int picture_frame;
};

/* the table, `*count` rows: the menus' map, the campaign in order, then the multiplayer maps */
static inline const struct stock_map *stock_maps(int *count)
{
	static const struct stock_map maps[] =
	{
		{ "ui", "Main menu", STOCK_MAP_INTERFACE, "fbc9f0191e4e04994f4d7e2e2a604d62709a26fc2dd3f0270346b9d1391a9f14", -1 },
		{ "a10", "The Pillar of Autumn", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "a30", "Halo", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "a50", "The Truth and Reconciliation", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "b30", "The Silent Cartographer", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "b40", "Assault on the Control Room", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "c10", "343 Guilty Spark", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "c20", "The Library", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "c40", "Two Betrayals", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "d20", "Keyes", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "d40", "The Maw", STOCK_MAP_CAMPAIGN, NULL, -1 },
		{ "beavercreek", "Battle Creek", STOCK_MAP_XBOX_MULTIPLAYER, "a65b47047a758d5591ac0422ba8489827250914f3e874d33da213564497b9dfc", 0 },
		{ "sidewinder", "Sidewinder", STOCK_MAP_XBOX_MULTIPLAYER, "e45682925a5e9284e4411ef5763fd186e4ca9d6839312dc8d97b1becfce142c8", 1 },
		{ "damnation", "Damnation", STOCK_MAP_XBOX_MULTIPLAYER, "8565e9b2f5eafeb2b0943c58879c7fb01cb5177856a78cecae7f61fc42591422", 2 },
		{ "ratrace", "Rat Race", STOCK_MAP_XBOX_MULTIPLAYER, "1ed5b576593613fbe029dc67bc53ff522470be716bc630df741b876bc4b03f76", 3 },
		{ "prisoner", "Prisoner", STOCK_MAP_XBOX_MULTIPLAYER, "2dbef4690f51a247b6ffda28c4a2dd663380da318cd9c8b5ad57600db0c86288", 4 },
		{ "hangemhigh", "Hang 'Em High", STOCK_MAP_XBOX_MULTIPLAYER, "41ce67fda4a6d4079f1961176d41004f7e128738dd2ee36257debc8a4adbcf4f", 5 },
		{ "chillout", "Chill Out", STOCK_MAP_XBOX_MULTIPLAYER, "6fd4328430b4c46018012da9a723a7034d4d807e0878af581882626b70c9896f", 6 },
		{ "carousel", "Derelict", STOCK_MAP_XBOX_MULTIPLAYER, "c964a77b7931963c24444837786346c342cc1fb7394494a4c4563c09f40b0718", 7 },
		{ "boardingaction", "Boarding Action", STOCK_MAP_XBOX_MULTIPLAYER, "e94cd50ad600d60ff14a153ab3211db2b6ec0e4d0984e3755254279acea09894", 8 },
		{ "bloodgulch", "Blood Gulch", STOCK_MAP_XBOX_MULTIPLAYER, "ce71cd19aa2582263d49d78d505a9ea966a51d27f547dd37094541dbdd233eab", 9 },
		{ "wizard", "Wizard", STOCK_MAP_XBOX_MULTIPLAYER, "923476273cd95271f919501f1d0c5399869b947457ba3ec164e250e6129755f3", 10 },
		{ "putput", "Chiron TL34", STOCK_MAP_XBOX_MULTIPLAYER, "6b91613495347fd6fc82b7a7a0e8266d9f11f2c4799f8a774dec0e637bacbd11", 11 },
		{ "longest", "Longest", STOCK_MAP_XBOX_MULTIPLAYER, "4118e66efe6516f145a59456036c0a758632b0411740b638af12f3f27ea4c208", 12 },
		{ "icefields", "Ice Fields", STOCK_MAP_PC_MULTIPLAYER, "fe134e8df69b287774f2cb9900d23089acecb29716aad5e64501c8058cfd81c6", -1 },
		{ "deathisland", "Death Island", STOCK_MAP_PC_MULTIPLAYER, "8a51bd63db267a955a2fa17c1f2ede550e726f07ce9c88ad8cd359652d155abb", -1 },
		{ "dangercanyon", "Danger Canyon", STOCK_MAP_PC_MULTIPLAYER, "e7a3d638c2f857771a805445d5c35ad19f02f7ec0ec484f72a3147de0e685ac3", -1 },
		{ "infinity", "Infinity", STOCK_MAP_PC_MULTIPLAYER, "9f3c1bf35388a4b6fbab8c17a2e26acaee0e4ef3a664c8799834ee733d326dd8", -1 },
		{ "timberland", "Timberland", STOCK_MAP_PC_MULTIPLAYER, "7b0614d8877c2ff8a7985a9239cd5142ff3bcc3c60c77d1a1e53fd526a1aea03", -1 },
		{ "gephyrophobia", "Gephyrophobia", STOCK_MAP_PC_MULTIPLAYER, "39f06b3c11663169e758a66137ca2a78894e10519956848ab103e9aef9293337", -1 },
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

/* the row of the map Custom Edition's copy of which has this content hash (32 bytes), or NULL */
static inline const struct stock_map *stock_map_with_content(const uint8_t *content)
{
	static const char digits[] = "0123456789abcdef";
	int count;
	const struct stock_map *maps = stock_maps(&count);
	int index;

	for (index = 0; content && index < count; index++)
	{
		const char *hex = maps[index].custom_edition_content;
		int position;

		for (position = 0; hex && position < 32; position++)
		{
			if (hex[position * 2] != digits[content[position] >> 4] || hex[position * 2 + 1] != digits[content[position] & 15])
				break;
		}
		if (hex && position == 32)
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
