/* The shipped menus, built by the real builder (port/linux/game/menu_tags.c over port/linux/src/menu_files.c) for one
platform set: tools/test_menu_builder_loads.py builds this with the game's headers stood in for by
port/ios/tests/menu_patch, embeds the shipped xbox menu files (generated into embedded.c) and runs it for every
platform set (the HALO_HOST_ variables). Bug class: a menu file the builder refuses ("menus: not added") takes every VISR
menu with it, and a mirror of the builder's rules in Python cannot see what the builder does not accept.

The ui.map the builder checks against is made of the tags the shipped files name: argv[2] lists the names of the
map's tags (one a line), and each is a tag of every group a widget can name. The patch table is left out (its
resolution against the real ui.map is tools/test_shipped_menu_patches.py's). A failure prints the builder's own
message, and the exit status is 1. argv[3]: the engine's event handler function names, one a line, in order. */

#include "menu_tags.c"
#include "menu_files.h"

#include <stdarg.h>
#include <sys/stat.h>

static int input_pad = 1, input_keyboard = 1;
int platform_menu_pad(void) { return input_pad; }
int platform_menu_keyboard(void) { return input_keyboard; }

static char const *language_name = "";
static char folder[256];
static long failures;
static char **function_names;
static long function_count;

/* ---------- what the game and the platform layer give (as menu_patch_probe.c has them) */
int host_development_hooks(void)
{
	return 0;
}

/* fake: prints the log lines to stdout where the test reads them */
void platform_log(char const *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	fputs("log: ", stdout);
	vprintf(format, arguments);
	fputc('\n', stdout);
	va_end(arguments);
}

/* fake: answers the debug.language the probe sets per case */
char const *config_string(char const *name)
{
	if (!strcmp(name, "debug.language"))
		return language_name;
	return "";
}

void config_folder(char *path, size_t size)
{
	snprintf(path, size, "%s", folder);
}

int platform_display_resolutions(long *widths, long *heights, int maximum)
{
	(void)widths, (void)heights, (void)maximum;
	return 0;
}

int platform_window_sizes(long *widths, long *heights, int maximum)
{
	(void)widths, (void)heights, (void)maximum;
	return 0;
}

void *global_network_game_server_get(void)
{
	return NULL;
}

unsigned int hud_hires_png_texture(const void *png, unsigned long size, unsigned long *levels)
{
	(void)levels;
	return size ? ((const unsigned char *)png)[0] : 0;
}

unsigned char *hud_hires_png_pixels(const void *png, unsigned long size, unsigned long *width, unsigned long *height)
{
	(void)png; (void)size; (void)width; (void)height;
	return NULL;
}

int posix_make_directory(const char *path)
{
	return mkdir(path, 0755);
}

void gpu_texture_destroy(unsigned int texture)
{
	(void)texture;
}

void display_error(short code, short player, boolean modal, boolean pause)
{
	(void)code; (void)player; (void)modal; (void)pause;
}

void display_error_abort_to_dashboard(short code, boolean optional)
{
	(void)code; (void)optional;
}

boolean pc_menu_profile_edit_begin(void)
{
	return TRUE;
}

boolean pc_menu_gametype_setup_active(void)
{
	return FALSE;
}

boolean pc_menu_gametype_edit_begin(void)
{
	return TRUE;
}

/* (the multiplayer mode: system link or not) */
static boolean probe_lan;

boolean pc_menu_multiplayer_lan(void)
{
	return probe_lan;
}

int host_sdl_display_max_fps(void)
{
	return 0;
}

static long textures_made;
static boolean reject_texture;

boolean rasterizer_bitmap_new(struct bitmap_data *bitmap)
{
	(void)bitmap;
	if (reject_texture)
		return FALSE;
	textures_made++;
	return TRUE;
}

void rasterizer_bitmap_delete(struct bitmap_data *bitmap)
{
	(void)bitmap;
}


char const *ui_widget_event_handler_function_name(long function_index)
{
	return function_index >= 0 && function_index < function_count ? function_names[function_index] : NULL;
}

/* ---------- the map's tag table */

#define MAP_SALT 0x1234
#define TABLE_CAPACITY 4096

static struct cache_file_tag_instance map_table[TABLE_CAPACITY];
static long map_count;
static struct cache_file_tag_instance *table;
static long table_count;

/* (the game's measure: here 8 for each character) */
short ui_widget_port_font_text_width(long font_index, void const *text)
{
	unsigned short const *characters = text;
	short width = 0;

	(void)font_index;
	while (characters && *characters++)
		width += 8;
	return width;
}

void *cache_files_tag_instances(long *count)
{
	*count = table_count;
	return table;
}

void cache_files_set_tag_instances(void *instances, long count)
{
	table = instances;
	table_count = count;
}

long tag_loaded(long group_tag, const char *name)
{
	long index;

	for (index = 0; index < table_count; index++)
	{
		if (table[index].group_tag == group_tag && table[index].name && !strcmp(table[index].name, name))
			return table[index].tag_index;
	}
	return NONE;
}

void *tag_get(long group_tag, long tag_index)
{
	(void)group_tag;
	return tag_index == NONE ? NULL : table[DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index)].base_address;
}

char *tag_get_name(long tag_index)
{
	/* (the game halts for a tag not in its table: a reference to a tag a patch has not written yet has no name) */
	if (table[DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index)].tag_index != tag_index)
	{
		printf("FAIL: the name of %lx, which is not in the table yet\n", (unsigned long)tag_index);
		failures++;
		return "";
	}
	return table[DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index)].name;
}

