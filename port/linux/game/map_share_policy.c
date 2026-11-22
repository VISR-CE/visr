/*
MAP_SHARE_POLICY.C

Which files map sharing may send (map_share_policy.h).
*/

/* ---------- headers */

#include "map_share_policy.h"
#include "cache_file_formats.h"

#include <string.h>

/* ---------- constants */

/* The publisher's maps, by file name: Custom Edition's (its 19 multiplayer
maps and ui.map), Halo PC's campaign levels, and the resource maps' names.
The Xbox disc's multiplayer maps are among Custom Edition's. */
static char const *const publisher_names[] =
{
	"beavercreek", "bloodgulch", "boardingaction", "carousel", "chillout", "damnation", "dangercanyon",
	"deathisland", "gephyrophobia", "hangemhigh", "icefields", "infinity", "longest", "prisoner", "putput",
	"ratrace", "sidewinder", "timberland", "wizard", "ui",
	"a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40",
	"bitmaps", "sounds", "loc",
};

/* the header checksums of Custom Edition's own maps (version 609, as
installed), which a copy keeps under any name */
static uint32_t const publisher_checksums[] =
{
	0x07B3876AUL, /* beavercreek */
	0x7B309554UL, /* bloodgulch */
	0xF4DEEF94UL, /* boardingaction */
	0x9C301A08UL, /* carousel */
	0x93C53C27UL, /* chillout */
	0x0FBA059DUL, /* damnation */
	0xC410CD74UL, /* dangercanyon */
	0x1DF8C97FUL, /* deathisland */
	0xD2872165UL, /* gephyrophobia */
	0xA7C8B9C6UL, /* hangemhigh */
	0x5EC1DEB7UL, /* icefields */
	0x0E7F7FE7UL, /* infinity */
	0xC8F48FF6UL, /* longest */
	0x43B81A8BUL, /* prisoner */
	0xAF2F0B84UL, /* putput */
	0xF7F8E14CUL, /* ratrace */
	0xBD95CF55UL, /* sidewinder */
	0x54446470UL, /* timberland */
	0x73EE7229UL, /* ui */
	0xCF3359B1UL, /* wizard */
};

static char const *const verdict_descriptions[NUMBER_OF_MAP_SHARE_POLICY_VERDICTS] =
{
	"a community map",
	"a resource map (Custom Edition's own)",
	"one of the publisher's maps",
	"not a Custom Edition map",
};

/* ---------- header reading */

struct header_bytes
{
	uint8_t const *bytes;
	uint32_t count;
};

static int header_read(
	void *context,
	uint32_t offset,
	uint32_t size,
	void *buffer)
{
	struct header_bytes const *header = context;

	if (offset > header->count || size > header->count - offset)
		return 0;
	memcpy(buffer, header->bytes + offset, size);
	return 1;
}

/* ---------- names */

static int ascii_lower(
	int character)
{
	return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
}

/* whether `length` characters of `name` are one of the publisher's map
names, in any case */
static int publisher_name(
	char const *name,
	uint32_t length)
{
	uint32_t index;

	for (index = 0; index < sizeof(publisher_names) / sizeof(publisher_names[0]); index++)
	{
		char const *stock = publisher_names[index];
		uint32_t position;

		if (strlen(stock) != length)
			continue;
		for (position = 0; position < length && ascii_lower((unsigned char)name[position]) == stock[position];
			position++)
			;
		if (position == length)
			return 1;
	}
	return 0;
}

static int publisher_checksum(
	uint32_t checksum)
{
	uint32_t index;

	for (index = 0; index < sizeof(publisher_checksums) / sizeof(publisher_checksums[0]); index++)
	{
		if (publisher_checksums[index] == checksum)
			return 1;
	}
	return 0;
}

/* ---------- public code */

enum map_share_policy_verdict map_share_policy_judge(
	int32_t kind,
	char const *name,
	uint32_t name_length,
	void const *header,
	uint32_t header_bytes,
	uint32_t file_size)
{
	struct header_bytes bytes;
	struct cache_file_source source;
	struct cache_file_identity identity;
	enum cache_file_status status;

	if (kind != _resource_map_none)
		return _map_share_policy_resource_map;
	if (!name || publisher_name(name, name_length))
		return _map_share_policy_publisher_map;
	if (!header || header_bytes < CACHE_FILE_HEADER_BYTES)
		return _map_share_policy_not_a_community_map;
	bytes.bytes = header;
	bytes.count = CACHE_FILE_HEADER_BYTES;
	source.context = &bytes;
	source.read = header_read;
	source.size = file_size;
	status = cache_file_identify(&source, &identity);
	if (identity.format == _cache_file_format_resource_map)
		return _map_share_policy_resource_map;
	if (status != _cache_file_status_ok || identity.format != _cache_file_format_custom_edition_cache)
		return _map_share_policy_not_a_community_map;
	if (publisher_name(identity.name, (uint32_t)strlen(identity.name)) || publisher_checksum(identity.checksum))
		return _map_share_policy_publisher_map;
	return _map_share_policy_community_map;
}

int map_share_policy_may_send(
	enum map_share_policy_verdict verdict)
{
	return verdict == _map_share_policy_community_map;
}

char const *map_share_policy_describe(
	enum map_share_policy_verdict verdict)
{
	return (unsigned)verdict < NUMBER_OF_MAP_SHARE_POLICY_VERDICTS ? verdict_descriptions[verdict] :
		"unknown";
}

char const *const *map_share_policy_publisher_names(
	int32_t *count)
{
	*count = (int32_t)(sizeof(publisher_names) / sizeof(publisher_names[0]));
	return publisher_names;
}
