/*
MAP_SHARE_SEND.C (test)

What map sharing (port/linux/game/map_share.c, map_share_files.c) sends to
other players: community maps only, never Custom Edition's resource maps or
its stock maps (by their content hashes, port/runtime/stock_maps.h: everything
but the cache header, so an edited header checksum does not hide one),
whatever path sends it. The files are real
files in a temporary folder, judged by the real map_share_policy.c and
cache_file_formats.c (compiled beside this one), and every path that sends a
file is driven: the catalog (map_share_file_hash), the host's offer
(host_request, host_offer), and what a machine passes on of a connection's
file (send_state, piece_sendable, serving_file_size, block_read,
connection_send_hashes, download_piece_had). Files are hashed as the hashing
thread does (hash_job_run, catalog_job_finish), here on the test's own thread.
tools/test_map_share_send.py takes the constants and structures (config.inc)
and the code (under_test.inc), and gives the stock table each Custom Edition
map's content hash as its fake copy here has it (stock_file).
*/

#include "harness.h"

#include <stdarg.h>
#include <stdint.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cache_file_formats.h"
#include "map_share_policy.h"
#include "monocypher.h"
#include "stock_maps.h"

typedef unsigned short word;

#define CUSTOM_EDITION_LEVEL_NAME_PREFIX "custom_maps\\"
#define CUSTOM_EDITION_MAXIMUM_LEVEL_NAME_LENGTH 63

#include "config.inc"

#define csmemset(destination, value, size) memset(destination, value, size)
#define csmemcpy(destination, source, size) memcpy(destination, source, size)
#define csmemcmp(first, second, size) memcmp(first, second, size)
#define csstrcmp(first, second) strcmp(first, second)
#define csstrncmp(first, second, count) strncmp(first, second, count)
#define csstrncpy(destination, source, count) strncpy(destination, source, count)
#define csstrcasecmp(first, second) strcasecmp(first, second)
#define csstrlen(string) strlen(string)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define _error_silent 0
#define error(level, ...) ((void)0)
#define _distributed_message_map_share_offer 0x40
#define __stdcall

static void p2p_blake2b_256(const void *data, unsigned long size, unsigned char *digest)
{
	crypto_blake2b(digest, 32, (const uint8_t *)data, (size_t)size);
}

/* ---------- the fake world */

struct distributed_message_header
{
	word header;
	byte type;
	byte count;
	long game_time;
};

struct network_game
{
	struct
	{
		char name[128];
		long version;
	} map;
};

static char fake_folder[256];
static struct network_game fake_game;
static unsigned long fake_needed;
static int fake_offers;
static byte fake_offer_result;
static byte fake_offer_count;
static byte fake_offer_kinds[8];

static struct map_share_globals map_share_globals;
static struct map_share_files_globals map_share_files_globals;

static unsigned long system_milliseconds(void)
{
	return 1000;
}

static struct network_game *map_share_network_game(void)
{
	return &fake_game;
}

static unsigned long network_game_server_machine_address(long machine_index)
{
	return 0x0A000001UL + (unsigned long)machine_index;
}

static boolean host_in_game(void)
{
	return FALSE;
}

static boolean map_share_sending(void)
{
	return TRUE;
}

static void map_share_fill_header(struct distributed_message_header *header, byte type, word size)
{
	(void)size;
	header->type = type;
}

/* (an offer's result and files, after its header) */
static boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size)
{
	struct distributed_map_share_offer const *offer =
		(void const *)((byte const *)message + sizeof(struct distributed_message_header));
	struct distributed_map_share_offer_file const *files = (void const *)(offer + 1);
	short index;

	(void)machine_index;
	(void)size;
	fake_offer_result = offer->result;
	fake_offer_count = offer->file_count;
	for (index = 0; index < offer->file_count && index < 8; index++)
		fake_offer_kinds[index] = files[index].kind;
	fake_offers++;
	return TRUE;
}

static void download_fail(char const *reason)
{
	(void)reason;
}

static void fake_path(char *path, char const *name)
{
	snprintf(path, MAP_SHARE_PATH_SIZE, "%s/%s", fake_folder, name);
}

#define config_boolean(name) 1
#define custom_edition_level_name(level_name) (!strncmp(level_name, "custom_maps\\", 12))
#define custom_edition_map_file_path(level_name, path) \
	(fake_path(path, level_name + 12), strcat(path, ".map"), 1)
#define custom_edition_resource_map_file_path(kind, path) \
	(fake_path(path, resource_map_type_describe((enum resource_map_type)(kind))), strcat(path, ".map"), 1)
#define custom_edition_map_resource_maps_needed(level_name) fake_needed

/* map_share_files.c's file_stamp, by the C library */
static boolean file_stamp(char const *path, unsigned long *size, unsigned long *modified)
{
	struct stat status;

	if (stat(path, &status))
		return FALSE;
	*size = (unsigned long)status.st_size;
	*modified = (unsigned long)status.st_mtime;
	return TRUE;
}

#include "under_test.inc"

/* ---------- the files */

enum
{
	FAKE_FILE_BYTES = MAP_SHARE_PIECE_BYTES + 0x1000,
	COMMUNITY_CHECKSUM = 0x12345678,
	BLOOD_GULCH_CHECKSUM = 0x7B309554,
};

