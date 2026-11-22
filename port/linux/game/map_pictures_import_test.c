/* SPDX-License-Identifier: Apache-2.0 */
#ifdef MAP_PICTURES_IMPORT_TEST
/* Exercise the production state machine without the Xbox platform headers or game data. */
#define HALO_IOS
#define __HALO_LINUX_PLATFORM_H
#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
void platform_log(const char *format, ...);
#include "../src/map_pictures_render.c"

static int pending, development, available, menu = 1, game, awake, done_calls, identity_fails;
static int progress_calls, progress_done, identities, loaded_scenario, completed;
static double clock_ms;
static char test_root[1024];

void platform_log(const char *format, ...) { (void)format; }
int config_boolean(const char *name) { (void)name; return development; }
int host_photo_mode_available(void) { return available; }
int host_map_pictures_import_stage(void) { return pending; }
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
	assert(menu && !awake);
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
int map_pictures_scenario_identity(int index, char *key, size_t size, char hash[65], uint32_t *revision)
{
	const char *name = map_pictures_scenario_name(index), *base = strrchr(name, '\\') + 1;
	assert(index == (pending ? map_pictures_scenario_import_index(identities) : identities));
	identities++;
	if (identity_fails) return 0;
	snprintf(key, size, "stock:%s", base);
	memset(hash, 'a', 64); hash[64] = 0; *revision = 7;
	return 1;
}
int map_pictures_scenario_in_game(void) { return game; }
int map_pictures_scenario_in_cinematic(void) { return 0; }
long map_pictures_scenario_ticks(void) { return 0; }
void map_pictures_scenario_fast_forward(int on) { (void)on; }
int map_pictures_scenario_fast_forwarding(void) { return 0; }
void photo_mode_end(void) { }
void photo_mode_prepare_variant(const char *key) { (void)key; }
void photo_mode_clear_variant(void) { }
void dsound_set_capture_mute(int on) { (void)on; }
void photo_mode_capture_begin(void) { }
int photo_mode_capture_state(int *framed, int *revision) { *framed = 1; *revision = 7; return 1; }

static void reset(int import, int dev, int photo)
{
	memset(&render, 0, sizeof(render));
	pending = import; development = dev; available = photo; menu = 1; game = 0;
	done_calls = progress_calls = progress_done = identities = completed = identity_fails = 0;
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
		/* GPU readback is unavailable in this asset-free harness. */
		if (render.state == STATE_FRAME_PENDING)
		{
			render.wants_frame = 0;
			render.frame_done = render.frame_ok = 1;
		}
	}
	assert(render.state == STATE_DONE && !awake);
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

int main(int argc, char **argv)
{
	uint8_t *data; uint32_t size;
	char directory[1100], path[1200], metadata[MAP_PICTURES_META_SIZE];
	struct map_pictures_metadata saved;
	assert(argc == 2);
	snprintf(test_root, sizeof(test_root), "%s", argv[1]);
	ordering();
	reset(0, 1, 0); run_maps();
	assert(!identities && !progress_calls && !done_calls);
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
	return 0;
}
#endif
