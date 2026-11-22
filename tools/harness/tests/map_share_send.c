/*
MAP_SHARE_SEND.C (test)

What map sharing (port/linux/game/map_share.c, map_share_files.c) sends to
other players: community maps only, never Custom Edition's resource maps or
one of the publisher's own maps, whatever path sends it. The files are real
files in a temporary folder, judged by the real map_share_policy.c and
cache_file_formats.c (compiled beside this one), and every path that sends a
file is driven: the catalog (map_share_file_hash), the host's offer
(host_request, host_offer), and what a machine passes on of a connection's
file (send_state, piece_sendable, serving_file_size, block_read,
connection_send_hashes, download_piece_had). tools/test_map_share_send.py
takes the constants and structures (config.inc) and the code
(under_test.inc).
*/

#include "harness.h"

#include <stdarg.h>
#include <stdint.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cache_file_formats.h"
#include "map_share_policy.h"

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

static void file_write(char const *name, byte const *header, long size)
{
	char path[MAP_SHARE_PATH_SIZE];
	FILE *stream;
	byte zero = 0;

	fake_path(path, name);
	stream = fopen(path, "wb");
	CHECK(stream != NULL, "%s cannot be written", path);
	fwrite(header, 1, MIN(size, CACHE_FILE_HEADER_BYTES), stream);
	fseek(stream, size - 1, SEEK_SET);
	fwrite(&zero, 1, 1, stream);
	fclose(stream);
}

static void community_file(char const *name, char const *header_name)
{
	byte header[CACHE_FILE_HEADER_BYTES];

	cache_header(header, CACHE_FILE_VERSION_CUSTOM_EDITION, header_name, COMMUNITY_CHECKSUM);
	file_write(name, header, FAKE_FILE_BYTES);
}

static void folder_make(void)
{
	strcpy(fake_folder, "/tmp/map-share-send-XXXXXX");
	CHECK(mkdtemp(fake_folder) != NULL, "no temporary folder");
}

/* whether the catalog takes the file (as the host's offer and every
connection does, map_share_file_hash) */
static boolean catalog_takes(char const *name, short kind)
{
	char path[MAP_SHARE_PATH_SIZE];

	fake_path(path, name);
	return map_share_file_hash(path, kind) != NONE;
}

/* ---------- the cases */