static void put32(byte *bytes, unsigned long value)
{
	bytes[0] = (byte)value;
	bytes[1] = (byte)(value >> 8);
	bytes[2] = (byte)(value >> 16);
	bytes[3] = (byte)(value >> 24);
}

/* a cache header of `version`, named `name`, with `checksum` (a Custom
Edition cache's tags start after the header and fill a tag index) */
static void cache_header(byte *header, long version, char const *name, unsigned long checksum)
{
	memset(header, 0, CACHE_FILE_HEADER_BYTES);
	memcpy(header, "daeh", 4);
	put32(header + 0x04, (unsigned long)version);
	put32(header + 0x10, CACHE_FILE_HEADER_BYTES);
	put32(header + 0x14, 0x28);
	strncpy((char *)header + 0x20, name, 31);
	strcpy((char *)header + 0x40, "01.00.00.0609");
	put32(header + 0x64, checksum);
	memcpy(header + 0x7FC, "toof", 4);
}

/* a resource map's header (bitmaps 1, sounds 2, loc 3), with no items */
static void resource_header(byte *header, unsigned long type)
{
	memset(header, 0, CACHE_FILE_HEADER_BYTES);
	put32(header, type);
	put32(header + 0x04, 0x10);
	put32(header + 0x08, 0x10);
}

/* a file of `size` bytes: `header`, `marker` right after it (what the
file's content is), then zeros */
static void marked_file_write(char const *name, byte const *header, char const *marker, long size)
{
	char path[MAP_SHARE_PATH_SIZE];
	FILE *stream;
	byte zero = 0;

	fake_path(path, name);
	stream = fopen(path, "wb");
	CHECK(stream != NULL, "%s cannot be written", path);
	fwrite(header, 1, MIN(size, CACHE_FILE_HEADER_BYTES), stream);
	if (marker && size > CACHE_FILE_HEADER_BYTES + (long)strlen(marker))
		fwrite(marker, 1, strlen(marker), stream);
	fseek(stream, size - 1, SEEK_SET);
	fwrite(&zero, 1, 1, stream);
	fclose(stream);
}

static void file_write(char const *name, byte const *header, long size)
{
	marked_file_write(name, header, NULL, size);
}

/* a Custom Edition map's first piece: its header (named `header_name`, with
`checksum`: a copy's may be edited), its tag data right after it (a tag
index of the scenario `scenario`, the structure BSP `bsp` and, if
`extra_group`, one more tag `extra`), then `marker`, what its content is */
enum
{
	FAKE_TAG_CACHE_ADDRESS = 0x40440000,
	FAKE_TAG_DATA_BYTES = 0x200,
	FAKE_TAG_INSTANCES = 0x28,
	FAKE_TAG_NAMES = FAKE_TAG_INSTANCES + 3 * 0x20,
};

static void map_piece(byte *piece, char const *header_name, unsigned long checksum, char const *scenario,
	char const *bsp, unsigned long extra_group, char const *extra, char const *marker)
{
	byte *tags = piece + CACHE_FILE_HEADER_BYTES;
	char const *names[3] = { scenario, bsp, extra };
	unsigned long const groups[3] = { 0x73636E72UL, 0x73627370UL, extra_group };
	long name_offset = FAKE_TAG_NAMES;
	int count = extra_group ? 3 : 2;
	int index;

	memset(piece, 0, MAP_SHARE_PIECE_BYTES);
	cache_header(piece, CACHE_FILE_VERSION_CUSTOM_EDITION, header_name, checksum);
	put32(piece + 0x14, FAKE_TAG_DATA_BYTES);
	put32(tags + 0x00, FAKE_TAG_CACHE_ADDRESS + FAKE_TAG_INSTANCES);
	put32(tags + 0x04, 0xE1740000UL);
	put32(tags + 0x0C, (unsigned long)count);
	put32(tags + 0x24, 0x74616773UL);
	for (index = 0; index < count; index++)
	{
		byte *instance = tags + FAKE_TAG_INSTANCES + index * 0x20;

		put32(instance + 0x00, groups[index]);
		put32(instance + 0x0C, 0xE1740000UL + (unsigned long)index);
		put32(instance + 0x10, FAKE_TAG_CACHE_ADDRESS + (unsigned long)name_offset);
		strcpy((char *)tags + name_offset, names[index]);
		name_offset += (long)strlen(names[index]) + 1;
	}
	CHECK(name_offset < FAKE_TAG_DATA_BYTES, "fake tag names too long");
	snprintf((char *)tags + FAKE_TAG_DATA_BYTES, 64, "%s", marker);
}

/* a copy of the stock map `stock`'s first piece (test_map_share_send.py
gives the table its content hash: `contents`) */
static void stock_piece(byte *piece, char const *stock, char const *header_name, unsigned long checksum)
{
	char path[128];
	char marker[64];

	snprintf(path, sizeof(path), "levels\\test\\%s\\%s", stock, stock);
	snprintf(marker, sizeof(marker), "stock %s", stock);
	map_piece(piece, header_name, checksum, path, path, 0, NULL, marker);
}

