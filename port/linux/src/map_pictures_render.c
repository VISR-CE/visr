/* SPDX-License-Identifier: Apache-2.0 */
/*
MAP_PICTURES_RENDER.C

Disc import or debug.map_pictures_render (development, AppKit Mac host): once the main menu is up, every
stock map whose cached picture is missing or stale is loaded in turn, framed by photo mode's scripted
capture (its saved framing, or the automatic spawn camera without one), and the rendered frame is
written to <cache>/map-pictures/<key>/ as picture.png (2048x1024), picture.bmp (the menus' copy) and
meta.json. Then the menu comes back and the map menus show the pictures
(custom_edition_maps_picture). A changed framing revision, map or renderer version renders that map again.
*/

#ifdef HALO_IOS

#include "map_pictures_render.h"
#include "platform.h"
#include "port_config.h"
#include "png_write.h"
#include "../game/map_pictures.h"
#include "../game/map_pictures_image.h"
#include "../game/map_pictures_scenario.h"
#include "../game/photo_mode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* port/runtime/guest/runtime/guest_host.h */
int host_photo_mode_available(void);
void host_map_pictures_keep_awake(int on);
/* port/linux/src/dsound_sdl.c: silences all output until switched off */
void dsound_set_capture_mute(int on);

/* what a render run holds while it loads maps: the screen awake and the sound off (every start and end of a run) */
static void run_hold(int on)
{
	host_map_pictures_keep_awake(on);
	dsound_set_capture_mute(on);
}
int host_map_pictures_import_stage(void);
void host_map_pictures_import_progress(int done, int total, const char *name);
void host_map_pictures_import_done(int rendered, int cached, int failed);
int host_map_pictures_begin_refresh(void);
void host_texture_cache_directory(char *text, unsigned long size);
/* port/linux/game/benchmark_scenario.c: an empty map is the main menu */
void benchmark_scenario_load(const char *map, const char *difficulty);
unsigned char benchmark_scenario_at_menu(void);
/* source/interface/ui_widget.c */
unsigned char filesystem_check_thread_is_active(void);
/* source/bink/bink_playback.c: a movie (the launch intro) holds the texture
cache's stolen memory until it stops, and a map load's precache takes that
memory too, so loading while one plays halts in xbox_texture_cache.c */
unsigned char bink_playback_in_progress(void);
/* source/cache/cache_files_windows.c: a map copy into the cache also holds it */
unsigned char cache_files_precache_in_progress(void);
/* d3d8_device.c: render lines, shadow size and anisotropy in place of the settings; 0 for the settings' own */
void halo_benchmark_overrides(long render_lines, long shadow_size, long anisotropy);
/* xbox_kernel.c: milliseconds of real time */
double halo_frame_trace_milliseconds(void);
int posix_make_directory(const char *path);
/* source/cache/cache_files_windows.c: texture and geometry reads waiting on the disk, and reads started */
short cache_file_requests_pending(void);
unsigned long cache_file_reads_started(void);
/* guest_gpu.c: pipelines still compiling on the worker threads */
unsigned int gpu_compiles_outstanding(void);
/* d3d8_device.c: pipelines the draws have built, counted */
uint32_t halo_pipelines_built(void);

#define PICTURE_WIDTH 2048
#define PICTURE_HEIGHT 1024
#define MENU_WIDTH 512
#define MENU_HEIGHT 256
/* a 16:9 frame at 1152 lines is 2048 wide, so the 2:1 picture is the frame's own pixels */
#define RENDER_LINES 1152
#define RENDER_SHADOWS 512
#define RENDERER_VERSION 10
#define AUTO_VERSION 3
/* the request's settle time, in game ticks (90 = 3 s); the capture itself does not wait in real time
before it: the clean-frame warm-up below covers streaming */
#define REQUEST_SETTLE_TICKS 90
#define LOAD_TIMEOUT_MILLISECONDS 60000.0
#define CAPTURE_TIMEOUT_MILLISECONDS 60000.0
#define FRAME_TIMEOUT_MILLISECONDS 10000.0
/* A picture's load skips the map's precache and shader-list warm, so the capture camera's
view loads its textures and compiles its pipelines as it draws. The readback waits for at
least FRAME_WAIT_MIN_UPDATES frames and then FRAME_WAIT_CLEAN_UPDATES frames in a row that
read nothing from the disk and compile nothing; at FRAME_WAIT_MAX_UPDATES it is taken anyway,
with a warning in the log. */
#define FRAME_WAIT_MIN_UPDATES 10
#define FRAME_WAIT_CLEAN_UPDATES 5
#define FRAME_WAIT_MAX_UPDATES 600
#define STAGE_COUNT 4
/* An opening cinematic is run fast, undrawn, for at most this much game time (5 minutes at
30 ticks a second); the real-time limit only guards a fast-forward that stalls */
#define CINEMATIC_TICK_LIMIT 9000L
#define CINEMATIC_TIMEOUT_MILLISECONDS 120000.0
/* a wait for the menu longer than this is logged with what holds it, and again each time it doubles */
#define MENU_WAIT_LOG_MILLISECONDS 10000.0
/* how long a finished run waits for the main menu before letting go of the sound and the screen anyway */
#define LEAVE_TIMEOUT_MILLISECONDS 30000.0

