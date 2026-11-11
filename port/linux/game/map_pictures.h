/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAP_PICTURES_H
#define MAP_PICTURES_H

#include <stddef.h>
#include <stdint.h>

#define MAP_PICTURES_KEY_SIZE 128
#define MAP_PICTURES_META_SIZE 1024
#define MAP_PICTURES_MAX_DIMENSION 16384

enum map_pictures_texture_set { MAP_PICTURES_ORIGINAL, MAP_PICTURES_UPSCALED };
enum map_pictures_framing { MAP_PICTURES_CURATED, MAP_PICTURES_AUTO };

struct map_pictures_metadata
{
	char map_key[MAP_PICTURES_KEY_SIZE];
	uint32_t framing_revision, renderer_version;
	enum map_pictures_texture_set texture_set;
	char map_hash[65]; /* BLAKE2b-256, lowercase hexadecimal. */
	uint32_t width, height;
	enum map_pictures_framing framing;
	uint32_t auto_version; /* Positive for auto:<version>, zero for curated. */
	int pre_game;
};

struct map_pictures_request
{
	struct map_pictures_metadata metadata;
	uint32_t settle_ticks;
	int erase_ai;
};

/* Schema 1, ASCII identifiers only. Parse consumes exactly length bytes and
   leaves out unchanged on failure. Write clears buf on failure. */
int map_pictures_meta_write(char *buf, size_t size, const struct map_pictures_metadata *metadata);
int map_pictures_meta_parse(const char *json, size_t length, struct map_pictures_metadata *out);
int map_pictures_is_stale(const struct map_pictures_metadata *cached, const struct map_pictures_request *wanted);
/* root is trusted. The key becomes one percent-encoded component; no I/O. */
int map_pictures_cache_path(char *buf, size_t size, const char *root, const char *map_key);

enum map_pictures_source
{
	MAP_PICTURES_RENDERED, MAP_PICTURES_CUSTOM_BMP, MAP_PICTURES_AUTO_PICTURE,
	MAP_PICTURES_STOCK, MAP_PICTURES_NAME_TILE
};
struct map_pictures_availability
{
	int rendered, custom_bmp, auto_picture, stock, name_tile;
	int is_custom_map, graphics_mode_hardcore;
};
/* auto_picture means an automatic picture that passed quality checks.
   The name tile remains the final fallback even when name_tile is zero. */
enum map_pictures_source map_pictures_lookup(struct map_pictures_availability available);

enum map_pictures_state
{
	MAP_PICTURES_IMPORT, MAP_PICTURES_PRE_GAME, MAP_PICTURES_GAMEPLAY,
	MAP_PICTURES_PAUSED, MAP_PICTURES_MENU, MAP_PICTURES_LOADING_SCREEN,
	MAP_PICTURES_NETWORK_CLIENT_PRE_GAME
};
int map_pictures_render_allowed(enum map_pictures_state state);

/* max_length counts Unicode code points, size counts bytes including NUL.
   ASCII letters become uppercase; other UTF-8 code points are preserved.
   Invalid UTF-8 stops the text before that sequence. Returns bytes written. */
size_t map_pictures_name_tile(char *buf, size_t size, const char *name, size_t max_length);

#endif
