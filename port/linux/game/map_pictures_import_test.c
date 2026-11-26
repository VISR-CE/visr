/* SPDX-License-Identifier: Apache-2.0 */
#ifdef MAP_PICTURES_IMPORT_TEST
/* Exercise the production state machine without the Xbox platform headers or game data. */
#define HALO_IOS
#define __HALO_LINUX_PLATFORM_H
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
void platform_log(const char *format, ...);
#include "../src/map_pictures_render.c"

static int refresh_ok, refresh_calls, stub_revision = 7, stub_hash = 4242;
static int pending, development, available, menu = 1, game, awake, muted, done_calls, identity_fails;
static int progress_calls, progress_done, identities, loaded_scenario, completed;
static double clock_ms;
static char test_root[1024];

void platform_log(const char *format, ...)
{
	va_list arguments;
	if (!getenv("MAP_PICTURES_TEST_LOG")) return;
	va_start(arguments, format); vfprintf(stderr, format, arguments); fputc('\n', stderr); va_end(arguments);
}
int config_boolean(const char *name) { (void)name; return development; }
static const char *suite_name = "";
const char *config_string(const char *name) { (void)name; return suite_name; }
int host_photo_mode_available(void) { return available; }
int host_map_pictures_import_stage(void) { return pending; }
/* the host side of a launch refresh: the stage comes up (pending) or the host has none */
int host_map_pictures_begin_refresh(void)
{
	refresh_calls++;
	if (!refresh_ok) return 0;
	pending = 1; identities = 0;
	return 1;
}
void host_map_pictures_keep_awake(int on) { awake = on; }
void host_texture_cache_directory(char *text, unsigned long size)
{
	snprintf(text, size, "%s/texture-cache", test_root);
}
void host_map_pictures_import_progress(int done, int total, const char *name)
{
	assert(total == 23 && done >= progress_done && done <= total && name);
	progress_done = done;
	progress_calls++;
}
void host_map_pictures_import_done(int rendered, int cached, int failed)
{
	assert(menu && !awake && !muted);
	assert(rendered + cached + failed == 23);
	completed = rendered + cached;
	done_calls++;
	pending = 0;
}
void benchmark_scenario_load(const char *map, const char *difficulty)
{
	int i;
	(void)difficulty;
	if (!*map) { menu = 1; game = 0; return; }
	for (i = 0; i < map_pictures_scenario_count(); i++)
		if (!strcmp(map, map_pictures_scenario_name(i))) break;
	assert(i < map_pictures_scenario_count());
	loaded_scenario = i;
	menu = 0; game = 1;
	/* The new map finishes loading at once here; the import goes map to map without the menu. */
	render.stages[STAGE_COUNT - 1] = clock_ms;
}
unsigned char benchmark_scenario_at_menu(void) { return (unsigned char)menu; }
unsigned char filesystem_check_thread_is_active(void) { return 0; }
unsigned char bink_playback_in_progress(void) { return 0; }
unsigned char cache_files_precache_in_progress(void) { return 0; }
void halo_benchmark_overrides(long a, long b, long c) { (void)a; (void)b; (void)c; }
double halo_frame_trace_milliseconds(void) { return clock_ms; }
int posix_make_directory(const char *path) { return mkdir(path, 0700); }
short cache_file_requests_pending(void) { return 0; }
unsigned long cache_file_reads_started(void) { return 0; }
unsigned int gpu_compiles_outstanding(void) { return 0; }
uint32_t halo_pipelines_built(void) { return 0; }
static int hashes;
static char stub_stamp[MAP_PICTURES_STAMP_SIZE] = "100:200";
int map_pictures_scenario_key(int index, char *key, size_t size)
{
	const char *name = map_pictures_scenario_name(index), *base = strrchr(name, '\\') + 1;
	if (identity_fails) return 0;
	snprintf(key, size, "stock:%s", base);
	return 1;
}
int map_pictures_scenario_identity(int index, char *key, size_t size, char hash[65], char stamp[MAP_PICTURES_STAMP_SIZE],
	const char *known_stamp, const char *known_hash, uint32_t *revision, uint32_t *framing_hash)
{
	assert(index == (pending ? map_pictures_scenario_import_index(identities) : identities));
	identities++;
	if (!map_pictures_scenario_key(index, key, size)) return 0;
	snprintf(stamp, MAP_PICTURES_STAMP_SIZE, "%s", stub_stamp);
	/* the production decision (scenario.c uses the same function): hash the file only when the cache cannot stand in */
	if (map_pictures_hash_known(stamp, known_stamp, known_hash)) memcpy(hash, known_hash, 65);
	else { hashes++; memset(hash, 'a', 64); hash[64] = 0; }
	*revision = (uint32_t)stub_revision; *framing_hash = (uint32_t)stub_hash;
	return 1;
}
static int network;
int map_pictures_scenario_network_active(void) { return network; }
int map_pictures_scenario_in_game(void) { return game; }
int map_pictures_scenario_in_cinematic(void) { return 0; }
long map_pictures_scenario_ticks(void) { return 0; }
void map_pictures_scenario_fast_forward(int on) { (void)on; }
int map_pictures_scenario_fast_forwarding(void) { return 0; }
void photo_mode_end(void) { }
void photo_mode_prepare_variant(const char *key) { (void)key; }
void photo_mode_clear_variant(void) { }
void dsound_set_capture_mute(int on) { muted = on; }
void photo_mode_capture_begin(void) { }
static int refuse_framing;
int photo_mode_capture_state(int *framed, int *revision) { *framed = !refuse_framing; *revision = stub_revision; return 1; }