/* a community map's: its own scenario and BSP, and, as community maps do,
a shader of a campaign level's */
static void community_piece(byte *piece, char const *header_name)
{
	char path[128];
	char marker[64];

	snprintf(path, sizeof(path), "levels\\test\\%s\\%s", header_name, header_name);
	snprintf(marker, sizeof(marker), "community %s", header_name);
	map_piece(piece, header_name, COMMUNITY_CHECKSUM, path, path, 0x73686472UL,
		"levels\\a10\\shaders\\metal", marker);
}

/* a conversion of a campaign level: by its scenario (levels\a10\a10, in any
case), or by a structure BSP of one with a scenario renamed */
static void conversion_piece(byte *piece, char const *header_name, char const *scenario, char const *bsp)
{
	char marker[64];

	snprintf(marker, sizeof(marker), "conversion %s %s", scenario, bsp);
	map_piece(piece, header_name, COMMUNITY_CHECKSUM, scenario, bsp, 0, NULL, marker);
}

/* a community map whose tags cannot be read: its last tag's name lies
outside the tag data (after its scenario's, which is read and is not a
campaign level's) */
static void unreadable_piece(byte *piece, char const *header_name)
{
	community_piece(piece, header_name);
	put32(piece + CACHE_FILE_HEADER_BYTES + FAKE_TAG_INSTANCES + 2 * 0x20 + 0x10,
		FAKE_TAG_CACHE_ADDRESS + FAKE_TAG_DATA_BYTES);
}

/* a file whose first piece is `piece`, zeros after it */
static void piece_file(char const *name, byte const *piece)
{
	char path[MAP_SHARE_PATH_SIZE];
	FILE *stream;
	byte zero = 0;

	fake_path(path, name);
	stream = fopen(path, "wb");
	CHECK(stream != NULL, "%s cannot be written", path);
	fwrite(piece, 1, MAP_SHARE_PIECE_BYTES, stream);
	fseek(stream, FAKE_FILE_BYTES - 1, SEEK_SET);
	fwrite(&zero, 1, 1, stream);
	fclose(stream);
}

/* a copy of the stock map `stock`, under any name and header */
static void stock_file(char const *name, char const *stock, char const *header_name, unsigned long checksum)
{
	static byte piece[MAP_SHARE_PIECE_BYTES];

	stock_piece(piece, stock, header_name, checksum);
	piece_file(name, piece);
}

static void community_file(char const *name, char const *header_name)
{
	static byte piece[MAP_SHARE_PIECE_BYTES];

	community_piece(piece, header_name);
	piece_file(name, piece);
}

static void conversion_file(char const *name, char const *header_name, char const *scenario, char const *bsp)
{
	static byte piece[MAP_SHARE_PIECE_BYTES];

	conversion_piece(piece, header_name, scenario, bsp);
	piece_file(name, piece);
}

static void unreadable_file(char const *name, char const *header_name)
{
	static byte piece[MAP_SHARE_PIECE_BYTES];

	unreadable_piece(piece, header_name);
	piece_file(name, piece);
}

static void folder_make(void)
{
	strcpy(fake_folder, "/tmp/map-share-send-XXXXXX");
	CHECK(mkdtemp(fake_folder) != NULL, "no temporary folder");
}

/* whether the catalog refuses the file before hashing it (by its kind, name
or header: map_share_file_hash says NONE) */
static boolean catalog_refuses_unhashed(char const *name, short kind)
{
	char path[MAP_SHARE_PATH_SIZE];

	fake_path(path, name);
	return map_share_file_hash(path, kind) == NONE;
}

/* the catalog file at `index` hashed, as the hashing thread does it */
static void catalog_hash(short index)
{
	struct map_share_file *file = map_share_file_get(index);
	struct hash_job job;

	if (!file || file->ready || file->failed)
		return;
	memset(&job, 0, sizeof(job));
	strncpy(job.path, file->path, sizeof(job.path) - 1);
	hash_job_run(&job);
	catalog_job_finish(index, &job);
}

/* whether the catalog sends the file (as the host's offer and every
connection takes it, map_share_file_hash), once it is hashed */
static boolean catalog_takes(char const *name, short kind)
{
	char path[MAP_SHARE_PATH_SIZE];
	short index;

	fake_path(path, name);
	index = map_share_file_hash(path, kind);
	catalog_hash(index);
	return index != NONE && map_share_file_get(index)->ready;
}

/* ---------- the cases */