enum render_state
{
	STATE_START, STATE_WAIT_MENU, STATE_LOADING, STATE_SETTLE, STATE_CAPTURING, STATE_FRAME_WAIT,
	STATE_FRAME_PENDING, STATE_LEAVING, STATE_DONE,
	/* a launch with no import stage: waits for the main menu, then looks for a stale or missing picture */
	STATE_SCAN
};

static struct
{
	enum render_state state;
	/* when the run finished and began waiting for the menu (STATE_LEAVING) */
	double left;
	/* the current map's cached meta.json, parsed once by build_request for the freshness check */
	struct map_pictures_metadata cached_meta;
	int have_cached_meta;
	int enabled, import_mode, index, rendered, cached, failed, updates, fast_forwarding;
	/* 1 while a map loads straight from the last one, with no menu between them (the import stage) */
	int direct;
	long fast_forward_start;
	char root[1024], key[MAP_PICTURES_KEY_SIZE];
	struct map_pictures_request request;
	double run_start, map_start, loaded, settled, captured, warmed;
	/* the wait for the menu between maps: when it began, the updates it took, the next log */
	double menu_wait, menu_wait_log;
	int menu_wait_updates;
	/* main.c's map load stages: cache ready, tags loaded, setup done, shader warm done */
	double stages[STAGE_COUNT];
	double fast_forward_real_start, fast_forward_milliseconds;
	long fast_forward_ticks;
	unsigned long last_reads;
	uint32_t last_pipelines;
	int clean_run, warm_capped;
	volatile int wants_frame, frame_done, frame_ok;
	double frame_milliseconds;
} render;

static double now(void)
{
	return halo_frame_trace_milliseconds();
}

/* the cache's root: the parent of the host's texture-cache folder */
static int cache_root(char *root, size_t size)
{
	char *slash;

	host_texture_cache_directory(root, (unsigned long)size);
	slash = strrchr(root, '/');
	if (!root[0] || !slash || slash == root)
		return 0;
	*slash = 0;
	return 1;
}

/* every level of directory (with a trailing slash) */
static int make_directories(const char *directory)
{
	char path[1024];
	size_t i, length = strlen(directory);

	if (length >= sizeof(path))
		return 0;
	memcpy(path, directory, length + 1);
	for (i = 1; i < length; i++)
		if (path[i] == '/')
		{
			path[i] = 0;
			posix_make_directory(path);
			path[i] = '/';
		}
	posix_make_directory(path);
	return 1;
}

static uint8_t *file_read(const char *path, uint32_t *size)
{
	FILE *file = fopen(path, "rb");
	long length;
	uint8_t *data = NULL;

	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) > 0 && length < 64L * 1024 * 1024 &&
		fseek(file, 0, SEEK_SET) == 0 && (data = malloc((size_t)length + 1)) &&
		fread(data, 1, (size_t)length, file) == (size_t)length)
	{
		data[length] = 0;
		*size = (uint32_t)length;
	}
	else
	{
		free(data);
		data = NULL;
	}
	fclose(file);
	return data;
}

/* written whole to path.part, then renamed over path */
static int file_write(const char *path, const void *data, size_t size)
{
	char part[1100];
	FILE *file;
	int ok;

	snprintf(part, sizeof(part), "%s.part", path);
	if (!(file = fopen(part, "wb")))
		return 0;
	ok = fwrite(data, 1, size, file) == size;
	ok = fclose(file) == 0 && ok;
	if (ok && rename(part, path) != 0)
		ok = 0;
	if (!ok)
		remove(part);
	return ok;
}

static int png_file_write(const char *path, const uint8_t *bgra, uint32_t width, uint32_t height)
{
	char part[1100];

	snprintf(part, sizeof(part), "%s.part", path);
	if (!png_write_bgra(part, bgra, width, height, (unsigned long)width * 4))
	{
		remove(part);
		return 0;
	}
	if (rename(part, path) != 0)
	{
		remove(part);
		return 0;
	}
	return 1;
}

static int map_directory(char *directory, size_t size, const char *key)
{
	return render.root[0] && map_pictures_cache_path(directory, size, render.root, key);
}

static int attempt_matches(const char *directory, const struct map_pictures_request *request);

/* 1 when the map's cached meta.json (parsed by build_request) describes the picture this request would render.
A current picture whose meta.json holds another stamp (made before stamps, or the file was copied or restored) gets
the request's stamp written, so the next launch hashes nothing for it. */
static int cached_fresh(const char *directory, const struct map_pictures_request *request)
{
	char path[1100], text[MAP_PICTURES_META_SIZE];
	int fresh = 0;
	FILE *bmp;

	if (render.have_cached_meta && !map_pictures_is_stale(&render.cached_meta, request))
	{
		snprintf(path, sizeof(path), "%spicture.bmp", directory);
		if ((bmp = fopen(path, "rb")))
		{
			fresh = 1;
			fclose(bmp);
		}
	}
	if (fresh && request->metadata.map_stamp[0] && strcmp(render.cached_meta.map_stamp, request->metadata.map_stamp) &&
		map_pictures_meta_write(text, sizeof(text), &request->metadata) > 0)
	{
		snprintf(path, sizeof(path), "%smeta.json", directory);
		file_write(path, text, strlen(text));
	}
	return fresh || attempt_matches(directory, request);
}

/* A map whose picture could not be made as asked (the capture failed, the saved framing was refused) leaves a
record of the request it was tried for, so the launch scan treats it as current until the request changes
(a new framing, map or renderer) instead of bringing the stage up at every launch. */
static int attempt_text(char *text, size_t size, const struct map_pictures_request *request)
{
	const struct map_pictures_metadata *m = &request->metadata;

	return snprintf(text, size, "%lu:%lu:%lu:%s", (unsigned long)m->renderer_version,
		(unsigned long)m->framing_revision, (unsigned long)m->framing_hash, m->map_hash) < (int)size;
}

static void attempt_write(const struct map_pictures_request *request)
{
	char directory[1100], path[1200], text[160];

	if (map_directory(directory, sizeof(directory), render.key) && make_directories(directory) &&
		attempt_text(text, sizeof(text), request))
	{
		snprintf(path, sizeof(path), "%sattempt.txt", directory);
		file_write(path, text, strlen(text));
	}
}

static int attempt_matches(const char *directory, const struct map_pictures_request *request)
{
	char path[1200], text[160], saved[160];
	uint32_t size = 0;
	uint8_t *data;
	int match = 0;

	snprintf(path, sizeof(path), "%sattempt.txt", directory);
	if (!(data = file_read(path, &size)))
		return 0;
	if (size < sizeof(saved) && attempt_text(text, sizeof(text), request))
	{
		memcpy(saved, data, size);
		saved[size] = 0;
		match = !strcmp(saved, text);
	}
	free(data);
	return match;
}

/* waits for the menu before the next map */
static void wait_menu(void)
{
	render.menu_wait = now();
	render.menu_wait_log = MENU_WAIT_LOG_MILLISECONDS;
	render.menu_wait_updates = 0;
	render.state = STATE_WAIT_MENU;
}

static void finish(void)
{
	halo_benchmark_overrides(0, 0, 0);
	photo_mode_clear_variant();
	benchmark_scenario_load("", "");
	render.left = now();
	render.state = STATE_LEAVING;
}

