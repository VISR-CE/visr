"""Which picture, in which shape, the server browser's detail panel gets (port/linux/game/custom_edition_maps.c).

Bug class: one texture shape for every consumer. The other menus show a map's picture as a 140x114 crop; the
browser's preview shows the whole 2:1 picture, so its alias tags get a texture of that shape, and a picture
stretched into a widget of another shape is distorted."""
import subprocess
from pathlib import Path

from probekit import cc
from test_network_discovery import function

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HALO_IOS 1
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define NUMBER_OF_SINGLE_PLAYER_LEVELS 10
#define FIRST_DISPLAY_INDEX 0x4000
#define FIRST_CAMPAIGN_DISPLAY_INDEX 0x3000
#define FIRST_CUSTOM_CAMPAIGN_DISPLAY_INDEX 0x6000
#define MENU_BROWSER_PICTURE_WIDTH 512
#define MENU_BROWSER_PICTURE_HEIGHT 256
#define csstrcasecmp strcasecmp
#define XBOX_POINTER(type, p) ((type *)(p))
#define _bitmap_format_a8r8g8b8 0
#define _error_silent 0
#include <strings.h>
void platform_log(char const *format, ...);
struct bitmap_data { int width, height; int hardware_format; void *base_address; };
struct bmp_file_picture { int unused; };
enum bmp_file_status { _bmp_file_status_ok };
struct stock_map { char const *file; };
struct custom_edition_map { boolean picture_read, wide_picture_read; struct bitmap_data *picture, *wide_picture; };

/* what the engine was asked to make: the texture's size and the shape the picture is fit to */
static int fit_shape_width, fit_shape_height, fit_width, fit_height, textures_made;
static struct bitmap_data store[16];
static struct bitmap_data *bitmap_2d_new(int w, int h, int a, int b)
{
	struct bitmap_data *bitmap = &store[textures_made++];
	(void)a; (void)b;
	bitmap->width = w; bitmap->height = h; bitmap->hardware_format = 1;
	bitmap->base_address = calloc((size_t)w * h, 4);
	return bitmap;
}
static void bitmap_rebuild(struct bitmap_data *b) { (void)b; }
static void bitmap_delete(struct bitmap_data *b) { (void)b; }
static enum bmp_file_status bmp_file_open(uint8_t const *f, uint32_t s, struct bmp_file_picture *p) { (void)f; (void)s; (void)p; return _bmp_file_status_ok; }
static char const *bmp_file_status_describe(enum bmp_file_status s) { (void)s; return ""; }
static void bmp_file_fit(uint8_t const *f, struct bmp_file_picture const *p, uint32_t sw, uint32_t sh, uint32_t w, uint32_t h, uint32_t *px)
{ (void)f; (void)p; (void)px; fit_shape_width = (int)sw; fit_shape_height = (int)sh; fit_width = (int)w; fit_height = (int)h; }
static void error(int c, char const *f, ...) { (void)c; (void)f; }

#define LEVEL_PICTURES_TAG_NAME "ui\\shell\\bitmaps\\mp_map_grafix"
#define CAMPAIGN_LEVEL_PICTURES_TAG_NAME "ui\\shell\\bitmaps\\sp_levels"
#define LEVEL_PICTURE_SHAPE_WIDTH 140
#define LEVEL_PICTURE_SHAPE_HEIGHT 114
#define UNKNOWN_LEVEL_FRAME 13
#define PICTURE_TEXTURE_SIZE 256
#define WIDE_PICTURE_TEXTURE_WIDTH MENU_BROWSER_PICTURE_WIDTH
#define WIDE_PICTURE_TEXTURE_HEIGHT MENU_BROWSER_PICTURE_HEIGHT
#define BROWSER_MAP_PICTURES_TAG_NAME "pc\\xbox\\browser\\preview_mp"
#define BROWSER_LEVEL_PICTURES_TAG_NAME "pc\\xbox\\browser\\preview_sp"

/* tags: the index is the alias' number */
static char const *tag_names[] = { "ui\\shell\\bitmaps\\mp_map_grafix", "pc\\xbox\\browser\\preview_mp",
	"ui\\shell\\bitmaps\\sp_levels", "pc\\xbox\\browser\\preview_sp" };