/* the catalog, which every file this machine sends whole comes from */
static void case_catalog(void)
{
	byte header[CACHE_FILE_HEADER_BYTES];
	struct stock_map const *maps;
	char name[64];
	int count;
	int index;
	int stocked = 0;

	folder_make();
	community_file("hugeass.map", "hugeass");
	CHECK(catalog_takes("hugeass.map", _map_share_file_map), "a community map is not sent");
	community_file("hugeass_1a2b3c4d.map", "hugeass");
	CHECK(catalog_takes("hugeass_1a2b3c4d.map", _map_share_file_map), "a community map's second version is not sent");

	/* Custom Edition's stock maps, by their content, under their names and
	any other, and with an edited header checksum */
	maps = stock_maps(&count);
	for (index = 0; index < count; index++)
	{
		if (!maps[index].custom_edition_content)
			continue;
		snprintf(name, sizeof(name), "%s.map", maps[index].file);
		stock_file(name, maps[index].file, maps[index].file, BLOOD_GULCH_CHECKSUM);
		CHECK(!catalog_takes(name, _map_share_file_map), "%s is sent", name);
		snprintf(name, sizeof(name), "renamed-%s.map", maps[index].file);
		stock_file(name, maps[index].file, "renamed", BLOOD_GULCH_CHECKSUM);
		CHECK(!catalog_takes(name, _map_share_file_map), "%s renamed is sent", maps[index].file);
		snprintf(name, sizeof(name), "edited-%s.map", maps[index].file);
		stock_file(name, maps[index].file, maps[index].file, COMMUNITY_CHECKSUM);
		CHECK(!catalog_takes(name, _map_share_file_map), "%s with an edited header checksum is sent",
			maps[index].file);
		stocked++;
	}
	CHECK(stocked == 20, "%d stock maps with content hashes", stocked);

	/* a community map that reuses a stock map's name, its file's or its
	header's, but differs in content is sent (written a second later than
	the stock copy of that name above, as a file replaced is known) */
	sleep(1);
	community_file("bloodgulch.map", "bloodgulch");
	CHECK(catalog_takes("bloodgulch.map", _map_share_file_map), "a community map named bloodgulch is not sent");
	community_file("A10_1a2b3c4d.map", "a10");
	CHECK(catalog_takes("A10_1a2b3c4d.map", _map_share_file_map), "a community map named a10 is not sent");
	community_file("mymod.map", "bloodgulch");
	CHECK(catalog_takes("mymod.map", _map_share_file_map), "a community map with a stock header name is not sent");

	/* conversions of a campaign level, by their tags under any name: the
	scenario's, or a structure BSP's with the scenario renamed; a
	community map that only borrows a campaign shader is sent (above) */
	conversion_file("pillar.map", "pillar", "levels\\a10\\a10", "levels\\a10\\a10a");
	CHECK(!catalog_takes("pillar.map", _map_share_file_map), "a campaign conversion is sent");
	conversion_file("hugeass2.map", "hugeass2", "LEVELS\\A30\\a30_coop", "levels\\test\\hugeass2\\hugeass2");
	CHECK(!catalog_takes("hugeass2.map", _map_share_file_map), "a campaign conversion's scenario in capitals is sent");
	conversion_file("cartographer.map", "cartographer", "levels\\test\\carto\\carto", "levels\\b30\\b30");
	CHECK(!catalog_takes("cartographer.map", _map_share_file_map),
		"a map on a campaign level's structure BSP, its scenario renamed, is sent");
	conversion_file("a100.map", "a100", "levels\\a100\\a100", "levels\\a100\\a100");
	CHECK(catalog_takes("a100.map", _map_share_file_map), "a community map in a folder named like a campaign level's is not sent");
	/* a map whose tags cannot be read is not known not to be one */
	unreadable_file("unreadable.map", "unreadable");
	CHECK(!catalog_takes("unreadable.map", _map_share_file_map), "a map whose tags cannot be read is sent");

	/* resource maps: as what they are sent as, or by what they are */
	resource_header(header, 1);
	file_write("bitmaps.map", header, FAKE_FILE_BYTES);
	CHECK(catalog_refuses_unhashed("bitmaps.map", _map_share_file_bitmaps), "bitmaps.map is sent");
	file_write("textures.map", header, FAKE_FILE_BYTES);
	CHECK(catalog_refuses_unhashed("textures.map", _map_share_file_map), "a resource map under a map's name is sent");
	CHECK(catalog_refuses_unhashed("hugeass.map", _map_share_file_sounds), "a file sent as sounds.map is sent");
	CHECK(catalog_refuses_unhashed("hugeass.map", _map_share_file_locale), "a file sent as loc.map is sent");
	/* (and a map by a resource map's name, in any case, as a second version) */
	community_file("Sounds.map", "sounds");
	CHECK(catalog_refuses_unhashed("Sounds.map", _map_share_file_map), "a map named sounds.map is sent");
	community_file("loc_1a2b3c4d.map", "loc");
	CHECK(catalog_refuses_unhashed("loc_1a2b3c4d.map", _map_share_file_map), "a map named loc.map is sent");

	/* not a Custom Edition cache: Halo PC's and the Xbox's, or none */
	cache_header(header, 7, "pcmap", COMMUNITY_CHECKSUM);
	file_write("pcmap.map", header, FAKE_FILE_BYTES);
	CHECK(catalog_refuses_unhashed("pcmap.map", _map_share_file_map), "a Halo PC cache is sent");
	cache_header(header, CACHE_FILE_VERSION_XBOX, "xboxmap", COMMUNITY_CHECKSUM);
	file_write("xboxmap.map", header, FAKE_FILE_BYTES);
	CHECK(catalog_refuses_unhashed("xboxmap.map", _map_share_file_map), "an Xbox cache is sent");
	cache_header(header, CACHE_FILE_VERSION_CUSTOM_EDITION, "tiny", COMMUNITY_CHECKSUM);
	file_write("tiny.map", header, 0x400);
	CHECK(catalog_refuses_unhashed("tiny.map", _map_share_file_map), "a file smaller than a header is sent");

	/* a community map hashed, then replaced by a stock one, is sent no more
	(what was sent of it is not sent of the new file) */
	{
		char path[MAP_SHARE_PATH_SIZE];
		short swapped;

		community_file("swapped.map", "swapped");
		fake_path(path, "swapped.map");
		CHECK(catalog_takes("swapped.map", _map_share_file_map), "a community map is not sent");
		swapped = map_share_file_hash(path, _map_share_file_map);
		CHECK(swapped != NONE && map_share_file_get(swapped)->ready, "a community map is not sent");
		sleep(1);
		stock_file("swapped.map", "bloodgulch", "swapped", COMMUNITY_CHECKSUM);
		CHECK(!catalog_takes("swapped.map", _map_share_file_map), "a community map replaced by a stock one is sent");
		CHECK(!map_share_file_get(swapped)->ready, "the catalog still sends a file replaced by a stock map");
		/* (and one replaced by a resource map, refused before it is hashed) */
		community_file("swapped2.map", "swapped2");
		fake_path(path, "swapped2.map");
		CHECK(catalog_takes("swapped2.map", _map_share_file_map), "a community map is not sent");
		swapped = map_share_file_hash(path, _map_share_file_map);
		sleep(1);
		resource_header(header, 2);
		file_write("swapped2.map", header, FAKE_FILE_BYTES);
		CHECK(!catalog_takes("swapped2.map", _map_share_file_map), "a community map replaced by sounds.map is sent");
		CHECK(!map_share_file_get(swapped)->ready, "the catalog still sends a file replaced by a resource map");
	}
}

