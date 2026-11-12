/* SPDX-License-Identifier: Apache-2.0 */
/*
MAP_PICTURES_RENDER.C

debug.map_pictures_render (development, AppKit Mac host): at launch, once the main menu is up, every
stock map whose cached picture is missing or stale is loaded in turn, framed by photo mode's scripted
capture (its saved framing, or the automatic spawn camera without one), and the presented frame is
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

#define PICTURE_WIDTH 2048
#define PICTURE_HEIGHT 1024
#define MENU_WIDTH 512
#define MENU_HEIGHT 256
/* a 16:9 frame at 1152 lines is 2048 wide, so the 2:1 picture is the frame's own pixels */
#define RENDER_LINES 1152
#define RENDER_SHADOWS 512
#define RENDERER_VERSION 5
#define AUTO_VERSION 1
#define SETTLE_MILLISECONDS 3000.0
#define LOAD_TIMEOUT_MILLISECONDS 60000.0
#define CAPTURE_TIMEOUT_MILLISECONDS 60000.0
#define FRAME_TIMEOUT_MILLISECONDS 10000.0
/* frames drawn with the capture camera before the readback, so textures the new view
streams in have landed */
#define FRAME_WAIT_UPDATES 45
/* An opening cinematic is run fast, undrawn, for at most this much game time (5 minutes at
30 ticks a second); the real-time limit only guards a fast-forward that stalls */
#define CINEMATIC_TICK_LIMIT 9000L
#define CINEMATIC_TIMEOUT_MILLISECONDS 120000.0

enum render_state
{
	STATE_START, STATE_WAIT_MENU, STATE_LOADING, STATE_SETTLE, STATE_CAPTURING, STATE_FRAME_WAIT,
	STATE_FRAME_PENDING, STATE_LEAVING, STATE_DONE
};