/* the catalog, which every file this machine sends whole comes from */
static void case_catalog(void)
{
	byte header[CACHE_FILE_HEADER_BYTES];
	char const *const *names;
	char name[64];
	int32_t count;
	int32_t index;

	folder_make();
	community_file("hugeass.map", "hugeass");
	CHECK(catalog_takes("hugeass.map", _map_share_file_map), "a community map is not sent");
	community_file("hugeass_1a2b3c4d.map", "hugeass");
	CHECK(catalog_takes("hugeass_1a2b3c4d.map", _map_share_file_map), "a community map's second version is not sent");

	/* the publisher's maps, by every name, in any case and as a second
	version (a community map's header: the name alone says) */
	names = map_share_policy_publisher_names(&count);
	CHECK(count >= 23, "only %d of the publisher's names", (int)count);
	for (index = 0; index < count; index++)
	{
		snprintf(name, sizeof(name), "%s.map", names[index]);
		community_file(name, "hugeass");
		CHECK(!catalog_takes(name, _map_share_file_map), "%s is sent", name);
		snprintf(name, sizeof(name), "%s_1a2b3c4d.map", names[index]);
		community_file(name, "hugeass");
		CHECK(!catalog_takes(name, _map_share_file_map), "%s is sent", name);
	}
	community_file("BloodGulch.map", "hugeass");
	CHECK(!catalog_takes("BloodGulch.map", _map_share_file_map), "BloodGulch.map is sent");

	/* a copy of one under another name: its header's name, or its checksum */
	community_file("renamed.map", "bloodgulch");
	CHECK(!catalog_takes("renamed.map", _map_share_file_map), "a stock map renamed is sent");
	cache_header(header, CACHE_FILE_VERSION_CUSTOM_EDITION, "renamed2", BLOOD_GULCH_CHECKSUM);
	file_write("renamed2.map", header, FAKE_FILE_BYTES);
	CHECK(!catalog_takes("renamed2.map", _map_share_file_map), "a stock map renamed, header and all, is sent");

	/* resource maps: as what they are sent as, or by what they are */
	resource_header(header, 1);
	file_write("bitmaps.map", header, FAKE_FILE_BYTES);
	CHECK(!catalog_takes("bitmaps.map", _map_share_file_bitmaps), "bitmaps.map is sent");
	file_write("textures.map", header, FAKE_FILE_BYTES);
	CHECK(!catalog_takes("textures.map", _map_share_file_map), "a resource map under a map's name is sent");
	CHECK(!catalog_takes("hugeass.map", _map_share_file_sounds), "a file sent as sounds.map is sent");
	CHECK(!catalog_takes("hugeass.map", _map_share_file_locale), "a file sent as loc.map is sent");

	/* not a Custom Edition cache: Halo PC's and the Xbox's, or none */
	cache_header(header, 7, "pcmap", COMMUNITY_CHECKSUM);
	file_write("pcmap.map", header, FAKE_FILE_BYTES);
	CHECK(!catalog_takes("pcmap.map", _map_share_file_map), "a Halo PC cache is sent");
	cache_header(header, CACHE_FILE_VERSION_XBOX, "xboxmap", COMMUNITY_CHECKSUM);
	file_write("xboxmap.map", header, FAKE_FILE_BYTES);
	CHECK(!catalog_takes("xboxmap.map", _map_share_file_map), "an Xbox cache is sent");
	cache_header(header, CACHE_FILE_VERSION_CUSTOM_EDITION, "tiny", COMMUNITY_CHECKSUM);
	file_write("tiny.map", header, 0x400);
	CHECK(!catalog_takes("tiny.map", _map_share_file_map), "a file smaller than a header is sent");

	/* a community map hashed, then replaced by a stock one, is sent no more
	(what was sent of it is not sent of the new file) */
	{
		char path[MAP_SHARE_PATH_SIZE];
		short swapped;

		community_file("swapped.map", "swapped");
		fake_path(path, "swapped.map");
		swapped = map_share_file_hash(path, _map_share_file_map);
		CHECK(swapped != NONE, "a community map is not sent");
		map_share_file_get(swapped)->ready = TRUE;
		sleep(1);
		community_file("swapped.map", "bloodgulch");
		CHECK(!catalog_takes("swapped.map", _map_share_file_map), "a community map replaced by a stock one is sent");
		CHECK(!map_share_file_get(swapped)->ready, "the catalog still sends a file replaced by a stock map");
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
	host_request(1, &asked);
	/* (host_update offers once the files are hashed) */
	if (!fake_offers)
		host_offer(1, _offer_ok);
}

static void case_offer(void)
{
	byte header[CACHE_FILE_HEADER_BYTES];
	short index;

	folder_make();
	community_file("hugeass.map", "hugeass");
	community_file("bloodgulch.map", "bloodgulch");
	community_file("renamed.map", "bloodgulch");
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

	/* the publisher's map, by its name or renamed */
	strcpy(fake_game.map.name, "custom_maps\\bloodgulch");
	request(FLAG(_map_share_file_map));
	CHECK(fake_offer_result == _offer_unavailable && fake_offer_count == 0, "a stock map offered");
	strcpy(fake_game.map.name, "custom_maps\\renamed");
	request(FLAG(_map_share_file_map));
	CHECK(fake_offer_result == _offer_unavailable && fake_offer_count == 0, "a stock map renamed offered");
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

static void case_relay(void)
{
	static byte piece[MAP_SHARE_PIECE_BYTES];
	struct connection *connection = relay_make(4);
	byte block[MAP_SHARE_BLOCK_BYTES];
	unsigned long bytes;
	struct serving serving;
	short index;

	/* a community map; bitmaps.map; a stock map renamed; a community map's
	header sent as sounds.map */
	cache_header(piece, CACHE_FILE_VERSION_CUSTOM_EDITION, "hugeass", COMMUNITY_CHECKSUM);
	download_file_make(0, _map_share_file_map, "hugeass.map", piece);
	resource_header(piece, 1);
	download_file_make(1, _map_share_file_bitmaps, "bitmaps.map", piece);
	cache_header(piece, CACHE_FILE_VERSION_CUSTOM_EDITION, "bloodgulch", BLOOD_GULCH_CHECKSUM);
	download_file_make(2, _map_share_file_map, "mymap.map", piece);
	cache_header(piece, CACHE_FILE_VERSION_CUSTOM_EDITION, "hugeass", COMMUNITY_CHECKSUM);
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
		CHECK(frames(connection, _frame_state, index) == 1, "file %d: its state not said", index);
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
}

int main(int count, char **arguments)
{
	char const *name = count > 1 ? arguments[1] : "";

	if (!strcmp(name, "catalog"))
		case_catalog();
	else if (!strcmp(name, "offer"))
		case_offer();
	else if (!strcmp(name, "relay"))
		case_relay();
	else if (!strcmp(name, "relay-unknown"))
		case_relay_unknown();
	else
		CHECK(0, "no case %s", name);
	return 0;
}