/* the host's offer of the game's files to a machine that asks */
static void request(byte wanted)
{
	struct distributed_map_share_request asked;

	memset(&asked, 0, sizeof(asked));
	strcpy(asked.level_name, fake_game.map.name);
	asked.version = 1;
	asked.wanted = wanted;
	fake_offers = 0;
	fake_offer_count = 0;
	fake_offer_result = 0xFF;
	host_request(1, &asked);
	/* (host_update offers once the files are hashed, or that it cannot) */
	if (!fake_offers)
	{
		struct host_machine *machine = &map_share_globals.machines[1];
		boolean ready;
		short index;

		for (index = 0; index < machine->file_count; index++)
			catalog_hash(machine->catalog_files[index]);
		if (host_files_failed(machine, &ready))
			host_offer(1, _offer_unavailable);
		else if (ready)
			host_offer(1, _offer_ok);
	}
}

static void case_offer(void)
{
	byte header[CACHE_FILE_HEADER_BYTES];
	short index;

	folder_make();
	community_file("hugeass.map", "hugeass");
	stock_file("bloodgulch.map", "bloodgulch", "bloodgulch", BLOOD_GULCH_CHECKSUM);
	stock_file("renamed.map", "bloodgulch", "renamed", BLOOD_GULCH_CHECKSUM);
	stock_file("edited.map", "bloodgulch", "edited", COMMUNITY_CHECKSUM);
	conversion_file("pillar.map", "pillar", "levels\\a10\\a10", "levels\\a10\\a10a");
	resource_header(header, 1);
	file_write("bitmaps.map", header, FAKE_FILE_BYTES);
	resource_header(header, 2);
	file_write("sounds.map", header, FAKE_FILE_BYTES);
	resource_header(header, 3);
	file_write("loc.map", header, FAKE_FILE_BYTES);
	fake_game.map.version = 1;
	fake_needed = FLAG(_map_share_file_bitmaps) | FLAG(_map_share_file_sounds) | FLAG(_map_share_file_locale);

	/* a community map, to a machine with the resource maps: the map alone */
	strcpy(fake_game.map.name, "custom_maps\\hugeass");
	request(FLAG(_map_share_file_map));
	CHECK(fake_offer_result == _offer_ok && fake_offer_count == 1 && fake_offer_kinds[0] == _map_share_file_map,
		"a community map offered as %d (%d files)", fake_offer_result, fake_offer_count);

	/* to a machine without them: nothing (it could not play the map) */
	for (index = _map_share_file_bitmaps; index <= _map_share_file_locale; index++)
	{
		request(FLAG(_map_share_file_map) | FLAG(index));
		CHECK(fake_offer_result == _offer_unavailable && fake_offer_count == 0,
			"resource map %d offered (%d files)", index, fake_offer_count);
		request(FLAG(index));
		CHECK(fake_offer_result == _offer_unavailable && fake_offer_count == 0,
			"resource map %d offered alone (%d files)", index, fake_offer_count);
	}

	/* a stock map, by its name or renamed */
	strcpy(fake_game.map.name, "custom_maps\\bloodgulch");
	request(FLAG(_map_share_file_map));
	CHECK(fake_offer_result == _offer_unavailable && fake_offer_count == 0, "a stock map offered");
	strcpy(fake_game.map.name, "custom_maps\\renamed");
	request(FLAG(_map_share_file_map));
	CHECK(fake_offer_result == _offer_unavailable && fake_offer_count == 0, "a stock map renamed offered");
	strcpy(fake_game.map.name, "custom_maps\\edited");
	request(FLAG(_map_share_file_map));
	CHECK(fake_offer_result == _offer_unavailable && fake_offer_count == 0,
		"a stock map with an edited header checksum offered");
	strcpy(fake_game.map.name, "custom_maps\\pillar");
	request(FLAG(_map_share_file_map));
	CHECK(fake_offer_result == _offer_unavailable && fake_offer_count == 0, "a campaign conversion offered");
}