static char const *tag_get_name(long index) { return tag_names[index]; }
static char const *tag_name_strip_path(char const *n) { return n; }
static char const *main_get_solo_level_name(short level) { static char name[16]; snprintf(name, sizeof(name), "level%d", level); return name; }
/* a stock map (display index 3) with a picture; map "auto" has no curated framing but its automatic-camera picture is cached */
static struct stock_map const stock = { "bloodgulch" };
static struct stock_map const *stock_map_with_picture(short frame) { return frame == 3 ? &stock : NULL; }
static int cached_missing;
static int map_pictures_render_picture_read(char const *name, uint8_t **data, uint32_t *size)
{
	if (cached_missing) return 0;
	(void)name; *data = malloc(16); *size = 16; return 1;
}
static void map_pictures_render_picture_free(uint8_t *d) { free(d); }
static struct custom_edition_map custom_map;
static int custom_wide_asked = -1, custom_reads;
static struct bitmap_data custom_square, custom_wide;
static struct custom_edition_map *custom_edition_map_get(short index) { return index == FIRST_DISPLAY_INDEX ? &custom_map : NULL; }
static struct bitmap_data *custom_edition_map_picture_read(struct custom_edition_map const *m, boolean wide)
{ (void)m; custom_wide_asked = wide; custom_reads++; return wide ? &custom_wide : &custom_square; }
static short campaign_level_get(short display_index)
{ short level = display_index - FIRST_CAMPAIGN_DISPLAY_INDEX; return level >= 0 && level < NUMBER_OF_SINGLE_PLAYER_LEVELS ? level : NONE; }
static long tag_loaded(unsigned long group, char const *name) { (void)group; (void)name; return NONE; }
static struct bitmap_data *bitmap_group_get_bitmap_from_sequence(long t, short s, short f) { (void)t; (void)s; (void)f; return NULL; }
#define CAMPAIGN_LEVEL_PICTURES_TAG_NAME_UNUSED 0

/* PRODUCTION */

int main(void)
{
	short frame;
	struct bitmap_data *picture;

	/* a stock map's rendered picture for the browser's alias: whole, in the picture's own shape (the cache holds
	   a rendered picture for a map the automatic camera framed too: the browser cannot tell the framings apart) */
	frame = 3;
	picture = custom_edition_maps_picture(1, &frame);
	assert(picture && picture->width == 512 && picture->height == 256);
	assert(fit_shape_width == 512 && fit_shape_height == 256 && fit_width == 512 && fit_height == 256);
	assert(custom_edition_maps_picture_rendered(picture));
	/* the other menus' alias (the disc's own tag): the 140x114 crop in its square texture, unchanged */
	frame = 3;
	picture = custom_edition_maps_picture(0, &frame);
	assert(picture && picture->width == 256 && picture->height == 256);
	assert(fit_shape_width == 140 && fit_shape_height == 114);
	/* the same name is cached once for each shape, never one texture for both */
	{
		short a = 3, b = 3, c = 3;
		struct bitmap_data *wide = custom_edition_maps_picture(1, &a);
		struct bitmap_data *square = custom_edition_maps_picture(0, &b);
		assert(wide != square);
		assert(custom_edition_maps_picture(1, &c) == wide);
	}
	/* a campaign level through the browser's level alias: whole too */
	frame = 2;
	picture = custom_edition_maps_picture(3, &frame);
	assert(picture && picture->width == 512 && picture->height == 256);
	/* a map without a cached picture yet (the import pass has not reached it): none, the stock frame shows */
	cached_missing = 1;
	frame = 5;
	assert(custom_edition_maps_picture(1, &frame) == NULL);
	cached_missing = 0;
	/* a custom map's own bmp: the browser asks for its wide texture, the other menus for the square one */
	frame = FIRST_DISPLAY_INDEX;
	assert(custom_edition_maps_picture(1, &frame) == &custom_wide && custom_wide_asked == 1);
	frame = FIRST_DISPLAY_INDEX;
	assert(custom_edition_maps_picture(0, &frame) == &custom_square && custom_wide_asked == 0);
	/* each is read once */
	frame = FIRST_DISPLAY_INDEX; custom_edition_maps_picture(1, &frame);
	frame = FIRST_DISPLAY_INDEX; custom_edition_maps_picture(0, &frame);
	assert(custom_reads == 2);
	puts("browser picture selection passed");
	return 0;
}
'''


def test_the_browsers_picture_is_whole_and_2_to_1_and_the_other_menus_keep_theirs(tmp_path):
    source = (ROOT / 'port/linux/game/custom_edition_maps.c').read_text()
    signatures = (
        'static struct bitmap_data *custom_edition_picture_new(\n',
        'static struct bitmap_data *rendered_picture_get(\n',
        'static struct bitmap_data *rendered_picture_for_frame(\n',
        'boolean custom_edition_maps_picture_rendered(\n',
        'struct bitmap_data *custom_edition_maps_picture(\n',
    )
    pieces = []
    for signature in signatures:
        pieces.append(function(source, signature))
    # the rendered-picture cache (its storage lives beside its getter)
    start = source.index('#define RENDERED_PICTURES_MAXIMUM')
    cache = source[start:source.index('/* The stock map\'s cached picture', start)]
    program = HARNESS.replace('/* PRODUCTION */', pieces[0] + '\n' + cache + '\n' + '\n'.join(pieces[1:]))
    probe, binary = tmp_path / 'picture.c', tmp_path / 'picture'
    probe.write_text(program)
    cc.build([probe], output=binary, extra_warnings=True, relaxed=['unused-function', 'unused-parameter'], port_stubs=True)
    assert 'passed' in subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout
