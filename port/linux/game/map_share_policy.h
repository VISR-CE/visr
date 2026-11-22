/*
MAP_SHARE_POLICY.H

Which files map sharing may send to another player (map_share.c,
map_share_files.c): community maps only. Never Custom Edition's resource
maps (bitmaps.map, sounds.map, loc.map) and never one of the publisher's own
maps (Custom Edition's and Halo PC's), whatever the file is named: they are
the players' own copies of the game, not this machine's to hand out.

A file is judged by what it is, not by which path asks: its kind as map
sharing names it, the name it is shared under, and its header (the first
CACHE_FILE_HEADER_BYTES bytes). A map is the publisher's when its file name
(without the hash digits a second version's ends in) or its header's name is
one of the publisher's, or its header's checksum is one of theirs (a copy
renamed, header and all). Anything that is not a Custom Edition cache
(a resource map under a map's name, a Halo PC or Xbox cache, a header not
had yet or not read) is refused.

Standalone C with fixed-width types, as cache_file_formats.c is: compiled into
the game and into the test tools/test_map_share_send.py.
*/

#ifndef __MAP_SHARE_POLICY_H
#define __MAP_SHARE_POLICY_H

/* ---------- headers */

#include <stdint.h>

/* ---------- constants */

/* what map sharing may do with a file, and why not */
enum map_share_policy_verdict
{
	/* a community map: it may be sent */
	_map_share_policy_community_map,
	/* a resource map, by the kind it is sent as or by its header */
	_map_share_policy_resource_map,
	/* one of the publisher's maps, by name or checksum */
	_map_share_policy_publisher_map,
	/* not a Custom Edition cache, or its header not had */
	_map_share_policy_not_a_community_map,

	NUMBER_OF_MAP_SHARE_POLICY_VERDICTS
};

/* ---------- prototypes */

/* What a file is, for sending it: `kind` is 0 for a map, else the resource
map type it is sent as (enum resource_map_type, as map_share.h numbers
them); `name` the name it is shared under, `name_length` characters of it
(the file name without its folder, extension, or the hash digits a second
version's name ends in); `header` its first `header_bytes` bytes (NULL when
not had) of a file of `file_size` bytes. */
enum map_share_policy_verdict map_share_policy_judge(
	int32_t kind,
	char const *name,
	uint32_t name_length,
	void const *header,
	uint32_t header_bytes,
	uint32_t file_size);

/* whether a verdict lets the file be sent */
int map_share_policy_may_send(
	enum map_share_policy_verdict verdict);

/* the verdict as a log line says it */
char const *map_share_policy_describe(
	enum map_share_policy_verdict verdict);

/* the publisher's map names, `*count` of them (for the tests) */
char const *const *map_share_policy_publisher_names(
	int32_t *count);

#endif