/* what a downloading machine passes on of the download's files */
static void download_file_make(short index, short kind, char const *name, byte const *header)
{
	struct download *download = &map_share_globals.download;
	struct download_file *file = &download->files[index];

	memset(file, 0, sizeof(*file));
	file->kind = kind;
	file->state = _file_transferring;
	file->size = FAKE_FILE_BYTES;
	file->piece_count = map_share_piece_count(file->size);
	file->piece_hashes = calloc((unsigned long)file->piece_count, MAP_SHARE_HASH_BYTES);
	/* (the later pieces' hashes, known before any piece is had: the fake
	files' second piece is zeros) */
	{
		static byte zeros[FAKE_FILE_BYTES - MAP_SHARE_PIECE_BYTES];

		p2p_blake2b_256(zeros, sizeof(zeros), file->piece_hashes + MAP_SHARE_HASH_BYTES);
	}
	file->have = calloc(1, 1);
	file->asked = calloc((unsigned long)file->piece_count, 1);
	fake_path(file->final_path, name);
	snprintf(file->part_path, sizeof(file->part_path), "%s.part", file->final_path);
	file->stream = fopen(file->part_path, "w+b");
	CHECK(file->stream != NULL, "%s cannot be written", file->part_path);
	if (header)
		download_piece_had(index, 0, header);
}

/* a machine downloading `count` files, and one connection to it that has
said hello, about none of them yet */
static struct connection *relay_make(short count)
{
	struct connection *connection = &map_share_globals.connections[0];
	short index;

	folder_make();
	memset(&map_share_globals, 0, sizeof(map_share_globals));
	for (index = 0; index < MAXIMUM_FILES; index++)
		map_share_globals.current_files[index] = NONE;
	connection->used = TRUE;
	connection->state = _connection_open;
	connection->hello_heard = TRUE;
	connection->output = calloc(OUTPUT_BYTES, 1);
	connection->file_count = count;
	for (index = 0; index < count; index++)
	{
		connection->download_file[index] = NONE;
		connection->catalog_file[index] = NONE;
	}
	map_share_globals.download.state = _download_transferring;
	map_share_globals.download.file_count = count;
	return connection;
}

/* the frames of `type` a connection's output holds of its file `file` */
static int frames(struct connection const *connection, byte type, short file)
{
	long offset = 0;
	int count = 0;

	while (offset + 5 <= connection->output_size)
	{
		byte const *frame = connection->output + offset;
		long length = (long)(frame[0] | frame[1] << 8 | frame[2] << 16 | (unsigned long)frame[3] << 24);

		if (frame[4] == type && length > 1 && frame[5] == file)
			count++;
		offset += 4 + length;
	}
	return count;
}

/* what the connection's state frame of file `file` says it has (NONE: no
frame), and whether any piece is in it */
static int state_said(struct connection const *connection, short file, boolean *pieces)
{
	long offset = 0;

	*pieces = FALSE;
	while (offset + 5 <= connection->output_size)
	{
		byte const *frame = connection->output + offset;
		long length = (long)(frame[0] | frame[1] << 8 | frame[2] << 16 | (unsigned long)frame[3] << 24);

		if (frame[4] == _frame_state && length > 2 && frame[5] == file)
		{
			long index;

			for (index = 7; index < 4 + length; index++)
				*pieces = *pieces || frame[index] != 0;
			return frame[6];
		}
		offset += 4 + length;
	}
	return NONE;
}

static void case_relay(void)
{
	static byte piece[MAP_SHARE_PIECE_BYTES];
	struct connection *connection = relay_make(4);
	byte block[MAP_SHARE_BLOCK_BYTES];
	unsigned long bytes;
	struct serving serving;
	short index;

	/* a community map; bitmaps.map; a stock map renamed, its header
	checksum edited; a community map sent as sounds.map */
	community_piece(piece, "hugeass");
	download_file_make(0, _map_share_file_map, "hugeass.map", piece);
	resource_header(piece, 1);
	download_file_make(1, _map_share_file_bitmaps, "bitmaps.map", piece);
	stock_piece(piece, "bloodgulch", "mymap", COMMUNITY_CHECKSUM);
	download_file_make(2, _map_share_file_map, "mymap.map", piece);
	community_piece(piece, "hugeass");
	download_file_make(3, _map_share_file_sounds, "sounds.map", piece);
	for (index = 0; index < 4; index++)
		connection->download_file[index] = index;

	memset(piece, 0, sizeof(piece));
	for (index = 0; index < 4; index++)
		download_piece_had(index, 1, piece);
	for (index = 0; index < 4; index++)
	{
		boolean sent = index == 0;

		CHECK(frames(connection, _frame_have, index) == (sent ? 1 : 0), "file %d: %d haves said", index,
			frames(connection, _frame_have, index));
		send_state(connection, index);
		{
			boolean pieces;
			int said = state_said(connection, index, &pieces);

			/* (one that is not passed on is still being downloaded, with
			nothing to give: never _have_none, which closes the connection
			the download needs) */
			CHECK(said == _have_pieces && pieces == sent, "file %d: said %d (pieces %d)", index, said, pieces);
		}
		CHECK(piece_sendable(connection, index, 0) == sent, "file %d: piece 0 sendable", index);
		CHECK(piece_sendable(connection, index, 1) == sent, "file %d: piece 1 sendable", index);
		CHECK((serving_file_size(connection, index) != 0) == sent, "file %d: a size to send", index);
		serving.file = index;
		serving.piece = 0;
		serving.offset = 0;
		CHECK(block_read(connection, &serving, block, &bytes) == sent, "file %d: a block read", index);
		connection->hashes_next[index] = 0;
	}
	connection_send_hashes(connection);
	for (index = 0; index < 4; index++)
	{
		CHECK(frames(connection, _frame_hashes, index) == (index == 0 ? 1 : 0), "file %d: its pieces' hashes sent",
			index);
	}
}

