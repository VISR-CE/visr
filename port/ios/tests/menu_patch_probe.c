/* The menus' patch table (port/linux/game/menu_tags.c, read by port/linux/src/menu_files.c): built by
tools/test_menu_patches.py for the Mac it runs on, with the game's headers stood in for by port/ios/tests/menu_patch
(MENU_TAGS_PROBE), and run against a small ui.map of its own: the multiplayer type, profile edit and main menu
screens as the Xbox's ui.map has them (bounds, offsets, descriptions), and a list for taking a row out.

The patch file below is read by the real reader, the patches made by the real code, and the tag table, the lists'
rows, the screens' children, the descriptions' strings and frames and pc_menu_patch_list_index checked after it.
argv[1]: "apple" (a keyboard, no Apple TV), "tvos" (the Apple TV, no keyboard) or "bad" (a malformed patch, which
fails the files). */

static int input_pad = 1, input_keyboard = 1;
static char const *menu_mode = "xbox";
int platform_menu_pad(void) { return input_pad; }
int platform_menu_keyboard(void) { return input_keyboard; }

#include "menu_tags.c"
#include "menu_files.h"

#include <stdarg.h>
#include <sys/stat.h>

/* ---------- the patch file */

static char const patches_xml[] =
	"<menus>\n"
	/* the multiplayer type list: SYSTEM LINK replaced, INTERNET and DIRECT LINK after it */
	"<patch list=\"ui\\shell\\main_menu\\multiplayer_type_select\\multiplayer_type_select_list\"\n"
	"       screen=\"ui\\shell\\main_menu\\multiplayer_type_select\\multiplayer_type_select_screen\">\n"
	"  <replace child=\"ui\\shell\\main_menu\\multiplayer_type_select\\multiplayer_type_conn_item\"\n"
	"           name=\"multiplayer_type_join_lan\" text=\"SYSTEM LINK PLAY\" run=\"mp type set mode\"\n"
	"           open=\"ui\\shell\\main_menu\\multiplayer_type_select\\connected\\4way_start2join_screen\"/>\n"
	"  <row after=\"ui\\shell\\main_menu\\multiplayer_type_select\\multiplayer_type_conn_item\"\n"
	"       name=\"multiplayer_type_internet\" text=\"INTERNET\" run=\"mp type set mode\" open=\"xbox/test_screen\"\n"
	"       description=\"Browse and host games on the Internet.\" picture=\"3\"/>\n"
	"  <row after=\"multiplayer_type_internet\" name=\"multiplayer_type_join_direct\" text=\"DIRECT LINK\"\n"
	"       run=\"mp type set mode\" open=\"xbox/test_screen\" description=\"Join a game by its code.\"\n"
	"       picture=\"png:ce/port/blank__0.png\"/>\n"
	"</patch>\n"
	/* the profile edit list: four rows before SAVE CHANGES (KEYBOARD with a keyboard), DELETE PROFILE after it
	on the Apple TV, a row's pitch below it (SAVE CHANGES is further from the row above it, which would not fit) */
	"<patch list=\"ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\profile_edit_select_list\"\n"
	"       screen=\"ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\player_profile_edit_screen\">\n"
	"  <row before=\"ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\save_changes_profile_item\"\n"
	"       model=\"ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\color_profile_item\"\n"
	"       name=\"profile_item_video\" text=\"VIDEO\" open=\"xbox/test_screen\" description=\"Video.\" picture=\"1\"/>\n"
	"  <row after=\"profile_item_video\" name=\"profile_item_audio\" text=\"AUDIO\" open=\"xbox/test_screen\"\n"
	"       description=\"Audio.\" picture=\"2\"/>\n"
	"  <row after=\"profile_item_audio\" name=\"profile_item_network\" text=\"NETWORK\" open=\"xbox/test_screen\"\n"
	"       description=\"Network.\" picture=\"bitmap:xbox/test_picture\"/>\n"
	"  <row after=\"profile_item_network\" platform=\"keyboard\" name=\"profile_item_keyboard\" text=\"KEYBOARD\"\n"
	"       open=\"xbox/test_screen\" description=\"Keyboard.\" picture=\"4\"/>\n"
	"  <row after=\"ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\save_changes_profile_item\"\n"
	"       platform=\"tvos\" pitch=\"33\" name=\"profile_item_delete\" text=\"DELETE PROFILE\"\n"
	"       run=\"request del player profile\"\n"
	"       description=\"Delete.\" picture=\"0\"/>\n"
	"</patch>\n"
	/* Exercise insertion with a custom campaign row, never an exit action. */
	"<patch list=\"ui\\shell\\main_menu\\main_menu_select_list\" screen=\"ui\\shell\\main_menu\\main_menu\">\n"
	"  <row after=\"ui\\shell\\main_menu\\main_menu_item_game_demos\" model=\"ui\\shell\\main_menu\\main_menu_item_settings\"\n"
	"       name=\"main_menu_item_custom\" text=\"CUSTOM CAMPAIGN\" run=\"port custom campaign begin\"\n"
	"       created=\"mouse emit accept event\"/>\n"
	"</patch>\n"
	/* a row out of the settings list: those after it move up, keeping their descriptions */
	"<patch list=\"ui\\shell\\settings\\settings_list\" screen=\"ui\\shell\\settings\\settings_screen\">\n"
	"  <remove child=\"ui\\shell\\settings\\settings_item_1\"/>\n"
	"</patch>\n"
	/* a row that is one of our widgets as it is: no tags of its own, no description; one that names no widget, or
	a map's, is not made */
	"<patch list=\"ui\\shell\\adopt\\adopt_list\" screen=\"ui\\shell\\adopt\\adopt_screen\">\n"
	"  <row after=\"ui\\shell\\adopt\\adopt_item_1\" name=\"adopted\" widget=\"xbox/test_row\" help=\"Help A|Help B\"/>\n"
	"</patch>\n"
	"<patch list=\"ui\\shell\\adopt\\adopt_other_list\" screen=\"ui\\shell\\adopt\\adopt_screen\">\n"
	"  <row after=\"ui\\shell\\adopt\\adopt_other_item\" name=\"adopted_missing\" widget=\"xbox/no_such_screen\"/>\n"
	"</patch>\n"
	"<patch list=\"ui\\shell\\adopt\\adopt_map_list\" screen=\"ui\\shell\\adopt\\adopt_screen\">\n"
	"  <row after=\"ui\\shell\\adopt\\adopt_map_item\" name=\"adopted_map\" widget=\"ui\\shell\\settings\\settings_item_0\"/>\n"
	"</patch>\n"
	/* a row's own picture that there is none of: not made */
	"<patch list=\"ui\\shell\\adopt\\adopt_bitmap_list\" screen=\"ui\\shell\\adopt\\adopt_screen\">\n"
	"  <row after=\"ui\\shell\\adopt\\adopt_bitmap_item\" name=\"bitmapped\" text=\"B\" open=\"xbox/test_screen\"\n"
	"       bitmap=\"xbox/no_such_bitmap\"/>\n"
	"</patch>\n"
	/* too many rows for the screen: not made, its tags left empty */
	"<patch list=\"ui\\shell\\overflow\\overflow_list\" screen=\"ui\\shell\\overflow\\overflow_screen\">\n"
	"  <row after=\"ui\\shell\\overflow\\overflow_item_0\" name=\"overflow_a\" text=\"A\" open=\"xbox/test_screen\"/>\n"
	"</patch>\n"
	/* a list the map has not got: not made, nothing used */
	"<patch list=\"ui\\shell\\missing\\missing_list\" screen=\"ui\\shell\\missing\\missing_screen\">\n"
	"  <row after=\"ui\\shell\\missing\\missing_item\" name=\"missing_row\" text=\"MISSING\" open=\"xbox/test_screen\"/>\n"
	"</patch>\n"
	/* a function there is none of: not made, its tags not taken */
	"<patch list=\"ui\\shell\\settings\\other_list\" screen=\"ui\\shell\\settings\\settings_screen\">\n"
	"  <row after=\"ui\\shell\\settings\\other_item_0\" name=\"other_row\" text=\"OTHER\" run=\"no such function\"/>\n"
	"</patch>\n"
	"</menus>\n";

