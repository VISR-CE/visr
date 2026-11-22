/*
MAP_SHARE_POLICY.C

Which files map sharing may send (map_share_policy.h).
*/

/* ---------- headers */

#include "map_share_policy.h"
#include "cache_file_formats.h"
#include "../../runtime/stock_maps.h"

#include <string.h>

/* a structure BSP's tag group */
#define STRUCTURE_BSP_GROUP_TAG 0x73627370UL

/* ---------- constants */

static char const *const verdict_descriptions[NUMBER_OF_MAP_SHARE_POLICY_VERDICTS] =
{
	"a community map",
	"a resource map (Custom Edition's own)",
	"a stock map (its content is Custom Edition's own map's)",
	"not a Custom Edition map",
	"a map not hashed yet",
	"a conversion of a campaign level (its scenario or a structure BSP is one of the campaign's)",
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

/* whether `length` characters of `name` are a resource map's name
(bitmaps, sounds, loc), in any case */
static int resource_map_name(
	char const *name,
	uint32_t length)
{
	int type;

	for (type = _resource_map_bitmaps; type < NUMBER_OF_RESOURCE_MAP_TYPES; type++)
	{
		char const *resource = resource_map_type_describe((enum resource_map_type)type);
		uint32_t position;

		if (strlen(resource) != length)
			continue;
		for (position = 0; position < length; position++)
		{
			char character = name[position];

			if (character >= 'A' && character <= 'Z')
				character = (char)(character - 'A' + 'a');
			if (character != resource[position])
				break;
		}
		if (position == length)
			return 1;
	}
	return 0;
}

/* ---------- campaign levels */

/* whether `path` lies in a campaign level's folder (levels\a10\...), in any
case */
static int campaign_level_path(
	char const *path)
{
	static char const prefix[] = "levels\\";
	int count;
	struct stock_map const *maps = stock_maps(&count);
	int index;

	for (index = 0; index < count; index++)
	{
		char const *level = maps[index].file;
		size_t position;
		size_t level_length = strlen(level);

		if (!(maps[index].flags & STOCK_MAP_CAMPAIGN))
			continue;
		for (position = 0; position < sizeof(prefix) - 1 + level_length; position++)
		{
			char expected = position < sizeof(prefix) - 1 ? prefix[position] : level[position - (sizeof(prefix) - 1)];
			char character = path[position];

			if (character >= 'A' && character <= 'Z')
				character = (char)(character - 'A' + 'a');
			if (character != expected)
				break;
		}
		if (position == sizeof(prefix) - 1 + level_length && path[position] == '\\')
			return 1;
	}
	return 0;
}

static int campaign_tag_visit(
	void *context,
	uint32_t group,
	char const *name,
	int is_scenario)
{
	int *campaign = context;

	if ((is_scenario || group == STRUCTURE_BSP_GROUP_TAG) && campaign_level_path(name))
	{
		*campaign = 1;
		return 0;
	}
	return 1;
}

/* ---------- public code */

int map_share_policy_tag_data(
	void const *header,
	uint32_t header_bytes,
	uint32_t file_size,
	uint32_t *offset,
	uint32_t *size)
{
	struct header_bytes bytes;
	struct cache_file_source source;
	struct cache_file_identity identity;

	if (!header || header_bytes < CACHE_FILE_HEADER_BYTES)
		return 0;
	bytes.bytes = header;
	bytes.count = CACHE_FILE_HEADER_BYTES;
	source.context = &bytes;
	source.read = header_read;
	source.size = file_size;
	if (cache_file_identify(&source, &identity) != _cache_file_status_ok ||
		identity.format != _cache_file_format_custom_edition_cache || !identity.tag_data_size)
	{
		return 0;
	}
	*offset = identity.tag_data_offset;
	*size = identity.tag_data_size;
	return 1;
}

int32_t map_share_policy_campaign(
	struct cache_file_source *source)
{
	int campaign = 0;

	return cache_file_tag_names(source, campaign_tag_visit, &campaign) == _cache_file_status_ok || campaign ?
		campaign : -1;
}


enum map_share_policy_verdict map_share_policy_judge(
	int32_t kind,
	char const *name,
	uint32_t name_length,
	void const *header,
	uint32_t header_bytes,
	uint32_t file_size,
	uint8_t const *content,
	int32_t campaign)
{
	struct header_bytes bytes;
	struct cache_file_source source;
	struct cache_file_identity identity;
	enum cache_file_status status;

	if (kind != _resource_map_none || !name || resource_map_name(name, name_length))
		return _map_share_policy_resource_map;
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
	if (!content || campaign < 0)
		return _map_share_policy_content_unknown;
	if (stock_map_with_content(content))
		return _map_share_policy_stock_map;
	if (campaign)
		return _map_share_policy_campaign_map;
	return _map_share_policy_community_map;
}

int map_share_policy_may_send(
	enum map_share_policy_verdict verdict)
{
	return verdict == _map_share_policy_community_map;
}

int map_share_policy_may_hash(
	enum map_share_policy_verdict verdict)
{
	return verdict == _map_share_policy_community_map || verdict == _map_share_policy_content_unknown;
}

char const *map_share_policy_describe(
	enum map_share_policy_verdict verdict)
{
	return (unsigned)verdict < NUMBER_OF_MAP_SHARE_POLICY_VERDICTS ? verdict_descriptions[verdict] :
		"unknown";
}