/* a download's campaign conversion, renamed, and a download's map whose
tags cannot be read are not passed on, with all their pieces had */
static void case_relay_refused(void)
{
	static byte piece[MAP_SHARE_PIECE_BYTES];
	struct connection *connection = relay_make(1);

	conversion_piece(piece, "pillar", "levels\\a10\\a10", "levels\\a10\\a10a");
	download_file_make(0, _map_share_file_map, "pillar.map", piece);
	connection->download_file[0] = 0;
	memset(piece, 0, sizeof(piece));
	download_piece_had(0, 1, piece);
	CHECK(!piece_sendable(connection, 0, 0), "a campaign conversion is passed on");

	connection = relay_make(1);
	unreadable_piece(piece, "unreadable");
	download_file_make(0, _map_share_file_map, "unreadable.map", piece);
	connection->download_file[0] = 0;
	memset(piece, 0, sizeof(piece));
	download_piece_had(0, 1, piece);
	CHECK(!piece_sendable(connection, 0, 0), "a map whose tags cannot be read is passed on");

	/* (and a community map so made is: the refusals above are theirs) */
	connection = relay_make(1);
	community_piece(piece, "hugeass");
	download_file_make(0, _map_share_file_map, "hugeass.map", piece);
	connection->download_file[0] = 0;
	memset(piece, 0, sizeof(piece));
	download_piece_had(0, 1, piece);
	CHECK(piece_sendable(connection, 0, 0), "a community map is not passed on");
}

/* a download's file whose first piece is not had yet is not passed on */
static void case_relay_unknown(void)
{
	static byte piece[MAP_SHARE_PIECE_BYTES];
	struct connection *connection = relay_make(1);

	download_file_make(0, _map_share_file_map, "later.map", NULL);
	connection->download_file[0] = 0;
	memset(piece, 0, sizeof(piece));
	download_piece_had(0, 1, piece);
	CHECK(connection->output_size == 0, "a have said of a file not judged yet");
	CHECK(!piece_sendable(connection, 0, 1), "a piece of a file not judged yet is sendable");
	send_state(connection, 0);
	CHECK(connection->output_size == 0, "the state of a file not judged yet is said");

	/* a community map whose tag data lies in its second piece: not passed
	on with its first piece alone, then passed on once its tags are had */
	connection = relay_make(1);
	community_piece(piece, "later");
	put32(piece + 0x10, MAP_SHARE_PIECE_BYTES);
	download_file_make(0, _map_share_file_map, "later.map", piece);
	connection->download_file[0] = 0;
	CHECK(!piece_sendable(connection, 0, 0), "a file whose tags are not had yet is passed on");
	{
		static byte tags[MAP_SHARE_PIECE_BYTES];

		memcpy(tags, piece + CACHE_FILE_HEADER_BYTES, FAKE_TAG_DATA_BYTES);
		download_piece_had(0, 1, tags);
	}
	CHECK(piece_sendable(connection, 0, 0), "a community map whose tags are had is not passed on");
}

/* a catalog file replaced on disk after it was judged is not sent until it
is judged again */
static void case_catalog_replaced(void)
{
	struct connection *connection = relay_make(1);
	char path[MAP_SHARE_PATH_SIZE];
	byte block[MAP_SHARE_BLOCK_BYTES];
	unsigned long bytes;
	struct serving serving;
	short index;

	community_file("hugeass.map", "hugeass");
	fake_path(path, "hugeass.map");
	index = map_share_file_hash(path, _map_share_file_map);
	CHECK(index != NONE, "a community map is not sent");
	map_share_file_get(index)->piece_count = map_share_piece_count(FAKE_FILE_BYTES);
	connection->catalog_file[0] = index;
	CHECK(!piece_sendable(connection, 0, 0), "a piece of a file not hashed yet is sendable");
	map_share_file_get(index)->ready = TRUE;
	serving.file = 0;
	serving.piece = 0;
	serving.offset = 0;
	CHECK(piece_sendable(connection, 0, 0), "a community map's piece is not sendable");
	CHECK(block_read(connection, &serving, block, &bytes), "a community map's block is not read");
	sleep(1);
	stock_file("hugeass.map", "bloodgulch", "hugeass", BLOOD_GULCH_CHECKSUM);
	CHECK(!piece_sendable(connection, 0, 0), "a replaced file's piece is sendable");
	CHECK(!serving_file_size(connection, 0), "a replaced file has a size to send");
	CHECK(!block_read(connection, &serving, block, &bytes), "a replaced file's block is read");
}