static char const bad_xml[] =
	"<menus><patch list=\"ui\\shell\\a\" screen=\"ui\\shell\\b\"><row name=\"r\" text=\"R\" open=\"x\"/></patch></menus>\n";

static char const screen_xml[] =
	"<menus><bitmap name=\"xbox/test_picture\"><frame map=\"ui\\shell\\main_menu\\multiplayer_type_select\\mp_options\" index=\"2\" width=\"460\" height=\"230\"/></bitmap>"
	"<bitmap name=\"xbox/test_cached\"><frame map=\"ui\\shell\\main_menu\\multiplayer_type_select\\mp_options\" index=\"0\" cache=\"cache/xbox/v1/extracted.png\" png=\"xbox/art/shipped.png\" width=\"512\" height=\"256\"/>"
	"<frame map=\"ui\\shell\\main_menu\\multiplayer_type_select\\mp_options\" index=\"1\" cache=\"cache/xbox/v1/present.png\" png=\"xbox/art/shipped.png\" width=\"512\" height=\"256\"/>"
	"<frame map=\"ui\\shell\\main_menu\\multiplayer_type_select\\mp_options\" index=\"2\" cache=\"cache/xbox/v1/upgrade.png\" png=\"xbox/art/shipped.png\" width=\"512\" height=\"256\"/></bitmap>"
	"<widget name=\"xbox/test_screen\"/><widget name=\"xbox/test_row\" width=\"512\" height=\"28\"/>"
	"<widget name=\"xbox/main_menu/multiplayer_type_select/lobby/lobby_screen\"/></menus>\n";

static char const pc_xml[] =
	"<menus><widget name=\"main_menu/main_menu\"/>"
	"<strings name=\"main_menu/settings_select/player_setup/player_profile_edit/profile_edit_descriptions\">"
	"<string text=\"Rename this profile.\"/></strings></menus>";

/* (embedded as tools/embed_assets.py has them; the malformed patch is a file of the same name in the menus folder,
which replaces it) */
/* (a picture's "files": the first byte is the texture the stand-in PNG decoder gives, 0 being one that cannot be
drawn) */
static const char shipped_png[] = "S";
const struct menu_file_embedded menu_files_embedded[] =
{
	{ "xbox/patches.xml", (const unsigned int *)patches_xml, sizeof(patches_xml) - 1 },
	{ "xbox/test.xml", (const unsigned int *)screen_xml, sizeof(screen_xml) - 1 },
	{ "ce/probe.xml", (const unsigned int *)pc_xml, sizeof(pc_xml) - 1 },
	{ "xbox/art/shipped.png", (const unsigned int *)shipped_png, 1 },
};
const unsigned int menu_files_embedded_count = 4;

/* ---------- what the game and the platform layer give */

static char mode[16];
static char folder[256];
static long failures;

void platform_log(char const *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	fputs("log: ", stdout);
	vprintf(format, arguments);
	fputc('\n', stdout);
	va_end(arguments);
}

