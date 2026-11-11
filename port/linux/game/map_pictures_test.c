/* SPDX-License-Identifier: Apache-2.0 */
#include "map_pictures.h"

/* Game builds discover every C file here; only standalone tests define main. */
#ifdef MAP_PICTURES_TEST

#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct map_pictures_metadata fixture(void)
{
	struct map_pictures_metadata m = {0};
	strcpy(m.map_key, "stock:example");
	memset(m.map_hash, 'a', 64);
	m.framing_revision = 3;
	m.renderer_version = 2;
	m.width = 2048; m.height = 1024;
	return m;
}

static void round_trip(struct map_pictures_metadata m)
{
	char json[MAP_PICTURES_META_SIZE], again[MAP_PICTURES_META_SIZE];
	struct map_pictures_metadata parsed;
	struct map_pictures_request wanted = {0};
	assert(map_pictures_meta_write(json, sizeof(json), &m));
	assert(map_pictures_meta_parse(json, strlen(json), &parsed));
	assert(map_pictures_meta_write(again, sizeof(again), &parsed));
	assert(!strcmp(json, again));
	assert(parsed.pre_game == m.pre_game);
	wanted.metadata = m;
	assert(!map_pictures_is_stale(&parsed, &wanted));
}

static void reject_replacement(const char *json, const char *old, const char *replacement)
{
	char bad[MAP_PICTURES_META_SIZE * 2];
	const char *at = strstr(json, old);
	struct map_pictures_metadata out = fixture(), before = out;
	size_t prefix;
	assert(at);
	prefix = (size_t)(at - json);
	assert(prefix + strlen(replacement) + strlen(at + strlen(old)) < sizeof(bad));
	memcpy(bad, json, prefix);
	strcpy(bad + prefix, replacement);
	strcpy(bad + prefix + strlen(replacement), at + strlen(old));
	assert(!map_pictures_meta_parse(bad, strlen(bad), &out));
	assert(!memcmp(&out, &before, sizeof(out)));
}

static void test_metadata(void)
{
	struct map_pictures_metadata m = fixture(), out;
	char json[MAP_PICTURES_META_SIZE], large[MAP_PICTURES_META_SIZE + 1];
	size_t i;
	static const char *bad_numbers[] = {"NaN", "nan", "Infinity", "1e999", "-1", "01", "1.0", "1e0",
		"4294967296", "999999999999999999999999", "null", "true", "[]", "{}", "\"2\""};
	round_trip(m);
	m.pre_game = 1; m.texture_set = MAP_PICTURES_UPSCALED;
	round_trip(m);
	m.framing = MAP_PICTURES_AUTO; m.auto_version = 7; m.framing_revision = 0;
	round_trip(m);
	m.auto_version = UINT32_MAX; m.renderer_version = UINT32_MAX;
	round_trip(m);
	m = fixture();
	assert(map_pictures_meta_write(json, sizeof(json), &m));
	for (i = 0; i < strlen(json); i++) assert(!map_pictures_meta_parse(json, i, &out));
	/* Apply each malformed number to a numeric field. */
	for (i = 0; i < sizeof(bad_numbers) / sizeof(bad_numbers[0]); i++)
	{
		char replacement[128];
		snprintf(replacement, sizeof(replacement), "\"renderer_version\":%s", bad_numbers[i]);
		reject_replacement(json, "\"renderer_version\":2", replacement);
	}
	reject_replacement(json, "\"schema\":1", "\"schema\":2");
	reject_replacement(json, "\"schema\":1", "\"schema\":1,\"schema\":1");
	reject_replacement(json, "\"schema\":1,", "");
	reject_replacement(json, "\"schema\":1", "\"unknown\":1");
	reject_replacement(json, "original", "broken");
	reject_replacement(json, "curated", "auto:0");
	reject_replacement(json, "curated", "auto:01");
	reject_replacement(json, "curated", "auto: 1");
	reject_replacement(json, "curated", "auto:4294967296");
	reject_replacement(json, "curated", "auto:NaN");
	reject_replacement(json, "\"width\":2048", "\"width\":0");
	reject_replacement(json, "\"height\":1024", "\"height\":16385");
	reject_replacement(json, "\"framing_revision\":3", "\"framing_revision\":0");
	reject_replacement(json, "\"pre_game\":false", "\"pre_game\":0");
	reject_replacement(json, "\"pre_game\":false", "\"pre_game\":falsehood");
	reject_replacement(json, "stock:example", "stock:exam\\ple");
	reject_replacement(json, "stock:example", "stock:exam\nple");
	reject_replacement(json, "stock:example", "stock:exam\xc3\xa9ple");
	reject_replacement(json, "stock:example", "");
	reject_replacement(json, "aaaaaaaa", "AAAAAAAa");
	reject_replacement(json, "}", ",}");
	reject_replacement(json, "}", "}garbage");
	memset(large, 'x', sizeof(large) - 1); large[sizeof(large) - 1] = 0;
	reject_replacement(json, "stock:example", large);
	assert(!map_pictures_meta_parse(large, sizeof(large), &out));
	memcpy(large, json, strlen(json)); large[10] = 0;
	assert(!map_pictures_meta_parse(large, strlen(json), &out));
	assert(map_pictures_meta_parse(json, strlen(json), &out));
	/* Field order and JSON whitespace do not affect the schema. */
	{
		const char *reordered = " \n{\"pre_game\":true,\"height\":1024,\"width\":2048,"
			"\"framing\":\"auto:2\",\"texture_set\":\"upscaled\",\"renderer_version\":1,"
			"\"framing_revision\":0,\"map_key\":\"stock:made_up\",\"schema\":1,"
			"\"map_hash\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"}\t";
		assert(map_pictures_meta_parse(reordered, strlen(reordered), &out));
		assert(out.auto_version == 2 && out.pre_game && out.texture_set == MAP_PICTURES_UPSCALED);
	}
	assert(!map_pictures_meta_write(json, 1, &m) && !json[0]);
	assert(!map_pictures_meta_write(json, 0, &m));
	assert(!map_pictures_meta_write(NULL, sizeof(json), &m));
	assert(!map_pictures_meta_write(json, sizeof(json), NULL));
	m.pre_game = 2; assert(!map_pictures_meta_write(json, sizeof(json), &m));
	m = fixture(); m.texture_set = (enum map_pictures_texture_set)9;
	assert(!map_pictures_meta_write(json, sizeof(json), &m));
	m = fixture(); m.framing = (enum map_pictures_framing)9;
	assert(!map_pictures_meta_write(json, sizeof(json), &m));
	m = fixture(); memset(m.map_key, 'a', sizeof(m.map_key));
	assert(!map_pictures_meta_write(json, sizeof(json), &m));
	assert(!map_pictures_meta_parse(NULL, 10, &out));
	assert(!map_pictures_meta_parse("{}", 2, NULL));
}