/* a stock map is hashed once: its content, kept for its size and time
written, refuses it again without hashing it, after the catalog has let it
go */
static void case_catalog_once(void)
{
	char path[MAP_SHARE_PATH_SIZE];

	folder_make();
	stock_file("bloodgulch.map", "bloodgulch", "bloodgulch", COMMUNITY_CHECKSUM);
	CHECK(!catalog_takes("bloodgulch.map", _map_share_file_map), "a stock map is sent");
	memset(map_share_files_globals.files, 0, sizeof(map_share_files_globals.files));
	fake_path(path, "bloodgulch.map");
	CHECK(map_share_file_hash(path, _map_share_file_map) == NONE, "a stock map refused before is hashed again");
	/* (a community map is hashed once too, and sent) */
	community_file("hugeass.map", "hugeass");
	CHECK(catalog_takes("hugeass.map", _map_share_file_map), "a community map is not sent");
}

/* a file that changed while it was hashed is hashed again, not sent as what
was read */
static void case_catalog_changing(void)
{
	char path[MAP_SHARE_PATH_SIZE];
	struct hash_job job;
	short index;

	folder_make();
	community_file("hugeass.map", "hugeass");
	fake_path(path, "hugeass.map");
	index = map_share_file_hash(path, _map_share_file_map);
	CHECK(index != NONE, "a community map is not taken");
	memset(&job, 0, sizeof(job));
	strncpy(job.path, path, sizeof(job.path) - 1);
	hash_job_run(&job);
	CHECK(job.modified != 0, "a file that did not change is said to have");
	job.modified = 0;
	catalog_job_finish(index, &job);
	CHECK(!map_share_file_get(index)->ready && !map_share_file_get(index)->failed,
		"a file that changed while it was hashed is sent, or never hashed again");
	catalog_hash(index);
	CHECK(map_share_file_get(index)->ready, "a community map hashed again is not sent");
}

/* a finished download, seeded with the pieces' hashes it was checked
against (map_share_file_hashed), is sent only if its content may be */
static boolean downloaded_takes(char const *name)
{
	char path[MAP_SHARE_PATH_SIZE];
	struct hash_job job;
	struct map_share_file *file;

	fake_path(path, name);
	memset(&job, 0, sizeof(job));
	strncpy(job.path, path, sizeof(job.path) - 1);
	hash_job_run(&job);
	CHECK(!job.failed, "%s cannot be hashed", name);
	file = map_share_file_get(map_share_file_hashed(path, _map_share_file_map, job.size, job.piece_hashes,
		job.piece_count, job.hash));
	free(job.piece_hashes);
	return file && file->ready;
}

static void case_downloaded(void)
{
	folder_make();
	community_file("hugeass.map", "hugeass");
	CHECK(downloaded_takes("hugeass.map"), "a downloaded community map is not passed on");
	stock_file("mymap.map", "bloodgulch", "mymap", COMMUNITY_CHECKSUM);
	CHECK(!downloaded_takes("mymap.map"), "a downloaded stock map with an edited header checksum is passed on");
	conversion_file("pillar.map", "pillar", "levels\\a10\\a10", "levels\\a10\\a10a");
	CHECK(!downloaded_takes("pillar.map"), "a downloaded campaign conversion is passed on");
}

/* each stock map's fake copy's content hash, for test_map_share_send.py
to give the stock table */
static void contents(void)
{
	struct stock_map const *maps;
	char name[64];
	int count;
	int index;

	folder_make();
	maps = stock_maps(&count);
	for (index = 0; index < count; index++)
	{
		struct hash_job job;
		int position;

		if (!maps[index].custom_edition_content)
			continue;
		snprintf(name, sizeof(name), "%s.map", maps[index].file);
		stock_file(name, maps[index].file, maps[index].file, BLOOD_GULCH_CHECKSUM);
		memset(&job, 0, sizeof(job));
		fake_path(job.path, name);
		hash_job_run(&job);
		CHECK(!job.failed, "%s cannot be hashed", name);
		printf("%s ", maps[index].file);
		for (position = 0; position < MAP_SHARE_POLICY_CONTENT_BYTES; position++)
			printf("%02x", job.content[position]);
		printf("\n");
		free(job.piece_hashes);
	}
}

int main(int count, char **arguments)
{
	char const *name = count > 1 ? arguments[1] : "";

	if (!strcmp(name, "contents"))
		contents();
	else if (!strcmp(name, "catalog"))
		case_catalog();
	else if (!strcmp(name, "offer"))
		case_offer();
	else if (!strcmp(name, "relay"))
		case_relay();
	else if (!strcmp(name, "catalog-once"))
		case_catalog_once();
	else if (!strcmp(name, "catalog-changing"))
		case_catalog_changing();
	else if (!strcmp(name, "downloaded"))
		case_downloaded();
	else if (!strcmp(name, "catalog-replaced"))
		case_catalog_replaced();
	else if (!strcmp(name, "relay-unknown"))
		case_relay_unknown();
	else if (!strcmp(name, "relay-refused"))
		case_relay_refused();
	else
		CHECK(0, "no case %s", name);
	return 0;
}