static void reset(int import, int dev, int photo)
{
	memset(&render, 0, sizeof(render));
	pending = import; development = dev; available = photo; menu = 1; game = 0;
	done_calls = progress_calls = progress_done = identities = completed = identity_fails = 0;
	refresh_ok = refresh_calls = hashes = 0; strcpy(stub_stamp, "100:200"); network = refuse_framing = 0; stub_revision = 7; stub_hash = 4242;
}

static void run_maps(void)
{
	int updates;
	for (updates = 0; updates < 2000 && render.state != STATE_DONE; updates++)
	{
		clock_ms += 100;
		map_pictures_render_update();
		if (render.import_mode && render.index > 0 && render.state != STATE_DONE && render.state != STATE_LEAVING)
			assert(!menu); /* Never back to the menu between import maps. */
		if (render.state == STATE_LOADING)
			assert(loaded_scenario == (render.import_mode ?
				map_pictures_scenario_import_index(render.index) : render.index));
		/* the render hold is one switch: the screen is kept awake exactly while the sound is off */
		assert(muted == awake);
		/* GPU readback is unavailable in this asset-free harness. */
		if (render.state == STATE_FRAME_PENDING)
		{
			render.wants_frame = 0;
			render.frame_done = render.frame_ok = 1;
		}
	}
	assert(render.state == STATE_DONE && !awake && !muted);
}

static void ordering(void)
{
	const char *campaign[] = { "a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40" };
	/* the render order players have always seen: the campaign in order, then multiplayer alphabetically */
	const char *scenarios[] = {
		"levels\\a10\\a10", "levels\\a30\\a30", "levels\\a50\\a50", "levels\\b30\\b30", "levels\\b40\\b40",
		"levels\\c10\\c10", "levels\\c20\\c20", "levels\\c40\\c40", "levels\\d20\\d20", "levels\\d40\\d40",
		"levels\\test\\beavercreek\\beavercreek", "levels\\test\\bloodgulch\\bloodgulch",
		"levels\\test\\boardingaction\\boardingaction", "levels\\test\\carousel\\carousel",
		"levels\\test\\chillout\\chillout", "levels\\test\\damnation\\damnation",
		"levels\\test\\hangemhigh\\hangemhigh", "levels\\test\\longest\\longest",
		"levels\\test\\prisoner\\prisoner", "levels\\test\\putput\\putput",
		"levels\\test\\ratrace\\ratrace", "levels\\test\\sidewinder\\sidewinder",
		"levels\\test\\wizard\\wizard",
	};
	int seen[23] = {0}, i;
	assert(map_pictures_scenario_count() == 23);
	for (i = 0; i < 23; i++) assert(!strcmp(map_pictures_scenario_name(i), scenarios[i]));
	assert(map_pictures_scenario_import_index(-1) == -1);
	assert(map_pictures_scenario_import_index(23) == -1);
	for (i = 0; i < 23; i++)
	{
		int index = map_pictures_scenario_import_index(i);
		const char *name = map_pictures_scenario_name(index);
		assert(index >= 0 && index < 23 && !seen[index]); seen[index] = 1;
		if (i < 13) assert(strstr(name, "levels\\test\\") == name);
		else assert(!strcmp(strrchr(name, '\\') + 1, campaign[i - 13]));
	}
}