static void test_staleness(void)
{
	struct map_pictures_metadata cached = fixture();
	struct map_pictures_request wanted = {0};
	wanted.metadata = cached;
	assert(!map_pictures_is_stale(&cached, &wanted));
#define STALE_CHANGE(change) do { wanted.metadata = cached; change; assert(map_pictures_is_stale(&cached, &wanted)); } while (0)
	STALE_CHANGE(wanted.metadata.framing_revision++);
	STALE_CHANGE(wanted.metadata.renderer_version++);
	STALE_CHANGE(wanted.metadata.texture_set = MAP_PICTURES_UPSCALED);
	STALE_CHANGE(wanted.metadata.map_hash[0] = 'b');
	STALE_CHANGE(wanted.metadata.map_key[6] = 'x');
	STALE_CHANGE(wanted.metadata.width++);
	STALE_CHANGE(wanted.metadata.height++);
#undef STALE_CHANGE
	wanted.metadata = cached;
	wanted.settle_ticks = 30; wanted.erase_ai = 1;
	assert(!map_pictures_is_stale(&cached, &wanted));
	cached.pre_game = 1;
	assert(map_pictures_is_stale(&cached, &wanted));
	wanted.erase_ai = 0;
	assert(map_pictures_is_stale(&cached, &wanted));
	wanted.settle_ticks = 0; wanted.erase_ai = 1;
	assert(map_pictures_is_stale(&cached, &wanted));
	wanted.erase_ai = 0;
	assert(!map_pictures_is_stale(&cached, &wanted));
	cached.framing = MAP_PICTURES_AUTO; cached.auto_version = 1;
	assert(map_pictures_is_stale(&cached, &wanted));
	wanted.metadata = cached;
	assert(!map_pictures_is_stale(&cached, &wanted));
	wanted.metadata.auto_version++;
	assert(map_pictures_is_stale(&cached, &wanted));
	assert(map_pictures_is_stale(NULL, &wanted));
	assert(map_pictures_is_stale(&cached, NULL));
}

