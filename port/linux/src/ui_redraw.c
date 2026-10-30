/*
UI_REDRAW.C

The menus' redraw (ui_redraw.h). While a screen other than the main menu
draws, the widgets' backgrounds that are the menus' frames are noted by their
textures' Data; the texture cache (xbox_textures.c) then asks for each, and
gets the picture rebuilt at the display's resolution (ui_redraw_rebuild.c)
from the player's own texels: the map's, or in a game map the picture the
menus saved from ui.map. Only frames (flat fills with bordered, rounded
edges): pictures, the legend's buttons among them, are drawn as they are.
*/

#include "ui_redraw.h"
#include "text_hires.h"
#include "platform.h"
#include "port_config.h"
#include "xgpu.h"
#include "gpu.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAXIMUM_TEXTURES 256
/* the most texels a rebuilt picture has (a 512x512 frame at 4x: 16 MB) */
#define MAXIMUM_REBUILT_TEXELS (2048UL * 2048UL)

/* d3d8_device.c: the display's pixels for each of the 480 lines */
float halo_screen_pixel_scale(void);

/* the frames redrawn, by the last part of their bitmap's tag name (the map's
"ui\shell\bitmaps\list_field" and the menus' "xbox/bitmaps/list_field" alike) */
static const char *const frames[] =
{
	"list_field",
	"list_item_bkd",
	"list_item_bkd_top",
	"alert_bkd",
	"option_bkds",
	"option_bkds_sm",
	"option_bkds_sm2",
	"sel_list_item_bkd",
	"sel_list_item_bkd_top",
	"sel_list_desc_bkd",
	"spinner_list_3_wide_item_background",
	"pregame_list_field",
	"pregame_field",
	"pregame_message_field",
	"pregame_message_field_split",
	"pregame_remote_field",
	"4way_profile_border",
	"join_game_border",
	"pausebox_left",
	"pausebox_center",
	"pausebox_right",
};

/* a frame noted: its texture (Data, and its format and size words: the
game's texture cache reuses Data for other bitmaps), its size in menu units,
and when it was last noted (a screen's count): the rebuilt texture stands
only for a frame a redrawn screen drew just now */
static struct
{
	unsigned long data, format_word, size_word;
	long width, height;
	unsigned long noted;
	unsigned int texture;
	int state;              /* 0: not yet built; 1: built; -1: drawn as it is */
} textures[MAXIMUM_TEXTURES];
static long texture_count;
static unsigned long generation = 1;
/* the screens drawn (ui_redraw_begin_screen) */
static unsigned long screens = 1;
/* a frame noted within this many screens is current (a frame draws once a
screen, with up to four players' screens a frame) */
#define NOTED_RECENTLY 8
static float built_scale;
static int screen_active;
static int last_enabled = -1;

static int names_equal(const char *a, const char *b)
{
	while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return !*a && !*b;
}

/* the last part of a tag name, after its last \ or / */
static const char *name_end(const char *name)
{
	const char *end = name;

	for (; *name; name++)
	{
		if (*name == '\\' || *name == '/')
			end = name + 1;
	}
	return end;
}

static int is_frame(const char *tag_name)
{
	const char *end = name_end(tag_name);
	size_t index;

	for (index = 0; index < sizeof(frames) / sizeof(frames[0]); index++)
	{
		if (names_equal(end, frames[index]))
			return 1;
	}
	return 0;
}

int ui_redraw_enabled(void)
{
	static int enabled;
	static unsigned long read_at = (unsigned long)-1;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		enabled = config_boolean("display.ui_redraw");
	}
	return enabled;
}

/* the rebuilt pictures' scale: the display's pixels per menu unit, at least
1.5 (below it the original is as sharp) and at most 4 */
static float wanted_scale(void)
{
	float pixels = halo_screen_pixel_scale();

	return pixels < 1.5f ? 1.5f : pixels > 4.0f ? 4.0f : pixels;
}

static void textures_release(void)
{
	long index;

	for (index = 0; index < texture_count; index++)
	{
		if (textures[index].texture)
			gpu_texture_destroy(textures[index].texture);
		textures[index].texture = 0;
		textures[index].state = 0;
	}
}