/* the cached picture's metadata for a map directory: 1 when meta.json parses */
static int cached_meta(const char *directory, struct map_pictures_metadata *cached)
{
	char path[1200];
	uint32_t size = 0;
	uint8_t *json;
	int ok;

	snprintf(path, sizeof(path), "%smeta.json", directory);
	if (!(json = file_read(path, &size)))
		return 0;
	memset(cached, 0, sizeof(*cached));
	ok = map_pictures_meta_parse((const char *)json, size, cached);
	free(json);
	return ok;
}

/* the request that renders a map's picture as the current framings and renderer would make it: 1 with
render.key and render.request filled, 0 when the map has no key or hash (framing_revision is for the log).
The map's hash is the cached picture's while the map file still has the stamp it was taken under. */
static int build_request(int scenario, char directory[1100], uint32_t *revision)
{
	char hash[65], stamp[MAP_PICTURES_STAMP_SIZE];
	uint32_t framing_hash = 0;
	struct map_pictures_metadata *cached = &render.cached_meta, *meta = &render.request.metadata;
	int have_cache = 0;

	*revision = 0;
	directory[0] = 0;
	if (map_pictures_scenario_key(scenario, render.key, sizeof(render.key)) &&
		map_directory(directory, 1100, render.key))
		have_cache = cached_meta(directory, cached);
	render.have_cached_meta = have_cache;
	if (!map_pictures_scenario_identity(scenario, render.key, sizeof(render.key), hash, stamp,
		have_cache ? cached->map_stamp : "", have_cache ? cached->map_hash : "", revision, &framing_hash))
		return 0;
	memset(&render.request, 0, sizeof(render.request));
	snprintf(meta->map_key, sizeof(meta->map_key), "%s", render.key);
	memcpy(meta->map_hash, hash, sizeof(meta->map_hash));
	snprintf(meta->map_stamp, sizeof(meta->map_stamp), "%s", stamp);
	meta->framing = *revision ? MAP_PICTURES_CURATED : MAP_PICTURES_AUTO;
	meta->framing_revision = *revision;
	meta->framing_hash = *revision ? framing_hash : 0;
	meta->auto_version = *revision ? 0 : AUTO_VERSION;
	meta->renderer_version = RENDERER_VERSION;
	meta->texture_set = MAP_PICTURES_ORIGINAL;
	meta->width = PICTURE_WIDTH;
	meta->height = PICTURE_HEIGHT;
	meta->pre_game = 0;
	render.request.settle_ticks = REQUEST_SETTLE_TICKS;
	if (!map_directory(directory, 1100, render.key))
		directory[0] = 0;
	return 1;
}

/* loads the next map that needs a picture, or goes back to the menu after the last */
static void next_map(void)
{
	int count = map_pictures_scenario_count();

	for (; render.index < count; render.index++)
	{
		char directory[1100];
		uint32_t revision = 0;
		double checked = now();
		int scenario = render.import_mode ? map_pictures_scenario_import_index(render.index) : render.index;

		if (render.import_mode)
			host_map_pictures_import_progress(render.index, count, map_pictures_scenario_name(scenario));
		if (!build_request(scenario, directory, &revision))
		{
			platform_log("map pictures: %s: no key or map hash, skipped (%.1f s, framing revision %u)",
				map_pictures_scenario_name(scenario), (now() - checked) / 1000.0, revision);
			render.failed++;
			continue;
		}
		if (directory[0] && cached_fresh(directory, &render.request))
		{
			platform_log("map pictures: %s: picture is current, skipped (%.1f s, framing revision %u)",
				render.key, (now() - checked) / 1000.0, revision);
			render.cached++;
			continue;
		}
		platform_log("map pictures: Preparing maps, %d of %d: %s (%s)", render.index + 1, count, render.key,
			revision ? "saved framing" : "automatic camera");
		halo_benchmark_overrides(RENDER_LINES, RENDER_SHADOWS, 0);
		render.map_start = now();
		memset(render.stages, 0, sizeof(render.stages));
		render.fast_forward_milliseconds = 0.0;
		render.fast_forward_ticks = 0;
		platform_log("map pictures: %s: load requested at %.1f s (%s)", render.key, (render.map_start - render.run_start) / 1000.0,
			render.direct ? "straight from the last map" : "from the menu");
		photo_mode_prepare_variant(render.key);
		benchmark_scenario_load(map_pictures_scenario_name(scenario), "normal");
		render.state = STATE_LOADING;
		return;
	}
	finish();
}