/* The class: a cached picture that no longer matches what would be rendered now (framing revision, the framing's
content at the same revision, renderer version, a cache from before the content fingerprint) is queued again at the
next launch's main menu, through the host's stage; a current cache and a running game queue nothing. The harness
reads no frames, so each case writes the cache a finished run would have left. */
static void seed_cache(int revision, int hash, void (*change)(const char *key, struct map_pictures_metadata *))
{
	int i;
	for (i = 0; i < map_pictures_scenario_count(); i++)
	{
		char directory[1100], path[1200], json[MAP_PICTURES_META_SIZE], key[MAP_PICTURES_KEY_SIZE];
		struct map_pictures_metadata m;
		memset(&m, 0, sizeof(m));
		snprintf(key, sizeof(key), "stock:%s", strrchr(map_pictures_scenario_name(i), '\\') + 1);
		snprintf(m.map_key, sizeof(m.map_key), "%s", key);
		memset(m.map_hash, 'a', 64);
		m.framing = MAP_PICTURES_CURATED; m.framing_revision = (uint32_t)revision; m.framing_hash = (uint32_t)hash;
		m.renderer_version = RENDERER_VERSION; m.texture_set = MAP_PICTURES_ORIGINAL;
		m.width = PICTURE_WIDTH; m.height = PICTURE_HEIGHT;
		snprintf(m.map_stamp, sizeof(m.map_stamp), "100:200");
		if (change) change(key, &m);
		assert(map_pictures_meta_write(json, sizeof(json), &m));
		assert(map_pictures_cache_path(directory, sizeof(directory), test_root, key) && make_directories(directory));
		snprintf(path, sizeof(path), "%smeta.json", directory);
		assert(file_write(path, json, strlen(json)));
		snprintf(path, sizeof(path), "%spicture.bmp", directory);
		assert(file_write(path, "bmp", 3));
		snprintf(path, sizeof(path), "%sattempt.txt", directory);
		unlink(path);
	}
}
static void no_stamps(const char *key, struct map_pictures_metadata *m)
{
	(void)key; m->map_stamp[0] = 0;
}
static void old_renderer_a30(const char *key, struct map_pictures_metadata *m)
{
	if (!strcmp(key, "stock:a30")) m->renderer_version--;
}
static void no_fingerprint_b30(const char *key, struct map_pictures_metadata *m)
{
	if (!strcmp(key, "stock:b30")) m->framing_hash = 0; /* written without the field */
}