/* whether a screen is the main menu: a name ending in main_menu\main_menu
(ui.map's) or main_menu/main_menu (the menus' files) */
static int is_main_menu(const char *tag_name)
{
	size_t length = strlen(tag_name);
	const char *tail;

	if (length < 19)
		return 0;
	tail = tag_name + length - 19;
	if (length > 19 && tail[-1] != '\\' && tail[-1] != '/')
		return 0;
	return names_equal(tail, "main_menu\\main_menu") || names_equal(tail, "main_menu/main_menu");
}

void ui_redraw_begin_screen(const char *tag_name)
{
	int enabled = ui_redraw_enabled();

	if (enabled != last_enabled)
	{
		last_enabled = enabled;
		generation++;
	}
	screens++;
	if (enabled && fabsf(built_scale - wanted_scale()) > 0.01f)
	{
		/* (a window made larger, or an external display) */
		textures_release();
		built_scale = wanted_scale();
		generation++;
	}
	/* the main menu's screen keeps its look: ui.map's
	"ui\shell\main_menu\main_menu", the menus' "main_menu/main_menu" */
	screen_active = enabled && tag_name && !is_main_menu(tag_name);
}

void ui_redraw_end_screen(void)
{
	screen_active = 0;
}

int ui_redraw_active(void)
{
	return screen_active;
}

int ui_redraw_text_from_tags(void)
{
	return screen_active && !strcmp(config_string("display.ui_redraw_text"), "game");
}

void ui_redraw_note(const char *tag_name, const void *hardware_format, long width, long height)
{
	const unsigned long *resource = hardware_format;
	long index;

	if (!screen_active || !tag_name || !resource || !resource[1] || width <= 0 || height <= 0 || !is_frame(tag_name))
		return;
	for (index = 0; index < texture_count; index++)
	{
		if (textures[index].data == resource[1])
			break;
	}
	if (index == texture_count)
	{
		if (texture_count == MAXIMUM_TEXTURES)
			return;
		memset(&textures[index], 0, sizeof(textures[index]));
		texture_count++;
	}
	else if (textures[index].format_word != resource[3] || textures[index].size_word != resource[4] ||
		textures[index].width != width || textures[index].height != height)
	{
		/* (another bitmap where a frame was: built again) */
		if (textures[index].texture)
			gpu_texture_destroy(textures[index].texture);
		textures[index].texture = 0;
		textures[index].state = 0;
	}
	else if (screens - textures[index].noted <= NOTED_RECENTLY)
	{
		textures[index].noted = screens;
		return;
	}
	/* (new, changed or drawn again after a while: the texture cache asks
	again, its result for this texture perhaps the original's) */
	textures[index].data = resource[1];
	textures[index].format_word = resource[3];
	textures[index].size_word = resource[4];
	textures[index].width = width;
	textures[index].height = height;
	textures[index].noted = screens;
	generation++;
}

unsigned long ui_redraw_generation(void)
{
	return generation;
}

static long texture_find(unsigned long data)
{
	long index;

	for (index = 0; data && index < texture_count; index++)
	{
		if (textures[index].data == data)
			return index;
	}
	return -1;
}

int ui_redraw_texture(unsigned long data, unsigned long format_word, unsigned long size_word,
	unsigned int *texture, unsigned long *levels)
{
	long index;

	if (!ui_redraw_enabled())
		return 0;
	index = texture_find(data);
	if (index < 0 || textures[index].state < 0 || textures[index].format_word != format_word ||
		textures[index].size_word != size_word || screens - textures[index].noted > NOTED_RECENTLY)
		return 0;
	if (textures[index].state == 0)
		return 2;
	*texture = textures[index].texture;
	*levels = 1;
	return 1;
}

