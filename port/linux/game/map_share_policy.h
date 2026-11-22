/*
MAP_SHARE_POLICY.H

Which files map sharing may send to another player (map_share.c,
map_share_files.c): community maps only. Never Custom Edition's resource
maps (bitmaps.map, sounds.map, loc.map), never Custom Edition's own stock
maps, and never a conversion of a campaign level: they are the players' own
copies of the game, or built from its campaign, not this machine's to hand
out.

A file is judged by what it is, not by which path asks: its kind as map
sharing names it, the name it is shared under, its header (the first
CACHE_FILE_HEADER_BYTES bytes) and its content hash (map_share_content_hash:
everything but the header). A resource map is known by the kind it is sent
as, by its name (without the hash digits a second version's ends in), or by
its header; a stock map by its content hash, which a copy keeps under any name
and with any header, an edited checksum too (port/runtime/stock_maps.h). A
community map that reuses a stock map's name but differs in content may be
sent. Anything that is not a valid Custom Edition cache (a Halo PC or Xbox
cache, a header not had yet or not read) is refused, and a map whose content
hash is not known yet may be hashed but not sent. A conversion of a
campaign level (Bungie's campaign content) is refused by its tags: its
scenario or a structure BSP in a campaign level's folder.

Standalone C with fixed-width types, as cache_file_formats.c is: compiled into
the game and into the test tools/test_map_share_send.py.
*/

#ifndef __MAP_SHARE_POLICY_H
#define __MAP_SHARE_POLICY_H

/* ---------- headers */

#include <stdint.h>

/* ---------- constants */

/* a content hash's bytes (BLAKE2b-256's) */
#define MAP_SHARE_POLICY_CONTENT_BYTES 32

/* what map sharing may do with a file, and why not */
enum map_share_policy_verdict
{
	/* a community map: it may be sent */
	_map_share_policy_community_map,
	/* a resource map, by the kind it is sent as or by its header */
	_map_share_policy_resource_map,
	/* a stock map: Custom Edition's own, by its content hash */
	_map_share_policy_stock_map,
	/* not a Custom Edition cache, or its header not had */
	_map_share_policy_not_a_community_map,
	/* a Custom Edition cache whose content hash, or whether it is a
	campaign conversion, is not known yet: it may be hashed, not sent */
	_map_share_policy_content_unknown,
	/* a conversion of one of the campaign's levels, by its tags */
	_map_share_policy_campaign_map,

	NUMBER_OF_MAP_SHARE_POLICY_VERDICTS
};

/* ---------- prototypes */

/* What a file is, for sending it: `kind` is 0 for a map, else the resource
map type it is sent as (enum resource_map_type, as map_share.h numbers
them); `name` the name it is shared under, `name_length` characters of it
(the file name without its folder, extension, or the hash digits a second
version's name ends in); `header` its first `header_bytes` bytes (NULL when
not had) of a file of `file_size` bytes; `content` its content hash
(MAP_SHARE_POLICY_CONTENT_BYTES bytes, NULL when not known yet); `campaign`
map_share_policy_campaign's answer for it (-1 when not known yet). */
enum map_share_policy_verdict map_share_policy_judge(
	int32_t kind,
	char const *name,
	uint32_t name_length,
	void const *header,
	uint32_t header_bytes,
	uint32_t file_size,
	uint8_t const *content,
	int32_t campaign);

/* Whether a Custom Edition cache is a conversion of a campaign level, by its
tags, not its name: 1 when its scenario's tag, or any structure BSP's, lies
in a campaign level's folder (levels\a10\..., the campaign rows of
port/runtime/stock_maps.h), however the file or its header is named; 0 when
none does; -1 when its tags cannot be read. Community maps that only borrow
a campaign level's shaders or sounds are not conversions. */
struct cache_file_source;
int32_t map_share_policy_campaign(
	struct cache_file_source *source);

/* where a Custom Edition cache's tag data lies (`*offset`, `*size` bytes),
from its header: what has to be read for map_share_policy_campaign; 0 when
the header is not a valid Custom Edition cache's */
int map_share_policy_tag_data(
	void const *header,
	uint32_t header_bytes,
	uint32_t file_size,
	uint32_t *offset,
	uint32_t *size);

/* whether a verdict lets the file be sent */
int map_share_policy_may_send(
	enum map_share_policy_verdict verdict);

/* whether a verdict lets the file be hashed for sending (it may be sent, or
its content hash is what is still to be known) */
int map_share_policy_may_hash(
	enum map_share_policy_verdict verdict);

/* the verdict as a log line says it */
char const *map_share_policy_describe(
	enum map_share_policy_verdict verdict);

#endif