static void launch_refresh(void)
{
	/* a cache the last run left current: scanned, nothing queued */
	seed_cache(8, 4243, NULL);
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4243; run_maps();
	assert(identities == 23 && !refresh_calls && !pending && !done_calls && !render.rendered);
	/* the class: a launch whose pictures are current reads no map at all (every hash comes from the cache) */
	assert(hashes == 0);
	/* a map file replaced under a new stamp is hashed again, and a cache without stamps (older metadata) too */
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4243; strcpy(stub_stamp, "101:200"); run_maps();
	assert(hashes == 23 && !refresh_calls);
	/* the class: that launch wrote the new stamps beside the current pictures, so the next one reads no map */
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4243; strcpy(stub_stamp, "101:200"); run_maps();
	assert(hashes == 0 && !refresh_calls && !render.rendered);
	seed_cache(8, 4243, no_stamps);
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4243; strcpy(stub_stamp, "101:200"); run_maps();
	assert(hashes == 23 && !refresh_calls);
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4243; strcpy(stub_stamp, "101:200"); run_maps();
	assert(hashes == 0 && !refresh_calls);
	seed_cache(8, 4243, NULL);
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4243; run_maps();
	assert(hashes == 0);
	/* a game already running: the scan never starts, so nothing loads during play */
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4243; game = 1; menu = 0; run_maps();
	assert(!identities && !refresh_calls && !render.rendered);
	/* a lobby or network game exists: the scan never starts */
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4244; network = 1; run_maps();
	assert(!identities && !refresh_calls && !render.rendered);
	/* a saved framing the game refuses is drawn by the automatic camera: the first launch renders it, the next
	finds the recorded attempt and queues nothing (the class: a map that cannot match its request must not bring
	the stage up at every launch) */
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4244; refuse_framing = 1; run_maps();
	assert(refresh_calls == 1 && render.rendered == 23);
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4244; run_maps();
	assert(!refresh_calls && !render.rendered && identities == 23);
	/* ... until the framing changes again */
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4245; run_maps();
	assert(refresh_calls == 1 && render.rendered == 23);
	seed_cache(8, 4243, NULL);
	/* a framing edited without a revision change: every picture is rendered again, through the stage */
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4244; run_maps();
	assert(refresh_calls == 1 && done_calls == 1 && render.rendered == 23 && !pending && progress_done == 23);
	/* a new framing revision */
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 9; stub_hash = 4243; run_maps();
	assert(refresh_calls == 1 && done_calls == 1 && render.rendered == 23);
	/* one picture from an older renderer: only that map is rendered again */
	seed_cache(8, 4243, old_renderer_a30);
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4243; run_maps();
	assert(refresh_calls == 1 && render.rendered == 1 && render.cached == 22 && completed == 23);
	/* a cache written before the fingerprint existed */
	seed_cache(8, 4243, no_fingerprint_b30);
	reset(0, 0, 0); refresh_ok = 1; stub_revision = 8; stub_hash = 4243; run_maps();
	assert(refresh_calls == 1 && render.rendered == 1 && render.cached == 22);
	/* a benchmark suite owns map loading: no scan */
	seed_cache(8, 4243, NULL);
	reset(0, 0, 0); refresh_ok = 1; stub_hash = 4244; suite_name = "a10"; run_maps(); suite_name = "";
	assert(!identities && !refresh_calls);
	/* a host without the stage (tvOS, visionOS) renders nothing and asks once */
	seed_cache(8, 4243, NULL);
	reset(0, 0, 0); stub_revision = 8; stub_hash = 4244; run_maps();
	assert(refresh_calls == 1 && !render.rendered && !done_calls);
}

int main(int argc, char **argv)
{
	uint8_t *data; uint32_t size;
	char directory[1100], path[1200], metadata[MAP_PICTURES_META_SIZE];
	struct map_pictures_metadata saved;
	assert(argc == 2);
	snprintf(test_root, sizeof(test_root), "%s", argv[1]);
	ordering();
	/* no import stage and no photo mode: the launch scan finds every picture missing and asks the host for the
	stage; this host has none, so nothing is rendered (and one map was hashed, no more) */
	reset(0, 1, 0); run_maps();
	assert(identities == 1 && refresh_calls == 1 && !progress_calls && !done_calls && !render.rendered);
	reset(1, 0, 0); run_maps();
	assert(render.rendered == 23 && completed == 23 && done_calls == 1 && progress_done == 23);
	assert(progress_calls == 25); /* Initial zero, one per map, final completion. */
	saved = render.request.metadata;
	reset(1, 0, 0); identity_fails = 1; run_maps();
	assert(render.failed == 23 && completed == 0 && done_calls == 1);
	reset(0, 1, 1); run_maps();
	assert(render.rendered == 23 && !progress_calls && !done_calls);
	/* A fresh launch reads the menu cache with no development flag or render state. */
	reset(0, 0, 0);
	assert(map_pictures_cache_path(directory, sizeof(directory), test_root, "stock:a10"));
	assert(make_directories(directory));
	snprintf(path, sizeof(path), "%spicture.bmp", directory);
	assert(file_write(path, "cached picture", 14));
	assert(map_pictures_render_picture_read("a10", &data, &size));
	assert(size == 14 && !memcmp(data, "cached picture", 14));
	map_pictures_render_picture_free(data);
	assert(!map_pictures_render_picture_read("missing", &data, &size) && !data && !size);
	snprintf(saved.map_key, sizeof(saved.map_key), "stock:a10");
	assert(map_pictures_meta_write(metadata, sizeof(metadata), &saved));
	snprintf(path, sizeof(path), "%smeta.json", directory);
	assert(file_write(path, metadata, strlen(metadata)));
	reset(1, 0, 0); run_maps();
	assert(render.rendered == 22 && render.cached == 1 && completed == 23 && done_calls == 1);
	launch_refresh();
	return 0;
}
#endif