/* a picture made smaller by a whole factor, each texel the mean of its
square (premultiplied) */
static unsigned char *box_reduce(const unsigned char *pixels, unsigned long width, unsigned long height,
	unsigned long factor)
{
	unsigned long out_width = width / factor, out_height = height / factor, x, y, dx, dy, channel;
	unsigned char *out = malloc(out_width * out_height * 4);

	if (!out)
		return NULL;
	for (y = 0; y < out_height; y++)
	{
		for (x = 0; x < out_width; x++)
		{
			float sum[4] = { 0 };

			for (dy = 0; dy < factor; dy++)
			{
				for (dx = 0; dx < factor; dx++)
				{
					const unsigned char *texel = pixels + ((y * factor + dy) * width + x * factor + dx) * 4;
					float alpha = texel[3] / 255.0f;

					for (channel = 0; channel < 3; channel++)
						sum[channel] += texel[channel] * alpha;
					sum[3] += texel[3];
				}
			}
			for (channel = 0; channel < 3; channel++)
			{
				float alpha = sum[3] / 255.0f;

				out[(y * out_width + x) * 4 + channel] = (unsigned char)(alpha > 0.0f ? sum[channel] / alpha + 0.5f : 0.0f);
			}
			out[(y * out_width + x) * 4 + 3] = (unsigned char)(sum[3] / (factor * factor) + 0.5f);
		}
	}
	return out;
}

int ui_redraw_build(unsigned long data, const unsigned char *pixels, unsigned long width, unsigned long height,
	int hand_made)
{
	long index = texture_find(data);
	float scale = built_scale > 0.0f ? built_scale : wanted_scale();
	unsigned long out_width, out_height;
	unsigned char *reduced = NULL, *rebuilt;
	struct gpu_texture_description description = { 0 };

	if (index < 0)
		return 0;
	textures[index].state = -1;
	if (!pixels || !width || !height)
		return 0;
	if (width >= 2UL * (unsigned long)textures[index].width && height >= 2UL * (unsigned long)textures[index].height)
	{
		unsigned long factor = width / (unsigned long)textures[index].width;

		/* (a hand-made redraw is drawn as it is; a picture the menus saved
		from the upscaled texture goes back to the original's size, so that
		every frame is rebuilt the same way) */
		if (hand_made || factor * (unsigned long)textures[index].width != width ||
			factor * (unsigned long)textures[index].height != height)
			return 0;
		reduced = box_reduce(pixels, width, height, factor);
		if (!reduced)
			return 0;
		pixels = reduced;
		width /= factor;
		height /= factor;
	}
	out_width = (unsigned long)(width * scale + 0.5f);
	out_height = (unsigned long)(height * scale + 0.5f);
	rebuilt = out_width * out_height <= MAXIMUM_REBUILT_TEXELS ? malloc(out_width * out_height * 4) : NULL;
	/* (the rebuild does not care which of the color channels is which: BGRA
	in, BGRA out) */
	if (!rebuilt || !ui_redraw_rebuild(pixels, (long)width, (long)height, 4, (long)out_width, (long)out_height, rebuilt))
	{
		free(reduced);
		free(rebuilt);
		return 0;
	}
	free(reduced);
	/* (one level: drawn at the display's pixels, texel for pixel; mips would
	blend in a softer level) */
	description.type = GPU_TEXTURE_2D;
	description.format = GPU_FORMAT_BGRA8;
	description.usage = GPU_USAGE_UPLOAD;
	description.width = (uint32_t)out_width;
	description.height = (uint32_t)out_height;
	description.depth = 1;
	description.levels = 1;
	textures[index].texture = gpu_texture_create(&description);
	if (textures[index].texture)
	{
		gpu_texture_upload(textures[index].texture, 0, 0, rebuilt, (uint32_t)(out_width * out_height * 4));
		textures[index].state = 1;
		platform_log("ui redraw: a %lux%lu frame drawn at %lux%lu", width, height, out_width, out_height);
	}
	free(rebuilt);
	return textures[index].state == 1;
}

void ui_redraw_forget(void)
{
	textures_release();
	texture_count = 0;
	generation++;
	/* (the text's fonts of this map's font tags too) */
	text_hires_forget_tag_fonts();
}

long ui_redraw_touch_target_units(void)
{
	/* (the host says it is an iPhone, with its points' pixels: host_main.m) */
	const char *phone = getenv("HALO_HOST_PHONE_SCALE");
	float points_scale = phone ? (float)atof(phone) : 0.0f;
	float pixels = halo_screen_pixel_scale();
	long points = config_integer("input.ui_touch_target");

	if (points_scale <= 0.0f || pixels <= 0.0f || points <= 0)
		return 0;
	return (long)ceilf(points * points_scale / pixels);
}