/* logs how much game time the intro fast-forward covered; stops it if it still runs */
static void fast_forward_end(void)
{
	long ticks;

	if (!render.fast_forwarding) return;
	map_pictures_scenario_fast_forward(0);
	render.fast_forwarding = 0;
	ticks = map_pictures_scenario_ticks() - render.fast_forward_start;
	render.fast_forward_ticks = ticks;
	render.fast_forward_milliseconds = now() - render.fast_forward_real_start;
	platform_log("map pictures: %s: fast-forwarded %ld ticks (%.1f s game time)", render.key, ticks,
		(double)ticks / 30.0);
}

/* After a map: the import stage loads the next one straight from this one (main_set_map_name defers the change
to the main loop, which unloads this map and loads the next with no menu between them, no menu music fade and
no upscaler resume and pause); the debug render goes back to the menu first, as before. */
static void advance(void)
{
	render.index++;
	if (render.import_mode)
	{
		platform_log("map pictures: %s: next load requested at %.1f s", render.key, (now() - render.run_start) / 1000.0);
		render.direct = 1;
		next_map();
		return;
	}
	halo_benchmark_overrides(0, 0, 0);
	benchmark_scenario_load("", "");
	wait_menu();
}

/* this map failed; the next one follows (advance) */
static void skip_map(const char *why)
{
	fast_forward_end();
	platform_log("map pictures: %s: %s, skipped (%.1f s, framing revision %u)", render.key, why,
		(now() - render.map_start) / 1000.0, render.request.metadata.framing_revision);
	render.failed++;
	attempt_write(&render.request);
	photo_mode_end();
	render.wants_frame = 0;
	advance();
}

/* the run's setup once something enables it: the cache folder, the keep-awake and mute, the first menu wait */
static void start_run(double time)
{
	if (!cache_root(render.root, sizeof(render.root)))
	{
		platform_log("map pictures: no cache folder; nothing rendered");
		platform_log("map pictures: done in %.1f s: 0 rendered, 0 cached, %d failed",
			(now() - time) / 1000.0, map_pictures_scenario_count());
		render.enabled = 0;
		if (render.import_mode)
			host_map_pictures_import_done(0, 0, map_pictures_scenario_count());
		render.state = STATE_DONE;
		return;
	}
	render.run_start = time;
	run_hold(1);
	if (render.import_mode)
		host_map_pictures_import_progress(0, map_pictures_scenario_count(), "");
	wait_menu();
}