char const *config_string(char const *name)
{
	if (!strcmp(name, "display.menus"))
		return menu_mode;
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

int posix_make_directory(const char *path)
{
	return mkdir(path, 0755);
}

void gpu_texture_destroy(unsigned int texture)
{
	(void)texture;
}

boolean pc_menu_profile_edit_begin(void)
{
	return TRUE;
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
	static char const *const names[] = { "NULL", "request del player profile", "mp game player quit" };

	return function_index >= 0 && function_index < (long)NUMBEROF(names) ? names[function_index] : NULL;
}

/* ---------- the map's tag table */

#define MAP_SALT 0x1234
#define TABLE_CAPACITY 256

static struct cache_file_tag_instance map_table[TABLE_CAPACITY];
static long map_count;
static struct cache_file_tag_instance *table;
static long table_count;

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

static struct ui_widget_definition *definition_of(long tag)
{
	return tag_get(UI_WIDGET_DEFINITION_TAG, tag);
}

/* the widget's children: tags and offsets */
static void children_set(long parent, long count, long const *tags, short const *xs, short const *ys)
{
	struct ui_widget_child_reference *children = calloc(count, sizeof(*children));
	long child;

	for (child = 0; child < count; child++)
	{
		char const *leaf = strrchr(tag_get_name(tags[child]), '\\') + 1;

		reference_set(&children[child].widget_tag, UI_WIDGET_DEFINITION_TAG, tags[child]);
		strncpy(children[child].name, leaf, sizeof(children[child].name) - 1);
		children[child].horizontal_offset = xs[child];
		children[child].vertical_offset = ys[child];
	}
	definition_of(parent)->child_widgets.count = count;
	definition_of(parent)->child_widgets.address = children;
}

/* a row: A and Start opening a screen and running a function, and a game
data input */
static long row_add(char const *name, short left, short top, short right, short bottom, short function)
{
	long tag = widget_add(name, _widget_type_text_box, left, top, right, bottom);
	struct ui_widget_definition *definition = definition_of(tag);
	struct ui_widget_event_handler_reference *handlers = calloc(3, sizeof(*handlers));
	struct ui_widget_game_data_input_reference *inputs = calloc(1, sizeof(*inputs));
	long handler;

	for (handler = 0; handler < 3; handler++)
	{
		handlers[handler].event_type = (short)(handler == 0 ? 24 : handler == 1 ? 0 : 12);
		handlers[handler].flags = FLAG(_event_handler_run_function_bit);
		handlers[handler].function = function;
		reference_clear(&handlers[handler].widget_tag, UI_WIDGET_DEFINITION_TAG);
		reference_clear(&handlers[handler].sound_effect, SOUND_TAG);
	}
	definition->event_handlers.count = 3;
	definition->event_handlers.address = handlers;
	inputs[0].function = 40;
	definition->game_data_inputs.count = 1;
	definition->game_data_inputs.address = inputs;
	return tag;
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

/* a picture of count frames, one sequence of them all, each its own size */
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

/* a description panel: a picture and a text, in that order or the other */
static long panel_add(char const *name, char const *picture_name, char const *text_name, char const *strings_name,
	char const *bitmap_name, long string_count, long frame_count, boolean text_first)
{
	long panel = widget_add(name, _widget_type_container, 275, 78, 595, 400);
	long picture = widget_add(picture_name, _widget_type_container, 288, 82, 595, 326);
	long text = widget_add(text_name, _widget_type_text_box, 290, 285, 580, 390);
	long parts[2];
	short const zero[2] = { 0, 0 };

	reference_set(&definition_of(picture)->background_bitmap, BITMAP_GROUP_TAG, bitmap_add(bitmap_name, frame_count));
	reference_set(&definition_of(text)->text_label_string_list, UNICODE_STRING_LIST_TAG,
		strings_add(strings_name, string_count));
	parts[0] = text_first ? text : picture;
	parts[1] = text_first ? picture : text;
	children_set(panel, 2, parts, zero, zero);
	return panel;
}

#define MP "ui\\shell\\main_menu\\multiplayer_type_select\\"
#define PE "ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\"
#define MM "ui\\shell\\main_menu\\"

static long mp_screen, mp_list, mp_blueline, mp_key, pe_screen, pe_list, pe_blueline, mm_screen, mm_list;
static long st_screen, st_list, ov_list, other_list;

static void adopt_build(void);

static void map_build(void)
{
	adopt_build();
	/* the multiplayer type screen (bounds and offsets as the Xbox's ui.map has them) */
	{
		long rows[4], children[4];
		short const xs[4] = { 0, 0, 0, 0 }, ys[4] = { -33, -33, -33, -11 };
		short const screen_xs[4] = { 0, 0, 0, 64 }, screen_ys[4] = { 0, 0, 0, 186 };

		widget_add(MP "connected\\4way_start2join_screen", _widget_type_container, 0, 0, 640, 480);
		rows[0] = row_add(MP "multiplayer_type_coop_item", 51, 111, 283, 143, 22);
		rows[1] = row_add(MP "multiplayer_type_split_item", 51, 144, 283, 176, 0);
		rows[2] = row_add(MP "multiplayer_type_conn_item", 51, 177, 283, 209, 91);
		rows[3] = row_add(MP "multiplayer_type_gametypes_item", 51, 210, 283, 242, 0);
		mp_list = widget_add(MP "multiplayer_type_select_list", _widget_type_column_list, 0, 0, 640, 480);
		children_set(mp_list, 4, rows, xs, ys);
		reference_set(&definition_of(mp_list)->extended_description_widget, UI_WIDGET_DEFINITION_TAG,
			panel_add(MP "multiplayer_type_list_ext_desc", MP "multiplayer_options_pic", MP "multiplayer_options_txt",
				MP "multiplayer_option_descriptions", MP "mp_options", 4, 4, FALSE));
		children[0] = widget_add(MP "header_multiplayer", _widget_type_container, 35, 11, 298, 70);
		children[1] = mp_list;
		children[2] = mp_key = widget_add(MM "button_key", _widget_type_container, 371, 414, 577, 440);
		children[3] = mp_blueline = widget_add(MM "blueline", _widget_type_container, 0, 0, 192, 2);
		mp_screen = widget_add(MP "multiplayer_type_select_screen", _widget_type_container, 0, 0, 640, 480);
		children_set(mp_screen, 4, children, screen_xs, screen_ys);
	}
	/* the profile edit screen */
	{
		long rows[5], children[8];
		short const xs[5] = { 0, 0, 0, 0, 0 }, ys[5] = { 0, 0, 0, 0, 22 };
		short const screen_xs[8] = { 0, 368, 0, 432, 346, 471, 492, 64 };
		short const screen_ys[8] = { 0, 414, 0, 353, 414, 414, 414, 220 };

		rows[0] = row_add(PE "name_profile_item", 51, 78, 283, 110, 66);
		rows[1] = row_add(PE "controller_setup_profile_item", 51, 111, 283, 143, 0);
		rows[2] = row_add(PE "advanced_controls_profile_item", 51, 144, 283, 176, 0);
		rows[3] = row_add(PE "color_profile_item", 51, 177, 283, 210, 0);
		rows[4] = row_add(PE "save_changes_profile_item", 51, 211, 283, 244, 67);
		pe_list = widget_add(PE "profile_edit_select_list", _widget_type_column_list, 0, 0, 640, 480);
		children_set(pe_list, 5, rows, xs, ys);
		/* (more strings than frames: the added ones are numbered past both) */
		reference_set(&definition_of(pe_list)->extended_description_widget, UI_WIDGET_DEFINITION_TAG,
			panel_add(PE "profile_edit_extended_description", PE "profile_edit_extended_desc_pic",
				PE "profile_edit_extended_desc_text", PE "profile_edit_descriptions", PE "profile_options", 6, 5, TRUE));
		children[0] = widget_add(PE "header_profile_settings", _widget_type_container, 35, 11, 490, 70);
		children[1] = widget_add(MM "=cancel", _widget_type_text_box, 0, 0, 100, 22);
		children[2] = pe_list;
		children[3] = widget_add(PE "profile_label", _widget_type_text_box, 0, 0, 400, 22);
		children[4] = widget_add(MM "b_butn", _widget_type_container, 0, 0, 21, 26);
		children[5] = widget_add(MM "a_butn", _widget_type_container, 0, 0, 21, 26);
		children[6] = widget_add(MM "=accept", _widget_type_text_box, 0, 0, 95, 22);
		children[7] = pe_blueline = mp_blueline;
		pe_screen = widget_add(PE "player_profile_edit_screen", _widget_type_container, 0, 0, 640, 480);
		children_set(pe_screen, 8, children, screen_xs, screen_ys);
	}
	/* the main menu */
	{
		long rows[4], children[3];
		short const xs[4] = { 194, 192, 192, 192 }, ys[4] = { 247, 283, 319, 355 };
		short const screen_xs[3] = { 0, 0, 450 }, screen_ys[3] = { 28, 0, 400 };

		rows[0] = row_add(MM "main_menu_item_load_camp", 0, 0, 256, 33, 101);
		rows[1] = row_add(MM "main_menu_item_multiplayer", 0, 0, 256, 33, 0);
		rows[2] = row_add(MM "main_menu_item_settings", 0, 0, 256, 33, 0);
		rows[3] = row_add(MM "main_menu_item_game_demos", 0, 0, 256, 33, 87);
		mm_list = widget_add(MM "main_menu_select_list", _widget_type_column_list, 0, 0, 640, 480);
		children_set(mm_list, 4, rows, xs, ys);
		children[0] = widget_add(MM "halo_logo", _widget_type_container, 0, 0, 640, 256);
		children[1] = mm_list;
		children[2] = widget_add(MM "build_number", _widget_type_text_box, 0, 0, 150, 20);
		mm_screen = widget_add(MM "main_menu", _widget_type_container, 0, 0, 640, 480);
		children_set(mm_screen, 3, children, screen_xs, screen_ys);
	}
	/* a settings screen of four rows 30 apart and a line below them; a second list on it */
	{
		long rows[4], others[1], children[3];
		short const xs[4] = { 0, 0, 0, 0 }, ys[4] = { 73, 103, 133, 163 };
		short const screen_xs[3] = { 0, 40, 0 }, screen_ys[3] = { 0, 200, 0 };

		rows[0] = row_add("ui\\shell\\settings\\settings_item_0", 54, 0, 300, 28, 0);
		rows[1] = row_add("ui\\shell\\settings\\settings_item_1", 54, 0, 300, 28, 0);
		rows[2] = row_add("ui\\shell\\settings\\settings_item_2", 54, 0, 300, 28, 0);
		rows[3] = row_add("ui\\shell\\settings\\settings_item_3", 54, 0, 300, 28, 0);
		st_list = widget_add("ui\\shell\\settings\\settings_list", _widget_type_column_list, 0, 0, 640, 480);
		children_set(st_list, 4, rows, xs, ys);
		others[0] = row_add("ui\\shell\\settings\\other_item_0", 54, 300, 300, 328, 0);
		other_list = widget_add("ui\\shell\\settings\\other_list", _widget_type_column_list, 0, 0, 640, 480);
		children_set(other_list, 1, others, xs, ys);
		children[0] = st_list;
		children[1] = widget_add("ui\\shell\\settings\\settings_line", _widget_type_container, 0, 0, 192, 2);
		children[2] = other_list;
		st_screen = widget_add("ui\\shell\\settings\\settings_screen", _widget_type_container, 0, 0, 640, 480);
		children_set(st_screen, 3, children, screen_xs, screen_ys);
	}
	/* a list at the bottom of its screen, with no room for more rows */
	{
		long rows[2], children[2];
		short const xs[2] = { 0, 0 }, ys[2] = { 330, 370 };
		short const screen_xs[2] = { 0, 300 }, screen_ys[2] = { 0, 414 };

		rows[0] = row_add("ui\\shell\\overflow\\overflow_item_0", 51, 0, 283, 33, 0);
		rows[1] = row_add("ui\\shell\\overflow\\overflow_item_1", 51, 0, 283, 33, 0);
		ov_list = widget_add("ui\\shell\\overflow\\overflow_list", _widget_type_column_list, 0, 0, 640, 480);
		children_set(ov_list, 2, rows, xs, ys);
		children[0] = ov_list;
		long legend = widget_add("ui\\shell\\overflow\\a_butn", _widget_type_container, 0, 7, 21, 33);
		short const legend_x[1] = { 0 }, legend_y[1] = { 7 };

		children[1] = widget_add("ui\\shell\\overflow\\button_key_options", _widget_type_container, 0, 0, 300, 26);
		children_set(children[1], 1, &legend, legend_x, legend_y);
		children_set(widget_add("ui\\shell\\overflow\\overflow_screen", _widget_type_container, 0, 0, 640, 480),
			2, children, screen_xs, screen_ys);
	}
}

static long ad_screen, ad_list, ad_other_list, ad_map_list, ad_bitmap_list, ad_text;

/* three lists on one screen, for the rows that are our widgets as they are */
static void adopt_build(void)
{
	long rows[2], others[1], maps[1], bitmaps[1], children[4];
	short const xs[2] = { 0, 0 }, ys[2] = { 73, 103 }, zero[1] = { 0 }, screen_xs[4] = { 0, 0, 0, 0 };
	short const screen_ys[4] = { 0, 0, 0, 0 };

	rows[0] = row_add("ui\\shell\\adopt\\adopt_item_0", 54, 0, 300, 28, 0);
	rows[1] = row_add("ui\\shell\\adopt\\adopt_item_1", 54, 0, 300, 28, 0);
	ad_list = widget_add("ui\\shell\\adopt\\adopt_list", _widget_type_column_list, 0, 0, 640, 480);
	children_set(ad_list, 2, rows, xs, ys);
	/* (a description of one text, whose strings are the rows' values' in turn) */
	ad_text = widget_add("ui\\shell\\adopt\\adopt_help", _widget_type_text_box, 68, 321, 550, 400);
	reference_set(&definition_of(ad_text)->text_label_string_list, UNICODE_STRING_LIST_TAG,
		strings_add("ui\\shell\\adopt\\adopt_help_strings", 4));
	reference_set(&definition_of(ad_list)->extended_description_widget, UI_WIDGET_DEFINITION_TAG, ad_text);
	others[0] = row_add("ui\\shell\\adopt\\adopt_other_item", 54, 0, 300, 28, 0);
	ad_other_list = widget_add("ui\\shell\\adopt\\adopt_other_list", _widget_type_column_list, 0, 0, 640, 480);
	children_set(ad_other_list, 1, others, zero, zero);
	maps[0] = row_add("ui\\shell\\adopt\\adopt_map_item", 54, 0, 300, 28, 0);
	ad_map_list = widget_add("ui\\shell\\adopt\\adopt_map_list", _widget_type_column_list, 0, 0, 640, 480);
	children_set(ad_map_list, 1, maps, zero, zero);
	bitmaps[0] = row_add("ui\\shell\\adopt\\adopt_bitmap_item", 54, 0, 300, 28, 0);
	ad_bitmap_list = widget_add("ui\\shell\\adopt\\adopt_bitmap_list", _widget_type_column_list, 0, 0, 640, 480);
	children_set(ad_bitmap_list, 1, bitmaps, zero, zero);
	children[0] = ad_list;
	children[1] = ad_other_list;
	children[2] = ad_map_list;
	children[3] = ad_bitmap_list;
	ad_screen = widget_add("ui\\shell\\adopt\\adopt_screen", _widget_type_container, 0, 0, 640, 480);
	children_set(ad_screen, 4, children, screen_xs, screen_ys);
}

/* ---------- the checks */

static void check(boolean ok, char const *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	fputs(ok ? "ok: " : "FAIL: ", stdout);
	vprintf(format, arguments);
	fputc('\n', stdout);
	va_end(arguments);
	if (!ok)
		failures++;
}

static struct ui_widget_child_reference *children_of(long tag)
{
	return definition_of(tag)->child_widgets.address;
}

static long child_count(long tag)
{
	return definition_of(tag)->child_widgets.count;
}

/* the rows' tops on the screen (the list at offset 0) */
static void check_rows(char const *what, long list, short const *tops, long count)
{
	long row;

	check(child_count(list) == count, "%s has %ld rows (%ld)", what, count, child_count(list));
	for (row = 0; row < count && row < child_count(list); row++)
	{
		struct ui_widget_child_reference const *reference = &children_of(list)[row];
		short top = (short)(definition_of(reference->widget_tag.index)->bounds.y0 + reference->vertical_offset);

		check(top == tops[row], "%s row %ld (%s) at %d (%d)", what, row, reference->name, tops[row], top);
	}
}

static short child_top(long screen, long child)
{
	struct ui_widget_child_reference const *reference = &children_of(screen)[child];

	return (short)(definition_of(reference->widget_tag.index)->bounds.y0 + reference->vertical_offset);
}

static short description_of(long list, long row)
{
	short description;

	pc_menu_patch_list_index(list, children_of(list)[row].widget_tag.index, &description);
	return description;
}

static void check_descriptions(char const *what, long list, short const *expected)
{
	long row;
	short description;

	check(pc_menu_patch_list_index(list, NONE, &description), "%s is a patched list", what);
	for (row = 0; row < child_count(list); row++)
	{
		check(description_of(list, row) == expected[row], "%s row %ld (%s) shows description %d (%d)", what, row,
			children_of(list)[row].name, expected[row], description_of(list, row));
	}
}

static boolean string_is(struct string_list const *list, long index, char const *text)
{
	unsigned short expected[256];
	long length = halo_menus_utf16(text, expected, 256);
	struct string_list_entry const *entry = (struct string_list_entry const *)list->strings.address + index;

	return index < list->strings.count && entry->string.size == length * 2 &&
		!memcmp(entry->string.address, expected, length * 2);
}

static struct ui_widget_definition const *row_definition(long list, long row)
{
	return definition_of(children_of(list)[row].widget_tag.index);
}

static void check_row_copy(long list, long row, char const *name, char const *text, long run, long open)
{
	struct ui_widget_definition const *definition = row_definition(list, row);
	struct ui_widget_event_handler_reference const *handlers = definition->event_handlers.address;
	struct string_list const *strings = tag_get(UNICODE_STRING_LIST_TAG, definition->text_label_string_list.index);
	long handler;
	boolean handlers_ok = definition->event_handlers.count >= 2;

	check(!strcmp(definition->name, name), "row %s is named so (%s)", name, definition->name);
	check(!strcmp(children_of(list)[row].name, name), "row %s's reference is named so", name);
	check(pc_menu_tag(children_of(list)[row].widget_tag.index), "row %s is one of our tags", name);
	check(strings && string_is(strings, 0, text) && definition->string_list_index == 0, "row %s reads %s", name, text);
	check(definition->game_data_inputs.count == 0, "row %s has no game data inputs", name);
	for (handler = 0; handlers_ok && handler < 2; handler++)
	{
		handlers_ok = handlers[handler].event_type == (handler ? 12 : 0) &&
			(run == NONE ? !TEST_FLAG(handlers[handler].flags, _event_handler_run_function_bit) :
				TEST_FLAG(handlers[handler].flags, _event_handler_run_function_bit) && handlers[handler].function == run) &&
			(open == NONE ? !TEST_FLAG(handlers[handler].flags, _event_handler_open_widget_bit) :
				TEST_FLAG(handlers[handler].flags, _event_handler_open_widget_bit) &&
					handlers[handler].widget_tag.index == open);
	}
	check(handlers_ok, "row %s has A and Start, running %ld and opening %lx", name, run, (unsigned long)open);
}

static void check_apple(boolean tvos)
{
	long test_screen = tag_loaded(UI_WIDGET_DEFINITION_TAG, "pc\\xbox\\test_screen");
	long set_mode = PC_MENU_FUNCTION_BASE + name_index("mp type set mode", port_function_names,
		NUMBEROF(port_function_names));
	long counted, used, index, empty = 0;

	check(test_screen != NONE, "our screen is in the table");
	/* the multiplayer type list: SYSTEM LINK (replaced), INTERNET, DIRECT LINK, then EDIT GAMETYPES 66 lower */
	{
		short const tops[6] = { 78, 111, 144, 177, 210, 265 };
		short const descriptions[6] = { 0, 1, 2, 4, 5, 3 };
		struct ui_widget_definition const *text = definition_of(children_of(
			definition_of(mp_list)->extended_description_widget.index)[1].widget_tag.index);
		struct ui_widget_definition const *picture = definition_of(children_of(
			definition_of(mp_list)->extended_description_widget.index)[0].widget_tag.index);
		struct string_list const *strings = tag_get(UNICODE_STRING_LIST_TAG, text->text_label_string_list.index);
		struct bitmap_group const *group = bitmap_group_get(picture->background_bitmap.index);
		struct bitmap_group_sequence const *sequence = group->sequences.address;
		struct bitmap_data const *frames = group->bitmaps.address;

		check_rows("multiplayer type", mp_list, tops, 6);
		check_row_copy(mp_list, 2, "multiplayer_type_join_lan", "SYSTEM LINK PLAY", set_mode,
			tag_loaded(UI_WIDGET_DEFINITION_TAG, MP "connected\\4way_start2join_screen"));
		check_row_copy(mp_list, 3, "multiplayer_type_internet", "INTERNET", set_mode, test_screen);
		check_row_copy(mp_list, 4, "multiplayer_type_join_direct", "DIRECT LINK", set_mode, test_screen);
		check(row_definition(mp_list, 3)->event_handlers.count == 2, "a row has only A and Start");
		check(TEST_FLAG(((struct ui_widget_event_handler_reference const *)row_definition(mp_list, 3)->
			event_handlers.address)->flags, _event_handler_try_to_branch_on_failure_bit),
			"a row that runs and opens opens only when its function succeeds");
		check(!strcmp(children_of(mp_list)[5].name, "multiplayer_type_gametypes_item"), "EDIT GAMETYPES is the map's");
		check_descriptions("multiplayer type", mp_list, descriptions);
		check(child_top(mp_screen, 3) == 252, "the line below the rows moved to 252 (%d)", child_top(mp_screen, 3));
		check(child_top(mp_screen, 2) == 414, "the legend stayed at 414 (%d)", child_top(mp_screen, 2));
		check(child_top(mp_screen, 0) == 11, "the header stayed (%d)", child_top(mp_screen, 0));
		check(strings->strings.count == 6 && string_is(strings, 3, "map 3") &&
			string_is(strings, 4, "Browse and host games on the Internet.") &&
			string_is(strings, 5, "Join a game by its code."), "the descriptions are added after the map's four");
		check(group->bitmaps.count == 6 && sequence->bitmap_count == 6 && sequence->first_bitmap_index == 0,
			"the picture has six frames in its sequence (%ld, %d)", group->bitmaps.count, sequence->bitmap_count);
		check(frames[4].pixels_offset == frames[3].pixels_offset && frames[4].width == frames[3].width &&
			frames[4].cache_block_index == NONE, "INTERNET's picture is a copy of frame 3");
		check(frames[5].width == frames[0].width && frames[5].height == frames[0].height &&
			frames[5].tag_index == picture->background_bitmap.index && frames[5].signature == BITMAP_GROUP_TAG,
			"DIRECT LINK's picture is a PNG of ours at the first frame's size");
		check(frames[0].cache_block_index == NONE && frames[3].pixels_offset == 4000,
			"the map's frames are kept, to be loaded from their new place");
	}
	/* the profile edit list: three or four rows before SAVE CHANGES, DELETE PROFILE after it on the Apple TV */
	{
		short const tops_keyboard[9] = { 78, 111, 144, 177, 210, 243, 276, 309, 365 };
		short const tops_tvos[9] = { 78, 111, 144, 177, 210, 243, 276, 332, 365 };
		short const descriptions_keyboard[9] = { 0, 1, 2, 3, 6, 7, 8, 9, 4 };
		short const descriptions_tvos[9] = { 0, 1, 2, 3, 6, 7, 8, 4, 9 };

		check_rows("profile edit", pe_list, tvos ? tops_tvos : tops_keyboard, 9);
		check_row_copy(pe_list, 4, "profile_item_video", "VIDEO", NONE, test_screen);
		check(!strcmp(children_of(pe_list)[tvos ? 7 : 8].name, "save_changes_profile_item"), "SAVE CHANGES is the map's");
		if (tvos)
		{
			check_row_copy(pe_list, 8, "profile_item_delete", "DELETE PROFILE",
				function_lookup("request del player profile"), NONE);
		}
		check_descriptions("profile edit", pe_list, tvos ? descriptions_tvos : descriptions_keyboard);
		{
			struct ui_widget_definition const *panel = definition_of(definition_of(pe_list)->extended_description_widget.index);
			struct ui_widget_child_reference const *parts = panel->child_widgets.address;
			struct bitmap_group const *group = bitmap_group_get(definition_of(parts[1].widget_tag.index)->background_bitmap.index);
			struct bitmap_data const *frames = group->bitmaps.address;

			check(frames[8].pixels_offset == 3000 && frames[8].width == 302 &&
				frames[8].cache_block_index == NONE && frames[8].base_address == NULL,
				"NETWORK borrows the other list's bitmap and resets its texture cache");
			{
				short x, y, width, height;

				check(pc_menu_frame_placement(&frames[8], &x, &y, &width, &height) &&
					width == 460 && height == 230,
					"NETWORK keeps the borrowed frame's display size");
			}
		}
		check(child_top(pe_screen, 7) == (tvos ? 319 : 352), "the line moved down with the rows (%d)",
			child_top(pe_screen, 7));
		check(child_top(pe_screen, 3) == 353, "the profile's name stayed (right of the list, %d)", child_top(pe_screen, 3));
		check(child_top(pe_screen, 5) == 414 && child_top(pe_screen, 4) == 414 && child_top(pe_screen, 1) == 414,
			"the legend stayed at 414");
	}
	/* the main menu: CUSTOM CAMPAIGN, a row below GAME DEMOS, its own created handler; the build number stays */
	{
		short const tops[5] = { 247, 283, 319, 355, 391 };
		struct ui_widget_definition const *custom = row_definition(mm_list, 4);
		struct ui_widget_event_handler_reference const *handlers = custom->event_handlers.address;
		long custom_function = PC_MENU_FUNCTION_BASE + name_index("port custom campaign begin", port_function_names,
			NUMBEROF(port_function_names));

		check_rows("main menu", mm_list, tops, 5);
		check(children_of(mm_list)[4].horizontal_offset == 192, "CUSTOM CAMPAIGN is in line with GAME DEMOS (%d)",
			children_of(mm_list)[4].horizontal_offset);
		check_row_copy(mm_list, 4, "main_menu_item_custom", "CUSTOM CAMPAIGN", custom_function, NONE);
		check(custom->event_handlers.count == 3 && handlers[2].event_type == 24 &&
			handlers[2].function == PC_MENU_FUNCTION_BASE + name_index("mouse emit accept event", port_function_names,
				NUMBEROF(port_function_names)), "CUSTOM CAMPAIGN has its own created handler, not GAME DEMOS'");
		check(child_top(mm_screen, 2) == 400 && child_top(mm_screen, 0) == 28, "the logo and build number stayed");
	}
	/* a row taken out: those after it a row higher, with their own descriptions */
	{
		short const tops[3] = { 73, 103, 133 };
		short const descriptions[3] = { 0, 2, 3 };

		check_rows("settings", st_list, tops, 3);
		check_descriptions("settings", st_list, descriptions);
		check(child_top(st_screen, 1) == 170, "the line below moved up a row (%d)", child_top(st_screen, 1));
	}
	/* not made: the list that does not fit, the one the map has not got, the function there is none of */
	{
		short const tops[2] = { 330, 370 };
		short description;

		check_rows("overflow", ov_list, tops, 2);
		check(!pc_menu_patch_list_index(ov_list, NONE, &description), "the overflowing list is not patched");
		check(!pc_menu_patch_list_index(other_list, NONE, &description) && child_count(other_list) == 1,
			"the list whose row runs no function is not patched");
		check(!pc_menu_patch_list_index(mm_screen, NONE, &description), "a screen is not a patched list");
	}
	/* a row that is one of our widgets as it is: the widget itself, in its place, without a description; the ones
	that name no widget or a map's not made */
	{
		short const tops[3] = { 73, 103, 133 };
		short description = 0;
		long test_row = tag_loaded(UI_WIDGET_DEFINITION_TAG, "pc\\xbox\\test_row");

		check_rows("adopt", ad_list, tops, 3);
		if (child_count(ad_list) < 3)
			return;
		check(children_of(ad_list)[2].widget_tag.index == test_row, "the adopted row is our widget, not a copy");
		check(!strcmp(children_of(ad_list)[2].name, "adopted"), "the adopted row is named so");
		check(pc_menu_patch_list_index(ad_list, children_of(ad_list)[2].widget_tag.index, &description) &&
			description == NONE, "the adopted row has no description (%d)", description);
		{
			struct string_list const *help = tag_get(UNICODE_STRING_LIST_TAG,
				definition_of(ad_text)->text_label_string_list.index);

			check(help->strings.count == 6 && string_is(help, 3, "map 3") && string_is(help, 4, "Help A") &&
				string_is(help, 5, "Help B"), "the adopted row's help strings follow the description's own (%ld)",
				help->strings.count);
		}
		check(!pc_menu_patch_list_index(ad_other_list, NONE, &description) && child_count(ad_other_list) == 1,
			"a row of a widget there is none of is not made");
		check(!pc_menu_patch_list_index(ad_map_list, NONE, &description) && child_count(ad_map_list) == 1,
			"a row of a map's widget is not made");
		check(!pc_menu_patch_list_index(ad_bitmap_list, NONE, &description) && child_count(ad_bitmap_list) == 1,
			"a row with a picture there is none of is not made");
	}
	/* the screen the engine opens after a network game: the Xbox's, but for Internet games the Xbox look's lobby */
	{
		char const *const pregame = "ui\\shell\\main_menu\\multiplayer_type_select\\connected\\pregame\\connected_pregame_screen";
		char const *const postgame = "ui\\shell\\main_menu\\multiplayer_type_select\\connected\\connected_map_select_postgame_wrapper";

		probe_lan = FALSE;
		check(!strcmp(pc_menus_screen(pregame), "pc\\xbox\\main_menu\\multiplayer_type_select\\lobby\\lobby_screen"),
			"an Internet game's pregame is the Xbox look's lobby (%s)", pc_menus_screen(pregame));
		check(pc_menus_screen(postgame) == postgame, "the postgame map select stays the Xbox's");
		probe_lan = TRUE;
		check(pc_menus_screen(pregame) == pregame, "a system link game's pregame stays the Xbox's");
		check(pc_menus_screen(postgame) == postgame, "the postgame map select stays the Xbox's for system link");
		probe_lan = FALSE;
		{
			char const *const pc_pregame = "pc\\main_menu\\multiplayer_type_select\\lobby\\lobby_screen";
			char const *const pc_postgame = "pc\\main_menu\\multiplayer_type_select\\connected\\connected_map_select_wrapper";
			char const *const xbox_pregame = "pc\\xbox\\main_menu\\multiplayer_type_select\\lobby\\lobby_screen";
			struct
			{
				char const *mode;
				int pad, lan;
				char const *pregame, *postgame;
			} const cases[] =
			{
				{ "auto", 0, FALSE, pc_pregame, pc_postgame },
				{ "auto", 0, TRUE, pc_pregame, pc_postgame },
				{ "auto", 1, FALSE, xbox_pregame, postgame },
				{ "auto", 1, TRUE, pregame, postgame },
				{ "pc", 1, FALSE, pc_pregame, pc_postgame },
				{ "pc", 1, TRUE, pc_pregame, pc_postgame },
				{ "xbox", 0, FALSE, xbox_pregame, postgame },
				{ "xbox", 0, TRUE, pregame, postgame },
			};
			long index;
			for (index = 0; index < NUMBEROF(cases); index++)
			{
				menu_mode = cases[index].mode;
				input_pad = cases[index].pad;
				probe_lan = cases[index].lan;
				/* A newly loaded map or the previous screen may hold the other look. */
				menu_tags.xbox_set = !input_pad;
				check(!strcmp(pc_menus_screen(pregame), cases[index].pregame),
					"%s pregame follows current input or override (pad %d, LAN %d)", menu_mode, input_pad, probe_lan);
				check(!strcmp(pc_menus_screen(postgame), cases[index].postgame),
					"%s postgame follows current input or override (pad %d, LAN %d)", menu_mode, input_pad, probe_lan);
			}
			menu_mode = "xbox";
			input_pad = 1;
			probe_lan = FALSE;
			menu_tags.xbox_set = TRUE;
		}
	}
	/* the tags: 2 for each row made or tried, the table trimmed to them, the tried ones empty */
	/* Four widgets, two bitmaps and the PC description list precede patch tags. */
	counted = 6 + 2 * (3 + (tvos ? 4 : 4) + 1 + 1 + 1 + 1) + 1;
	used = 6 + 2 * (3 + 4 + 1 + 1) + 1;
	check(table_count == map_count + used, "the table has the map's %ld tags and %ld of ours (%ld counted): %ld",
		map_count, used, counted, table_count);
	for (index = map_count; index < table_count; index++)
		empty += table[index].tag_index == NONE && table[index].group_tag == NONE;
	check(empty == 2, "the overflowing patch's two tags are empty (%ld)", empty);
	check(tag_loaded(UI_WIDGET_DEFINITION_TAG, "pc\\patch\\multiplayer_type_internet") ==
		children_of(mp_list)[3].widget_tag.index, "a row's tag is named pc\\patch\\<name>");
	check(textures_made == 1, "one texture made, DIRECT LINK's picture (%ld)", textures_made);
}

/* A failed bitmap build must leave both map blocks and the sequence intact. */
static void check_panel_failure(void)
{
	struct patch_work *work = calloc(1, sizeof(*work));
	struct string_list_entry entry = { 0 };
	struct string_list strings = { 0 };
	struct bitmap_data frame = { 0 };
	struct bitmap_group group = { 0 };
	struct bitmap_group_sequence sequence = { 0 };

	strings.strings.address = &entry;
	strings.strings.count = 1;
	group.bitmaps.address = &frame;
	group.bitmaps.count = 1;
	sequence.bitmap_count = 1;
	work->panel.strings = &strings;
	work->panel.group = &group;
	work->panel.sequence = &sequence;
	work->panel.base = work->panel.frames = 1;
	work->added_count = 1;
	work->added_descriptions[0] = "New description";
	work->added_pictures[0] = "png:ce/port/blank__0.png";
	reject_texture = TRUE;
	check(!patch_panel_write(work), "a failed bitmap build rejects the panel");
	reject_texture = FALSE;
	check(strings.strings.address == &entry && strings.strings.count == 1,
		"a failed bitmap build keeps the original string block");
	check(group.bitmaps.address == &frame && group.bitmaps.count == 1 && sequence.bitmap_count == 1,
		"a failed bitmap build keeps the original bitmap block and sequence");
	free(work);
}

/* ui.map's pictures that a frame asks to have saved in the cache (a game map's menus draw them from there): the one
that is not there yet is saved when the menus first draw, through the texture cache and the decoder, once; the one
that is there already is not (tools/test_menu_patches.py reads the PNG that is written) */
static long hardware_requests, art_extractions;
static unsigned char extracted_pixels[300 * 300 * 4];
static char const extracted_resource[] = "resource";

void *_texture_cache_bitmap_get_hardware_format(struct bitmap_data *bitmap, boolean block, boolean load)
{
	(void)bitmap;
	check(block && load, "the picture is loaded and waited for");
	hardware_requests++;
	return (void *)extracted_resource;
}

/* (xbox_textures.c's: a picture is saved where it is not there, or an upscaled version has appeared since it was
saved from the original, which the probe says of upgrade.png) */
int halo_menus_art_needs_save(char const *cache_path)
{
	return !halo_menus_cache_exists(cache_path) || !strcmp(cache_path, "cache/xbox/v1/upgrade.png");
}

int halo_menus_art_extract(void const *hardware_format, char const *cache_path)
{
	long x, y;

	art_extractions++;
	for (y = 0; y < 300; y++)
		for (x = 0; x < 300; x++)
		{
			unsigned char *pixel = extracted_pixels + (y * 300 + x) * 4;

			pixel[0] = (unsigned char)x;
			pixel[1] = (unsigned char)y;
			pixel[2] = (unsigned char)(x ^ y);
			pixel[3] = 255;
		}
	return hardware_format == extracted_resource && halo_menus_cache_write_png(cache_path, extracted_pixels, 300, 300);
}

static void check_extraction(void)
{
	unsigned long levels = 0;

	menu_art_texture(0, &levels);
	check(hardware_requests == 0 && art_extractions == 0, "texture resolution does not extract pictures");
	check(!halo_menus_cache_exists("cache/xbox/v1/extracted.png"), "the picture is not saved before the menu update");
	pc_menu_art_extract();
	check(hardware_requests == 2 && art_extractions == 2, "only the pictures that are not saved, or have an upscaled version "
		"now, are asked for (%ld, %ld)", hardware_requests, art_extractions);
	check(halo_menus_cache_exists("cache/xbox/v1/extracted.png"), "the picture is saved");
	{
		char note[32];

		check(halo_menus_cache_write_note("cache/xbox/v1/extracted.png.key", "00ff 2 2 2 2\n") &&
			halo_menus_cache_read("cache/xbox/v1/extracted.png.key", note, sizeof(note) - 1) == 13, "a note is kept beside a picture");
	}
	pc_menu_art_extract();
	check(art_extractions == 2, "a picture is saved once");
	check(!halo_menus_cache_exists("../escape.png") && !halo_menus_cache_write_png("../escape.png",
		extracted_pixels, 1, 1), "nothing is written outside the menus folder");
}

/* a frame's picture is its cache where the player's file is there and can be drawn, else its png, else nothing */
static void check_art(void)
{
	static unsigned long texture[2];
	unsigned long levels = 0, id = 100;
	char path[400];
	FILE *file;
	long index;
	struct { char const *art; unsigned int expected; char const *what; } const cases[] =
	{
		{ "cache/xbox/v1/there.png|xbox/art/shipped.png", 'C', "the player's cache is drawn before the shipped png" },
		{ "cache/xbox/v1/missing.png|xbox/art/shipped.png", 'S', "the shipped png is drawn where the cache is missing" },
		{ "cache/xbox/v1/broken.png|xbox/art/shipped.png", 'S', "the shipped png is drawn where the cache cannot be" },
		{ "cache/xbox/v1/missing.png|cache/xbox/v1/missing2.png", 0, "no picture is drawn where there is none" },
		{ "xbox/art/shipped.png", 'S', "a png alone is drawn" },
		{ "cache/xbox/v1/legacy.png|xbox/art/shipped.png", 'S', "an unversioned picture is ignored" },
		{ "../cache/xbox/v1/there.png|xbox/art/shipped.png", 'S', "a cache outside the menus folder is not read" },
	};

	snprintf(path, sizeof(path), "%smenus/cache/xbox/v1", folder);
	{
		char command[500];

		snprintf(command, sizeof(command), "mkdir -p '%s'", path);
		check(system(command) == 0, "the cache folder is made");
	}
	for (index = 0; index < 2; index++)
	{
		snprintf(path, sizeof(path), "%smenus/cache/xbox/v1/%s.png", folder, index ? "broken" : "there");
		file = fopen(path, "wb");
		check(file != NULL, "%s is written", path);
		if (file)
		{
			fputc(index ? 0 : 'C', file);
			fclose(file);
		}
	}
	snprintf(path, sizeof(path), "%smenus/cache/xbox/legacy.png", folder);
	file = fopen(path, "wb");
	check(file != NULL, "the old unversioned picture is written");
	if (file)
	{
		fputc('C', file);
		fclose(file);
	}
	for (index = 0; index < (long)(sizeof(cases) / sizeof(*cases)); index++)
	{
		texture[1] = id + index;
		halo_menus_art_register(texture, cases[index].art);
		check(menu_art_texture(id + index, &levels) == cases[index].expected, "%s", cases[index].what);
	}
	halo_menus_art_forget();
}

int main(int argc, char **argv)
{
	snprintf(mode, sizeof(mode), "%s", argc > 1 ? argv[1] : "apple");
	snprintf(folder, sizeof(folder), "%s/", argc > 2 ? argv[2] : "/nonexistent");
	setenv("HALO_HOST_TVOS", !strcmp(mode, "tvos") ? "1" : "0", 1);
	setenv("HALO_HOST_KEYBOARD", !strcmp(mode, "tvos") ? "0" : "1", 1);
	if (!strcmp(mode, "bad"))
	{
		char path[400];
		FILE *file;

		snprintf(path, sizeof(path), "%smenus/xbox/patches.xml", folder);
		file = fopen(path, "w");
		check(file != NULL, "the malformed patch is written to %s", path);
		if (file)
		{
			fputs(bad_xml, file);
			fclose(file);
		}
	}
	if (strcmp(mode, "bad"))
	{
		char path[400];
		FILE *file;

		snprintf(path, sizeof(path), "mkdir -p '%smenus/cache/xbox/v1'", folder);
		check(system(path) == 0, "the cache folder is made");
		snprintf(path, sizeof(path), "%smenus/cache/xbox/v1/extracted.png", folder);
		remove(path);
		snprintf(path, sizeof(path), "%smenus/cache/xbox/v1/present.png", folder);
		file = fopen(path, "wb");
		check(file != NULL, "%s is written", path);
		if (file)
			fclose(file);
		snprintf(path, sizeof(path), "%smenus/cache/xbox/v1/upgrade.png", folder);
		file = fopen(path, "wb");
		if (file)
		{
			fputs("saved from the original", file);
			fclose(file);
		}
	}
	map_build();
	menu_tags_loaded("ui");
	if (!strcmp(mode, "bad"))
	{
		check(!menu_tags.loaded && table == map_table && table_count == map_count,
			"a malformed patch fails the files: nothing is added");
	}
	else
	{
		check(menu_tags.loaded, "the menus loaded");
		if (menu_tags.loaded)
			check_apple(!strcmp(mode, "tvos"));
		{
			long xbox = tag_loaded(UI_WIDGET_DEFINITION_TAG, MM "main_menu");
			long pc = tag_loaded(UI_WIDGET_DEFINITION_TAG, "pc\\main_menu\\main_menu");
			long open = xbox;
			menu_mode = "auto";
			input_pad = 0;
			check(pc != NONE, "both looks loaded together");
			check(open == xbox, "input changes do not mutate an open screen");
			check(pc_menu_screen_open(open) == pc && !menu_tags.xbox_set, "next open takes keyboard look");
			input_pad = 1;
			check(pc_menu_screen_open(pc) == xbox && menu_tags.xbox_set, "back to main takes pad look");
			check(pc_menu_screen_open(mp_screen) == mp_screen && menu_tags.xbox_set,
				"an unavailable counterpart holds the look");
			menu_mode = "pc";
			check(pc_menu_screen_open(xbox) == pc, "fixed PC overrides pad input");
			menu_mode = "xbox";
			input_pad = 0;
			check(pc_menu_screen_open(pc) == xbox, "fixed Xbox overrides keyboard input");
			input_keyboard = 0;
			pc_menu_screen_open(xbox);
			{
				struct string_list *list = tag_get(UNICODE_STRING_LIST_TAG,
					tag_loaded(UNICODE_STRING_LIST_TAG, "pc\\main_menu\\settings_select\\player_setup\\player_profile_edit\\profile_edit_descriptions"));
				check(string_is(list, 0, "Rename this profile using\\nthe virtual keyboard.\\n\\nCurrent Profile:"),
					"keyboard removal changes the next screen's description");
				input_keyboard = 1;
				pc_menu_screen_open(xbox);
				check(string_is(list, 0, "Rename this profile with your\\nkeyboard.\\n\\nCurrent Profile:"),
					"keyboard attachment changes the next screen's description");
			}
		}
		check_panel_failure();
		check_extraction();
		check_art();
		menu_tags_unloaded();
		check(table == map_table && table_count == map_count, "the map's own table is back after unloading");
	}
	printf("%s (%s)\n", failures ? "menu patch probe FAILED" : "menu patch probe passed", mode);
	return failures != 0;
}