static void test_paths(void)
{
	char path[1024], small[2], key[MAP_PICTURES_KEY_SIZE + 1];
	assert(map_pictures_cache_path(path, sizeof(path), "cache", "stock:example"));
	assert(!strcmp(path, "cache/map-pictures/stock%3Aexample/"));
	assert(map_pictures_cache_path(path, sizeof(path), "cache/", "../a:b\\c%"));
	assert(!strcmp(path, "cache/map-pictures/%2E%2E%2Fa%3Ab%5Cc%25/"));
	assert(map_pictures_cache_path(path, sizeof(path), "/", ".."));
	assert(!strcmp(path, "/map-pictures/%2E%2E/"));
	assert(map_pictures_cache_path(path, sizeof(path), "cache", "%3A"));
	assert(!strcmp(path, "cache/map-pictures/%253A/"));
	assert(map_pictures_cache_path(path, sizeof(path), "cache", "a_b-c9"));
	assert(!strcmp(path, "cache/map-pictures/a_b-c9/"));
	assert(!map_pictures_cache_path(small, sizeof(small), "cache", "a") && !small[0]);
	assert(!map_pictures_cache_path(path, sizeof(path), "cache", ""));
	assert(!map_pictures_cache_path(path, sizeof(path), "", "a"));
	assert(!map_pictures_cache_path(NULL, 0, "cache", "a"));
	memset(key, '.', sizeof(key) - 1); key[sizeof(key) - 1] = 0;
	assert(!map_pictures_cache_path(path, sizeof(path), "cache", key));
	key[MAP_PICTURES_KEY_SIZE - 1] = 0;
	assert(map_pictures_cache_path(path, sizeof(path), "cache", key));
}

static void test_lookup(void)
{
	unsigned int mask;
	/* All 128 combinations include both values of the name-tile flag. */
	for (mask = 0; mask < 128; mask++)
	{
		struct map_pictures_availability a = {mask & 1, mask & 2, mask & 4, mask & 8,
			mask & 16, mask & 32, mask & 64};
		enum map_pictures_source expected = MAP_PICTURES_NAME_TILE;
		if (a.stock) expected = MAP_PICTURES_STOCK;
		if (a.is_custom_map && !a.graphics_mode_hardcore && a.auto_picture) expected = MAP_PICTURES_AUTO_PICTURE;
		if (a.is_custom_map && a.custom_bmp) expected = MAP_PICTURES_CUSTOM_BMP;
		if (!a.graphics_mode_hardcore && a.rendered) expected = MAP_PICTURES_RENDERED;
		assert(map_pictures_lookup(a) == expected);
	}
}

static void test_scheduler(void)
{
	int state;
	static const int allowed[] = {1, 1, 0, 0, 0, 0, 1};
	for (state = MAP_PICTURES_IMPORT; state <= MAP_PICTURES_NETWORK_CLIENT_PRE_GAME; state++)
		assert(map_pictures_render_allowed((enum map_pictures_state)state) == allowed[state]);
	assert(!map_pictures_render_allowed((enum map_pictures_state)-1));
	assert(!map_pictures_render_allowed((enum map_pictures_state)99));
}

static void test_names(void)
{
	char text[64];
	assert(map_pictures_name_tile(text, sizeof(text), "Example map", 7) == 7);
	assert(!strcmp(text, "EXAMPLE"));
	assert(map_pictures_name_tile(text, 4, "example", 99) == 3 && !strcmp(text, "EXA"));
	assert(map_pictures_name_tile(text, sizeof(text), "a\xc3\xa9\xe4\xb8\xad\xf0\x9f\x8c\x8bz", 4) == 10);
	assert(!strcmp(text, "A\xc3\xa9\xe4\xb8\xad\xf0\x9f\x8c\x8b"));
	assert(map_pictures_name_tile(text, 3, "a\xc3\xa9z", 10) == 1 && !strcmp(text, "A"));
	assert(map_pictures_name_tile(text, 4, "a\xc3\xa9z", 10) == 3 && !strcmp(text, "A\xc3\xa9"));
	assert(!map_pictures_name_tile(text, 4, "\xf0\x9f\x8c\x8b", 10));
	assert(map_pictures_name_tile(text, 5, "\xf0\x9f\x8c\x8b", 1) == 4);
	assert(!map_pictures_name_tile(text, sizeof(text), "example", 0) && !text[0]);
	assert(!map_pictures_name_tile(text, 1, "example", 99) && !text[0]);
	assert(!map_pictures_name_tile(NULL, 0, "example", 99));
	assert(!map_pictures_name_tile(text, sizeof(text), NULL, 99));
	assert(!map_pictures_name_tile(text, sizeof(text), "", 99));
	assert(map_pictures_name_tile(text, sizeof(text), "a\xc3", 99) == 1);
	assert(map_pictures_name_tile(text, sizeof(text), "a\xe4\xb8", 99) == 1);
	assert(map_pictures_name_tile(text, sizeof(text), "a\xf0\x9f\x8c", 99) == 1);
	assert(map_pictures_name_tile(text, sizeof(text), "a\xc0\xaf", 99) == 1);
	assert(map_pictures_name_tile(text, sizeof(text), "a\xed\xa0\x80", 99) == 1);
	assert(map_pictures_name_tile(text, sizeof(text), "a\xf4\x90\x80\x80", 99) == 1);
}

int main(void)
{
	test_metadata(); test_staleness(); test_paths(); test_lookup(); test_scheduler(); test_names();
	puts("map_pictures_test: metadata, staleness, paths, 128 lookup combinations, scheduler, UTF-8 names passed");
	return 0;
}

#endif