static long map_add(long group_tag, char const *name, void *base)
{
	long index = map_count++;

	map_table[index].group_tag = group_tag;
	map_table[index].parent_group_tags[0] = map_table[index].parent_group_tags[1] = NONE;
	map_table[index].tag_index = (MAP_SALT << 16) | index;
	map_table[index].name = (char *)name;
	map_table[index].base_address = base;
	table = map_table;
	table_count = map_count;
	return map_table[index].tag_index;
}

static struct ui_widget_definition *widget(short type, short left, short top, short right, short bottom)
{
	struct ui_widget_definition *definition = calloc(1, sizeof(*definition));

	definition->type = type;
	definition->bounds.x0 = left;
	definition->bounds.y0 = top;
	definition->bounds.x1 = right;
	definition->bounds.y1 = bottom;
	reference_clear(&definition->text_label_string_list, UNICODE_STRING_LIST_TAG);
	reference_clear(&definition->background_bitmap, BITMAP_GROUP_TAG);
	reference_clear(&definition->extended_description_widget, UI_WIDGET_DEFINITION_TAG);
	return definition;
}

static long widget_add(char const *name, short type, short left, short top, short right, short bottom)
{
	return map_add(UI_WIDGET_DEFINITION_TAG, name, widget(type, left, top, right, bottom));
}

static long strings_add(char const *name, long count)
{
	struct string_list *list = calloc(1, sizeof(*list));
	struct string_list_entry *entries = calloc(count, sizeof(*entries));
	long index;

	for (index = 0; index < count; index++)
	{
		unsigned short *text = calloc(16, sizeof(*text));
		char ascii[16];

		snprintf(ascii, sizeof(ascii), "map %ld", index);
		entries[index].string.size = halo_menus_utf16(ascii, text, 16) * 2;
		entries[index].string.address = text;
	}
	list->strings.count = count;
	list->strings.address = entries;
	return map_add(UNICODE_STRING_LIST_TAG, name, list);
}


static long bitmap_add(char const *name, long count)
{
	struct bitmap_group *group = calloc(1, sizeof(*group));
	struct bitmap_group_sequence *sequence = calloc(1, sizeof(*sequence));
	struct bitmap_data *bitmaps = calloc(count, sizeof(*bitmaps));
	long index;

	for (index = 0; index < count; index++)
	{
		bitmaps[index].width = (short)(300 + index);
		bitmaps[index].height = 240;
		bitmaps[index].pixels_offset = 1000 * (index + 1);
		bitmaps[index].cache_block_index = 7;
	}
	sequence->bitmap_count = (short)count;
	group->sequences.count = 1;
	group->sequences.address = sequence;
	group->bitmaps.count = count;
	group->bitmaps.address = bitmaps;
	return map_add(BITMAP_GROUP_TAG, name, group);
}


/* (pictures drawn from ui.map are not part of what loads: a frame that asks for one is given a stand-in) */
void *_texture_cache_bitmap_get_hardware_format(struct bitmap_data *bitmap, boolean block, boolean load)
{
	(void)bitmap; (void)block; (void)load;
	return NULL;
}
void halo_menus_picture_register(void const *hardware_format) { (void)hardware_format; }
void halo_menus_pictures_forget(void) {}
int halo_menus_art_needs_save(char const *cache_path) { (void)cache_path; return 0; }
int halo_menus_art_extract(void const *hardware_format, char const *cache_path)
{ (void)hardware_format; (void)cache_path; return 0; }

int main(int argc, char **argv)
{
	FILE *list;
	char line[512];

	if (argc < 4)
	{
		fprintf(stderr, "usage: menu_load_probe EMPTY_FOLDER NAMES FUNCTIONS\n");
		return 2;
	}
	snprintf(folder, sizeof(folder), "%s/", argv[1]);
	list = fopen(argv[3], "r");
	if (!list)
		return 2;
	while (fgets(line, sizeof(line), list))
	{
		line[strcspn(line, "\r\n")] = 0;
		function_names = realloc(function_names, (function_count + 1) * sizeof(*function_names));
		function_names[function_count++] = strdup(line);
	}
	fclose(list);
	/* ui.map's tags, one to a line, each in the one group it has: "W TYPE NAME" a widget (TYPE 0 container, 1 text
	box, 2 spinner list, 3 column list), "B" a bitmap, "S" a string list, "F" a font, "N" a sound */
	list = fopen(argv[2], "r");
	if (!list)
		return 2;
	while (fgets(line, sizeof(line), list))
	{
		char *name = line + 2;

		line[strcspn(line, "\r\n")] = 0;
		if (!*line)
			continue;
		switch (line[0])
		{
		case 'W':
			widget_add(strdup(name + 2), (short)(name[0] - '0'), 0, 0, 640, 480);
			break;
		case 'B':
			bitmap_add(strdup(name), 8);
			break;
		case 'S':
			strings_add(strdup(name), 64);
			break;
		case 'F':
			map_add(FONT_TAG, strdup(name), calloc(1, 1));
			break;
		case 'N':
			map_add(SOUND_TAG, strdup(name), calloc(1, 1));
			break;
		default:
			fprintf(stderr, "unknown tag line: %s\n", line);
			return 2;
		}
	}
	fclose(list);
	menu_tags_loaded("ui");
	if (!menu_tags.loaded)
	{
		printf("FAIL: the shipped menus did not load\n");
		return 1;
	}
	return failures != 0;
}