void map_pictures_render_update(void)
{
	double time = now();
	int framed = 0, revision = 0, capture;

	switch (render.state)
	{
	case STATE_START:
		render.import_mode = host_map_pictures_import_stage();
		render.enabled = (config_boolean("debug.map_pictures_render") && host_photo_mode_available()) || render.import_mode;
		if (!render.enabled)
		{
			/* no import stage and no development switch: after the menu is up, a launch with a stale or
			missing picture starts the stage itself (STATE_SCAN) */
			render.index = 0;
			/* a benchmark suite owns map loading for its whole run */
			render.state = (config_string("debug.benchmark_suite")[0] ||
				(getenv("HALO_RUNNER") && !config_boolean("debug.map_pictures_scan"))) ? STATE_DONE : STATE_SCAN;
			return;
		}
		start_run(time);
		return;
	case STATE_SCAN:
	{
		char directory[1100];
		uint32_t revision;

		/* never during play: a game that started before the scan ended leaves it for the next launch */
		if (map_pictures_scenario_in_game() || map_pictures_scenario_network_active())
		{
			render.state = STATE_DONE;
			return;
		}
		if (!benchmark_scenario_at_menu() || filesystem_check_thread_is_active() || bink_playback_in_progress() ||
			cache_files_precache_in_progress())
			return;
		if (render.index == 0 && !cache_root(render.root, sizeof(render.root)))
		{
			render.state = STATE_DONE;
			return;
		}
		if (render.index >= map_pictures_scenario_count())
		{
			render.state = STATE_DONE;
			return;
		}
		/* one map a frame: a map's hash is a fraction of a second the first time (map_share_map_hash) */
		if (!build_request(render.index++, directory, &revision) || (directory[0] && cached_fresh(directory, &render.request)))
			return;
		platform_log("map pictures: %s is missing or stale (framing revision %u, renderer %u): preparing maps",
			render.key, revision, RENDERER_VERSION);
		if (!host_map_pictures_begin_refresh())
		{
			render.state = STATE_DONE;
			return;
		}
		render.import_mode = host_map_pictures_import_stage();
		render.enabled = 1;
		render.index = 0;
		start_run(time);
		return;
	}
	case STATE_WAIT_MENU:
	{
		int menu = benchmark_scenario_at_menu(), check = filesystem_check_thread_is_active(),
			movie = bink_playback_in_progress(), precache = cache_files_precache_in_progress();
		double waited = time - render.menu_wait;

		render.menu_wait_updates++;
		/* the import stage plays no movie (guest_bink.c refuses them), so it never waits on one */
		if (render.import_mode)
			movie = 0;
		if (menu && !check && !movie && !precache)
		{
			if (waited > MENU_WAIT_LOG_MILLISECONDS)
				platform_log("map pictures: the menu took %.1f s (%d updates)", waited / 1000.0,
					render.menu_wait_updates);
			next_map();
		}
		else if (waited > render.menu_wait_log)
		{
			platform_log("map pictures: waiting %.1f s for the menu (%d updates): menu %d, disk check %d, "
				"movie %d, map copy %d", waited / 1000.0, render.menu_wait_updates, menu, check, movie, precache);
			render.menu_wait_log *= 2.0;
		}
		return;
	}
	case STATE_LOADING:
		/* from a map, the last map still reads as in game until the change runs: wait for main_new_map to
		finish this one (its last stage, after the shader warm) */
		if (map_pictures_scenario_in_game() && (!render.direct || render.stages[STAGE_COUNT - 1] != 0.0))
		{
			render.loaded = time;
			platform_log("map pictures: %s: in game at %.1f s (%.1f s after its load was requested)", render.key,
				(time - render.run_start) / 1000.0, (time - render.map_start) / 1000.0);
			render.state = STATE_SETTLE;
		}
		else if (time - render.map_start > LOAD_TIMEOUT_MILLISECONDS)
			skip_map("the map did not load in time");
		return;
	case STATE_SETTLE:
		/* A campaign map opens on a cutscene, and a capture taken during it shows the
		cutscene (space, fades, black). Every map takes one route past it: the game runs
		fast and undrawn until the tick the cinematic ends (map_pictures_scenario.c stops it
		there and pauses), never waiting in real time and never skipping, so each map is
		captured from the state its intro leaves. The capture starts at once: the readback's
		clean-frame warm-up waits for streaming. */
		if (map_pictures_scenario_in_cinematic() || render.fast_forwarding)
		{
			if (!render.fast_forwarding)
			{
				render.fast_forwarding = 1;
				render.fast_forward_start = map_pictures_scenario_ticks();
				render.fast_forward_real_start = time;
				map_pictures_scenario_fast_forward(1);
			}
			if (map_pictures_scenario_fast_forwarding())
			{
				if (map_pictures_scenario_ticks() - render.fast_forward_start > CINEMATIC_TICK_LIMIT)
				{
					skip_map("the opening cinematic ran past 5 minutes of game time");
					return;
				}
				if (time - render.loaded > CINEMATIC_TIMEOUT_MILLISECONDS)
				{
					skip_map("the opening cinematic did not end in time");
					return;
				}
				return;
			}
			/* (ended on the cinematic's last tick) */
		}
		fast_forward_end();
		render.settled = time;
		photo_mode_capture_begin();
		render.state = STATE_CAPTURING;
		return;
	case STATE_CAPTURING:
		capture = photo_mode_capture_state(&framed, &revision);
		if (capture < 0)
			skip_map("the capture failed");
		else if (capture > 0)
		{
			if (framed && (uint32_t)revision != render.request.metadata.framing_revision)
				platform_log("map pictures: %s: framing revision %d, expected %u", render.key, revision,
					render.request.metadata.framing_revision);
			if (!framed && render.request.metadata.framing == MAP_PICTURES_CURATED)
			{
				/* the saved framing was refused: what was drawn is the automatic camera's */
				render.request.metadata.framing = MAP_PICTURES_AUTO;
				attempt_write(&render.request); /* (before the request becomes the automatic one) */
				render.request.metadata.framing_revision = 0;
				render.request.metadata.framing_hash = 0;
				render.request.metadata.auto_version = AUTO_VERSION;
			}
			render.captured = time;
			platform_log("map pictures: %s: capture began at %.1f s", render.key, (time - render.run_start) / 1000.0);
			render.updates = 0;
			render.clean_run = 0;
			render.warm_capped = 0;
			render.last_reads = cache_file_reads_started();
			render.last_pipelines = halo_pipelines_built();
			render.state = STATE_FRAME_WAIT;
		}
		else if (time - render.settled > CAPTURE_TIMEOUT_MILLISECONDS)
			skip_map("the capture did not settle in time");
		return;
	case STATE_FRAME_WAIT:
	{
		unsigned long reads = cache_file_reads_started();
		uint32_t pipelines = halo_pipelines_built();
		int clean = reads == render.last_reads && pipelines == render.last_pipelines &&
			cache_file_requests_pending() == 0 && gpu_compiles_outstanding() == 0;

		render.last_reads = reads;
		render.last_pipelines = pipelines;
		render.clean_run = clean ? render.clean_run + 1 : 0;
		++render.updates;
		if (render.updates >= FRAME_WAIT_MAX_UPDATES && render.clean_run < FRAME_WAIT_CLEAN_UPDATES)
		{
			render.warm_capped = 1;
			platform_log("map pictures: %s: textures or pipelines still loading after %d frames; taking the picture anyway",
				render.key, render.updates);
		}
		if ((render.updates >= FRAME_WAIT_MIN_UPDATES && render.clean_run >= FRAME_WAIT_CLEAN_UPDATES) ||
			render.warm_capped)
		{
			render.warmed = time;
			render.frame_done = 0;
			render.frame_ok = 0;
			render.wants_frame = 1;
			render.state = STATE_FRAME_PENDING;
		}
		return;
	}
	case STATE_FRAME_PENDING:
		if (render.frame_done)
		{
			if (!render.frame_ok)
			{
				skip_map("the picture could not be written");
				return;
			}
			{
				/* stages main.c did not reach (a menu-only load) read as 0 */
				double cache = render.stages[0] ? render.stages[0] - render.map_start : 0.0;
				double tags = render.stages[1] && render.stages[0] ? render.stages[1] - render.stages[0] : 0.0;
				double setup = render.stages[2] && render.stages[1] ? render.stages[2] - render.stages[1] : 0.0;
				double warm = render.stages[3] && render.stages[2] ? render.stages[3] - render.stages[2] : 0.0;
				double first_frame = render.stages[3] ? render.loaded - render.stages[3] : 0.0;

				platform_log("map pictures: %s: to cache ready %.0f ms (precache skipped), tag load %.0f ms, "
					"setup %.0f ms, shader warm %.0f ms (skipped), to in game %.0f ms, load total %.0f ms; "
					"fast-forward %ld ticks in %.0f ms, settle %.0f ms, capture %.0f ms, "
					"warm-up %d frames in %.0f ms (%s), write %.0f ms, total %.0f ms",
					render.key, cache, tags, setup, warm, first_frame, render.loaded - render.map_start,
					render.fast_forward_ticks, render.fast_forward_milliseconds,
					render.settled - render.loaded - render.fast_forward_milliseconds,
					render.captured - render.settled, render.updates, render.warmed - render.captured,
					render.warm_capped ? "capped" : "clean", render.frame_milliseconds, time - render.map_start);
			}
			platform_log("map pictures: %s: rendered in %.1f s, framing revision %u", render.key,
				(time - render.map_start) / 1000.0, render.request.metadata.framing_revision);
			render.rendered++;
			photo_mode_end();
			advance();
		}
		else if (time - render.captured > FRAME_TIMEOUT_MILLISECONDS)
			skip_map("no rendered frame was read back");
		return;
	case STATE_LEAVING:
		/* a run that never gets back to the menu must not leave the game silent and the screen held awake */
		if (benchmark_scenario_at_menu() || time - render.left > LEAVE_TIMEOUT_MILLISECONDS)
		{
			if (!benchmark_scenario_at_menu())
				platform_log("map pictures: the menu did not come back within %.0f s; letting go of the sound and the screen",
					LEAVE_TIMEOUT_MILLISECONDS / 1000.0);
			platform_log("map pictures: done in %.1f s: %d rendered, %d cached, %d failed",
				(time - render.run_start) / 1000.0, render.rendered, render.cached, render.failed);
			run_hold(0);
			if (render.import_mode)
			{
				host_map_pictures_import_progress(map_pictures_scenario_count(), map_pictures_scenario_count(), "");
				host_map_pictures_import_done(render.rendered, render.cached, render.failed);
			}
			render.state = STATE_DONE;
		}
		return;
	case STATE_DONE:
		return;
	}
}