static struct
{
	enum render_state state;
	int enabled, index, rendered, cached, failed, updates, fast_forwarding;
	long fast_forward_start;
	char root[1024], key[MAP_PICTURES_KEY_SIZE];
	struct map_pictures_request request;
	double run_start, map_start, loaded, quiet, settled, captured;
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

/* 1 when the map's cached meta.json describes the picture this request would render */
static int cached_fresh(const char *directory, const struct map_pictures_request *request)
{
	char path[1100];
	struct map_pictures_metadata cached;
	uint32_t size = 0;
	uint8_t *json;
	int fresh = 0;
	FILE *bmp;

	snprintf(path, sizeof(path), "%smeta.json", directory);
	if (!(json = file_read(path, &size)))
		return 0;
	memset(&cached, 0, sizeof(cached));
	if (map_pictures_meta_parse((const char *)json, size, &cached) && !map_pictures_is_stale(&cached, request))
	{
		snprintf(path, sizeof(path), "%spicture.bmp", directory);
		if ((bmp = fopen(path, "rb")))
		{
			fresh = 1;
			fclose(bmp);
		}
	}
	free(json);
	return fresh;
}

static void finish(void)
{
	halo_benchmark_overrides(0, 0, 0);
	benchmark_scenario_load("", "");
	render.state = STATE_LEAVING;
}

/* loads the next map that needs a picture, or goes back to the menu after the last */
static void next_map(void)
{
	int count = map_pictures_scenario_count();

	for (; render.index < count; render.index++)
	{
		char hash[65], directory[1100];
		uint32_t revision = 0;
		struct map_pictures_metadata *meta = &render.request.metadata;

		if (!map_pictures_scenario_identity(render.index, render.key, sizeof(render.key), hash, &revision))
		{
			platform_log("map pictures: %s: no key or map hash, skipped", map_pictures_scenario_name(render.index));
			render.failed++;
			continue;
		}
		memset(&render.request, 0, sizeof(render.request));
		snprintf(meta->map_key, sizeof(meta->map_key), "%s", render.key);
		memcpy(meta->map_hash, hash, sizeof(meta->map_hash));
		meta->framing = revision ? MAP_PICTURES_CURATED : MAP_PICTURES_AUTO;
		meta->framing_revision = revision;
		meta->auto_version = revision ? 0 : AUTO_VERSION;
		meta->renderer_version = RENDERER_VERSION;
		meta->texture_set = MAP_PICTURES_ORIGINAL;
		meta->width = PICTURE_WIDTH;
		meta->height = PICTURE_HEIGHT;
		meta->pre_game = 0;
		render.request.settle_ticks = (uint32_t)(SETTLE_MILLISECONDS * 30.0 / 1000.0);
		if (map_directory(directory, sizeof(directory), render.key) && cached_fresh(directory, &render.request))
		{
			render.cached++;
			continue;
		}
		platform_log("map pictures: Preparing maps, %d of %d: %s (%s)", render.index + 1, count, render.key,
			revision ? "saved framing" : "automatic camera");
		halo_benchmark_overrides(RENDER_LINES, RENDER_SHADOWS, 0);
		render.map_start = now();
		benchmark_scenario_load(map_pictures_scenario_name(render.index), "normal");
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
	platform_log("map pictures: %s: fast-forwarded %ld ticks (%.1f s game time)", render.key, ticks,
		(double)ticks / 30.0);
}

/* this map failed; the menu comes back before the next one */
static void skip_map(const char *why)
{
	fast_forward_end();
	platform_log("map pictures: %s: %s, skipped", render.key, why);
	render.failed++;
	photo_mode_end();
	render.wants_frame = 0;
	render.index++;
	halo_benchmark_overrides(0, 0, 0);
	benchmark_scenario_load("", "");
	render.state = STATE_WAIT_MENU;
}

void map_pictures_render_update(void)
{
	double time = now();
	int framed = 0, revision = 0, capture;

	switch (render.state)
	{
	case STATE_START:
		render.enabled = config_boolean("debug.map_pictures_render") && host_photo_mode_available();
		if (!render.enabled)
		{
			render.state = STATE_DONE;
			return;
		}
		if (!cache_root(render.root, sizeof(render.root)))
		{
			platform_log("map pictures: no cache folder; nothing rendered");
			render.enabled = 0;
			render.state = STATE_DONE;
			return;
		}
		render.run_start = time;
		render.state = STATE_WAIT_MENU;
		return;
	case STATE_WAIT_MENU:
		if (benchmark_scenario_at_menu() && !filesystem_check_thread_is_active() &&
			!bink_playback_in_progress() && !cache_files_precache_in_progress())
			next_map();
		return;
	case STATE_LOADING:
		if (map_pictures_scenario_in_game())
		{
			render.loaded = time;
			render.quiet = time;
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
		captured from the state its intro leaves. Then settle so streaming has caught up. */
		if (map_pictures_scenario_in_cinematic() || render.fast_forwarding)
		{
			if (!render.fast_forwarding)
			{
				render.fast_forwarding = 1;
				render.fast_forward_start = map_pictures_scenario_ticks();
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
				render.quiet = time;
				return;
			}
			/* (ended on the cinematic's last tick) */
		}
		fast_forward_end();
		if (time - render.quiet >= SETTLE_MILLISECONDS)
		{
			render.settled = time;
			photo_mode_capture_begin();
			render.state = STATE_CAPTURING;
		}
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
				render.request.metadata.framing_revision = 0;
				render.request.metadata.auto_version = AUTO_VERSION;
			}
			render.captured = time;
			render.updates = 0;
			render.state = STATE_FRAME_WAIT;
		}
		else if (time - render.settled > CAPTURE_TIMEOUT_MILLISECONDS)
			skip_map("the capture did not settle in time");
		return;
	case STATE_FRAME_WAIT:
		if (++render.updates >= FRAME_WAIT_UPDATES)
		{
			render.frame_done = 0;
			render.frame_ok = 0;
			render.wants_frame = 1;
			render.state = STATE_FRAME_PENDING;
		}
		return;
	case STATE_FRAME_PENDING:
		if (render.frame_done)
		{
			if (!render.frame_ok)
			{
				skip_map("the picture could not be written");
				return;
			}
			platform_log("map pictures: %s: load %.0f ms, settle %.0f ms, capture %.0f ms, write %.0f ms, total %.0f ms",
				render.key, render.loaded - render.map_start, render.settled - render.loaded,
				render.captured - render.settled, render.frame_milliseconds, time - render.map_start);
			render.rendered++;
			photo_mode_end();
			render.index++;
			halo_benchmark_overrides(0, 0, 0);
			benchmark_scenario_load("", "");
			render.state = STATE_WAIT_MENU;
		}
		else if (time - render.captured > FRAME_TIMEOUT_MILLISECONDS)
			skip_map("no frame was presented");
		return;
	case STATE_LEAVING:
		if (benchmark_scenario_at_menu())
		{
			platform_log("map pictures: done in %.1f s: %d rendered, %d cached, %d failed",
				(time - render.run_start) / 1000.0, render.rendered, render.cached, render.failed);
			render.state = STATE_DONE;
		}
		return;
	case STATE_DONE:
		return;
	}
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
	char key[MAP_PICTURES_KEY_SIZE], directory[1100], path[1200];

	*data = NULL;
	*size = 0;
	if (!render.enabled || render.state != STATE_DONE || !base_name || !*base_name)
		return 0;
	snprintf(key, sizeof(key), "stock:%s", base_name);
	if (!map_directory(directory, sizeof(directory), key))
		return 0;
	snprintf(path, sizeof(path), "%spicture.bmp", directory);
	return (*data = file_read(path, size)) != NULL;
}

#endif