void map_pictures_render_stage(int stage)
{
	if (map_pictures_render_active() && stage >= 0 && stage < STAGE_COUNT)
		render.stages[stage] = now();
}

int map_pictures_render_active(void)
{
	return render.enabled && render.state >= STATE_LOADING && render.state <= STATE_FRAME_PENDING;
}

int map_pictures_render_wants_frame(void)
{
	return render.wants_frame;
}

void map_pictures_render_frame(const uint8_t *bgra, uint32_t width, uint32_t height, size_t stride)
{
	char directory[1100], path[1200], meta[MAP_PICTURES_META_SIZE];
	uint8_t *picture = NULL, *menu = NULL, *bmp = NULL;
	size_t bmp_size = map_pictures_bmp_size(MENU_WIDTH, MENU_HEIGHT), bmp_written;
	double start = now();
	int ok;

	if (!render.wants_frame)
		return;
	render.wants_frame = 0;
	picture = malloc((size_t)PICTURE_WIDTH * PICTURE_HEIGHT * 4);
	menu = malloc((size_t)MENU_WIDTH * MENU_HEIGHT * 4);
	bmp = bmp_size ? malloc(bmp_size) : NULL;
	ok = picture && menu && bmp && map_directory(directory, sizeof(directory), render.key) &&
		make_directories(directory) &&
		map_pictures_crop_resample(bgra, width, height, stride, picture, PICTURE_WIDTH, PICTURE_HEIGHT) &&
		map_pictures_crop_resample(bgra, width, height, stride, menu, MENU_WIDTH, MENU_HEIGHT) &&
		(bmp_written = map_pictures_bmp_encode(bmp, bmp_size, menu, MENU_WIDTH, MENU_HEIGHT)) != 0 &&
		map_pictures_meta_write(meta, sizeof(meta), &render.request.metadata) > 0;
	if (ok)
	{
		snprintf(path, sizeof(path), "%spicture.png", directory);
		ok = png_file_write(path, picture, PICTURE_WIDTH, PICTURE_HEIGHT);
	}
	if (ok)
	{
		snprintf(path, sizeof(path), "%spicture.bmp", directory);
		ok = file_write(path, bmp, bmp_written);
	}
	/* meta.json last: a picture without it is rendered again */
	if (ok)
	{
		snprintf(path, sizeof(path), "%smeta.json", directory);
		ok = file_write(path, meta, strlen(meta));
	}
	if (ok)
		platform_log("map pictures: %s: %ux%u frame to %spicture.png", render.key, width, height, directory);
	free(picture);
	free(menu);
	free(bmp);
	render.frame_milliseconds = now() - start;
	render.frame_ok = ok;
	render.frame_done = 1;
}

/* frees a picture from map_pictures_render_picture_read: it is the platform's allocation, so the game's own free
(the checked allocator in custom_edition_maps.c) must not take it */
void map_pictures_render_picture_free(uint8_t *data)
{
	free(data);
}

int map_pictures_render_picture_read(const char *base_name, uint8_t **data, uint32_t *size)
{
	char root[1024], key[MAP_PICTURES_KEY_SIZE], directory[1100], path[1200];

	*data = NULL;
	*size = 0;
	if (!base_name || !*base_name || !cache_root(root, sizeof(root)))
		return 0;
	snprintf(key, sizeof(key), "stock:%s", base_name);
	if (!map_pictures_cache_path(directory, sizeof(directory), root, key))
		return 0;
	snprintf(path, sizeof(path), "%spicture.bmp", directory);
	return (*data = file_read(path, size)) != NULL;
}

#endif
