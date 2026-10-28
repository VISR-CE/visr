/*
MENU_TAGS.C

The menus written in XML (halo_menus.h, read by port/linux/src/menu_files.c),
built into the tags the game's widgets are made of when ui.map has loaded
(scenario_tags_load), and added to its tag table: a widget definition
('DeLa') for each <widget>, a unicode string list ('ustr') for each
<strings> and each widget's own text, and a bitmap group ('bitm') for each
<bitmap>, whose textures are drawn from their PNGs (menu_art_texture,
xbox_textures.c). The game's widgets then run them as their own:
main_screen_shell_load opens pc_menus_root_name()'s.

Each name the files use is looked up here: our widgets (named pc\<name>),
the map's tags (a path, with its backslashes), the game's event handler and
game data functions, and the port's own (menu_functions.c, from
PC_MENU_FUNCTION_BASE), which include the PC version's (most of which do
nothing yet). Its mouse events are kept, though this engine never sends
them (the port's mouse support clicks with A). Anything that does not resolve, or that the widget
code would stop the game for (a spinner's children, a list from strings
with children), is logged with its file and line and nothing is added: the
game keeps its own menus.

The tag table cannot grow where it is (the tags' names follow it), so it
is copied, with ours after it; every existing tag keeps its index. All of it
is let go in scenario_tags_unload, before the next map's tags load.

A multiplayer map gets them too, for its in-game pause menu's SETTINGS
(pause_patch): there the map's tags they name that it has not (ui.map's
pictures, sounds and screens) are none, a missing picture drawn blank, and
no screen pauses the game (a network game's clock runs on).
*/

#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "tag_files/tag_groups.h"
#include "text/text_group.h"
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"

#include "halo_menus.h"

#include <stdlib.h>
#include <string.h>

/* the platform layer's (port/linux/src) */
void platform_log(char const *format, ...);
char const *config_string(char const *name);
int platform_display_resolutions(long *widths, long *heights, int maximum);
int platform_window_sizes(long *widths, long *heights, int maximum);

/* the game's network server (this machine hosts: network_game_globals.c) */
void *global_network_game_server_get(void);

/* cache_files.c's (port) */
void *cache_files_tag_instances(long *count);
void cache_files_set_tag_instances(void *instances, long count);

/* the game's (port) */
char *tag_get_name(long tag_index);
char const *ui_widget_event_handler_function_name(long function_index);

void menu_tags_loaded(char const *map_name);
void menu_tags_unloaded(void);
char const *pc_menus_root_name(void);
char const *pc_menu_function_name(long function_index);
char const *pc_menu_game_data_input_name(long function_index);
boolean pc_menu_tag(long tag_index);
/* whether the multiplayer mode chosen is system link (menu_functions.c) */
boolean pc_menu_multiplayer_lan(void);
short pc_menu_list_window(long tag_index);
boolean pc_menu_patch_list_index(long list_tag, long row_tag, short *description);
boolean pc_menu_frame_placement(struct bitmap_data const *bitmap, short *x, short *y, short *width, short *height);

/* The native Mac's DISPLAY choice follows the window between displays.
Keep its UTF-16 backing stable so an open spinner sees the new label. */
static char display_rate_label[32];
static unsigned short display_rate_unicode[32];

int pc_menu_display_rate_refresh(void)
{
#ifdef HALO_IOS
	int host_sdl_display_max_fps(void);
	int fps = host_sdl_display_max_fps();

	if (fps > 0)
	{
		snprintf(display_rate_label, sizeof(display_rate_label), "DISPLAY (%d HZ)", fps);
		halo_menus_utf16(display_rate_label, display_rate_unicode, NUMBEROF(display_rate_unicode));
	}
	return fps;
#else
	return 0;
#endif
}

/* ---------- constants */

#define PC_MENU_TAG_PREFIX "pc\\"
/* the multiplayer maps' widget collection, whose pause screens pause_patch
adds to */
#define MULTIPLAYER_COLLECTION "ui\\shell\\multiplayer"
/* the tags pause_patch adds: two buttons and their texts */
#define PAUSE_PATCH_TAGS 4
/* what a frame of ui.map's is drawn as in a game map, which has not got it */
#define BLANK_PNG "ce/port/blank__0.png"
#define UI_WIDGET_DEFINITION_TAG 'DeLa'
#define UNICODE_STRING_LIST_TAG 'ustr'
#define FONT_TAG 'font'
#define SOUND_TAG 'snd!'
#define MAXIMUM_STRINGS 64
#define MAXIMUM_TEXT 2048
/* the size of a menu bitmap's D3D texture, which only stands for its PNG */
#define PLACEHOLDER_SIZE 4

enum
{
	_bitmap_type_2d = 0,
	_bitmap_format_a8r8g8b8 = 11,
	_bitmap_has_power_of_two_dimensions_bit = 0,
};

enum
{
	_event_handler_close_current_widget_bit = 0,
	_event_handler_close_other_widget_bit,
	_event_handler_close_all_widgets_bit,
	_event_handler_open_widget_bit,
	_event_handler_reload_self_bit,
	_event_handler_reload_other_widget_bit,
	_event_handler_give_focus_to_widget_bit,
	_event_handler_run_function_bit,
	_event_handler_replace_self_with_widget_bit,
	_event_handler_go_back_to_previous_widget_bit,
	_event_handler_run_scenario_script_bit,
	_event_handler_try_to_branch_on_failure_bit,
};

enum
{
	_widget_type_container,
	_widget_type_text_box,
	_widget_type_spinner_list,
	_widget_type_column_list,
};

enum
{
	_list_items_generated_from_string_list_tag_bit = 1,
};

enum
{
	_child_widget_use_custom_controller_index_bit = 0,
};

/* ---------- structures */

struct cache_file_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

/* (the sizes are the guest's, of 32-bit pointers: tools/test_menu_patches.py
builds this file for the Mac it runs on, MENU_TAGS_PROBE) */
#ifndef MENU_TAGS_PROBE
typedef char verify_cache_file_tag_instance_size[
	sizeof(struct cache_file_tag_instance) == 0x20 ? 1 : -1];
#endif

/* the widget definition, as ui_widget.c has it */
struct ui_widget_event_handler_reference
{
	long flags;
	short event_type;
	short function;
	struct tag_reference widget_tag;
	struct tag_reference sound_effect;
	char script[32];
};

struct ui_widget_child_reference
{
	struct tag_reference widget_tag;
	char name[32];
	long flags;
	short custom_controller_index;
	short vertical_offset;
	short horizontal_offset;
	byte unknown03A[0x50 - 0x3A];
};

struct ui_widget_conditional_reference
{
	struct tag_reference widget_tag;
	char name[32];
	long flags;
	short custom_controller_index;
	byte unknown036[0x50 - 0x36];
};

struct ui_widget_search_and_replace_reference
{
	char search_string[32];
	short replace_function;
};

struct ui_widget_game_data_input_reference
{
	short function;
	byte unknown002[0x24 - 0x02];
};

struct ui_widget_definition
{
	short type;
	short controller_index;
	char name[32];
	rectangle2d bounds;
	long flags;
	long milliseconds_to_auto_close;
	long auto_close_fade_time;
	struct tag_reference background_bitmap;
	struct tag_block game_data_inputs;
	struct tag_block event_handlers;
	struct tag_block search_and_replace_functions;
	byte unknown06C[0xEC - 0x6C];
	struct tag_reference text_label_string_list;
	struct tag_reference text_font;
	real_argb_color text_color;
	short justification;
	word text_box_flags;
	byte unknown120[0x12E - 0x120];
	short string_list_index;
	short horizontal_offset;
	short vertical_offset;
	byte unknown134[0x150 - 0x134];
	long list_flags;
	struct tag_reference list_header_bitmap;
	struct tag_reference list_footer_bitmap;
	rectangle2d list_header_bounds;
	rectangle2d list_footer_bounds;
	byte unknown184[0x1A4 - 0x184];
	struct tag_reference extended_description_widget;
	byte unknown1B4[0x2D4 - 0x1B4];
	struct tag_block conditional_widgets;
	byte unknown2E0[0x3E0 - 0x2E0];
	struct tag_block child_widgets;
};

#ifndef MENU_TAGS_PROBE
typedef char verify_ui_widget_event_handler_reference_size[
	sizeof(struct ui_widget_event_handler_reference) == 0x48 ? 1 : -1];
typedef char verify_ui_widget_child_reference_size[
	sizeof(struct ui_widget_child_reference) == 0x50 ? 1 : -1];
typedef char verify_ui_widget_conditional_reference_size[
	sizeof(struct ui_widget_conditional_reference) == 0x50 ? 1 : -1];
typedef char verify_ui_widget_search_and_replace_reference_size[
	sizeof(struct ui_widget_search_and_replace_reference) == 0x22 ? 1 : -1];
typedef char verify_ui_widget_game_data_input_reference_size[
	sizeof(struct ui_widget_game_data_input_reference) == 0x24 ? 1 : -1];
typedef char verify_ui_widget_definition_text_color_offset[
	offsetof(struct ui_widget_definition, text_color) == 0x10C ? 1 : -1];
typedef char verify_ui_widget_definition_list_flags_offset[
	offsetof(struct ui_widget_definition, list_flags) == 0x150 ? 1 : -1];
typedef char verify_ui_widget_definition_conditional_widgets_offset[
	offsetof(struct ui_widget_definition, conditional_widgets) == 0x2D4 ? 1 : -1];
typedef char verify_ui_widget_definition_size[
	sizeof(struct ui_widget_definition) == 0x3EC ? 1 : -1];
#endif

/* a frame of ui.map's that a menu file scales: where in its widget, and at
what size, it is drawn (pc_menu_frame_placement) */
struct menu_frame_placement
{
	struct bitmap_data const *bitmap;
	short x, y;
	short width, height;
};

/* a patched list's row and its description's number */
struct menu_patch_index
{
	long list_tag;
	long row_tag;
	short description;
};

/* a spinner bound to a setting (menu_functions.c) */
struct pc_menu_setting
{
	long definition_index;
	char const *setting;
	long value_count;
	char const *values[MAXIMUM_STRINGS];
	short loaded_index;
};

/* ---------- globals */

/* the names the files may use, by their values */
static char const *const event_names[] =
{
	"a", "b", "x", "y", "black", "white", "left_trigger", "right_trigger",
	"up", "down", "left", "right", "start", "back", "left_thumb", "right_thumb",
	"stick_up", "stick_down", "stick_left", "stick_right",
	"right_stick_up", "right_stick_down", "right_stick_left", "right_stick_right",
	"created", "deleted",
	/* (the PC version's, which this engine never sends) */
	"get_focus", "lose_focus", "left_mouse", "middle_mouse", "right_mouse", "double_click", "custom_activation",
	"post_render",
};

static char const *const widget_flag_names[] =
{
	"pass_unhandled_to_focused_child", "pause_game", "flash_bitmap", "up_down_tabs_children",
	"left_right_tabs_children", "up_down_tabs_items", "left_right_tabs_items", "no_focused_child",
	"pass_unhandled_to_all_children", "render_any_controller", "pass_handled_to_all_children",
	"main_menu_if_no_history", "tag_controller_index", "nifty_fx", "no_history",
	/* (the PC version's, which this engine has no use for) */
	"force_handle_mouse", "no_widescreen_fill",
};

static char const *const widget_type_names[] = { "container", "text", "spinner", "column_list" };
static char const *const list_flag_names[] = { "items_in_code", "items_from_strings", "one_tooltip", "single_preview" };
static char const *const text_flag_names[] = { "editable", "password", "flashing", "no_focus_test" };
static char const *const align_names[] = { "left", "right", "center" };
/* (build_number and pid are the PC version's, which this engine has not) */
static char const *const replace_names[] = { "none", "controller", "build_number", "pid" };

/* the game data functions, as the tags name them
(ui_widget_game_data_input_functions.c) */
static char const *const game_data_input_names[] =
{
	"NULL", "player settings menu update desc", "unused", "playlist settings menu update desc",
	"gametype select menu update desc", "multiplayer type menu update desc", "solo level select update",
	"difficulty menu update desc", "build number textbox only", "server list update",
	"network pregame status update", "splitscreen pregame status update", "net splitscreen prejoin players",
	"mp profile list update", "3wide player profile list update", "plyr prof edit select menu upd8",
	"player profile small menu update", "game settings lists text update", "solo game objective text",
	"color picker update", "game settings lists pic update", "main menu fake animate",
	"mp level select update", "get active plyr profile name", "get edit plyr profile name",
	"get edit game settings name", "get active plyr profile color", "mp set textbox map name",
	"mp set textbox game ruleset", "mp set textbox teams noteams", "mp set textbox score limit",
	"mp set textbox score limit type", "mp set bitmap for map", "mp set bitmap for ruleset",
	"mp set textbox", "mp edit profile set rule text", "system link status check", "mp game directions",
	"teams no teams bitmap update", "warn if diff will nuke saved game", "dim if no net cable",
};

/* the port's event handler functions (menu_functions.c), from
PC_MENU_FUNCTION_BASE: its own, then the PC version's that the Xbox's have
not (as its tags name them), then its own of the Xbox's names, which our
widgets run instead of the Xbox's (those take the Xbox's widgets: a spinner
of levels, the difficulty list itself) */
static char const *const port_function_names[] =
{
	"port quit game", "port setting load", "port setting save", "unwired",
	"exit gracefully to xbox dashboard", "pause game invert pitch", "start new coop game",
	"pause game invert spinner get", "pause game invert spinner set", "main menu quit game",
	"mouse emit accept event", "mouse emit back event", "mouse emit dpad left event",
	"mouse emit dpad right event", "mouse spinner 3wide click", "controls screen init", "video screen init",
	"controls begin binding", "gamespy screen init", "gamespy screen dispose", "gamespy select header",
	"gamespy select item", "gamespy select button", "plr prof init mouse set", "plr prof change mouse set",
	"plr prof init audio set", "plr prof change audio set", "plr prof change video set",
	"controls screen dispose", "controls screen change set", "mouse emit x event", "gamepad screen init",
	"gamepad screen dispose", "gamepad screen change gamepads", "gamepad screen select item",
	"mouse screen defaults", "audio screen defaults", "video screen defaults", "controls screen defaults",
	"profile set edit begin", "profile manager delete", "profile manager select", "gamespy dismiss error",
	"server settings init", "ss edit server name", "ss edit server password", "ss start game",
	"video test dialog init", "video test dialog dispose", "video test dialog accept",
	"gamespy dismiss filters", "gamespy update filter settings", "gamespy back handler",
	"mouse spinner 1wide click", "controls back handler", "controls advanced launch", "controls advanced ok",
	"mp pause menu open", "mp game options open", "mp choose team", "mp prof init vehicle options",
	"mp prof save vehicle options", "single prev cl item activated", "mp prof init teamplay options",
	"mp prof save teamplay options", "mp game options choose", "emit custom activation event",
	"plr prof cancel audio set", "plr prof init network options", "plr prof save network options",
	"difficulty item select",
	"gamespy get patch", "video screen dispose", "campaign menu init", "campaign menu continue",
	"load game menu init", "load game menu dispose", "load game menu activated", "solo menu save checkpoint",
	"mp type set mode", "direct ip connect init",
	"direct ip connect go", "direct ip edit field", "network settings edit a port", "network settings defaults",
	"load game menu delete request", "load game menu delete finish",
	"initialize sp level list solo", "dispose sp level list", "solo level set map", "set difficulty",
	"port settings save", "port settings defaults", "controls binding slot",
	"color picker menu initialize", "color picker menu dispose", "color picker select color",
	"player profile list initialize", "player profile list dispose", "request del player profile",
	"final del player profile",
	"ss copy invite", "join controller to mp game", "mp level list initialize", "mp level list dispose",
	"mp level select", "mp profiles list initialize", "mp profiles list dispose", "mp profile set for game",
	"port lobby preview join",
	"port setup edit",
	"port map list back",
	"port pause end game",
	"port coop begin", "port coop player 2 list initialize", "port coop player 2",
	"port lobby open", "port lobby add player", "port lobby join", "port lobby leave",
	"port lobby player list initialize", "port lobby player choose",
	"port lobby preview add", "port lobby preview leave",
	/* (the gametype editor's: the Xbox's walk their rows by place, which the
	PC version's screens changed) */
	"mp profile begin editing", "mp profile save changes", "request del playlist profile", "final del playlist profile",
	"mp profile init game engine", "mp profile set game engine", "mp profile init ctf rules",
	"mp profile init koth rules", "mp profile init slayer rules", "mp profile init oddball rules",
	"mp profile init racing rules", "mp profile init player opts", "mp profile init item options",
	"mp profile init indicator opts", "mp profile set ctf rules", "mp profile set koth rules",
	"mp profile set slayer rules", "mp profile set oddball rules", "mp profile set racing rules",
	"mp profile set player options", "mp profile set item options", "mp profile set indicator opts",
	/* (Settings' OK: the Xbox's fails when the profile has no changes, the
	settings' screens having written theirs to config.toml) */
	"player profile save changes",
	/* (the server browser's password screen) */
	"port password init", "port password edit", "port password join", "port password back",
	/* (Settings begins editing after its new profile is created) */
	"new campaign decision",
	/* (Direct Link's ENTER CODE screen: a join code or an invite link) */
	"port code open", "port code init", "port code edit", "port code join", "port code back",
	/* (the lobby's Y: the invite link in the share sheet, Apple builds) */
	"port lobby share",
	/* (the Xbox look's main menu: CUSTOM CAMPAIGN is there only with Custom Edition campaign maps, and opens
	their list) */
	"disable if no custom campaigns", "port custom campaign begin",
	/* (the Apple TV's DELETE PROFILE and DELETE GAMETYPE rows: the one being edited is the one the Xbox's
	confirmation deletes) */
	"port delete request", "port delete finish",
	/* (the Xbox look's Item Options rows the Xbox's functions have no fields for: map weapons, loadout) */
	"port option row init", "port option row save",
};

/* the PC version's game data functions that the Xbox's have not, from
PC_MENU_FUNCTION_BASE (menu_functions.c) */
static char const *const port_game_data_input_names[] =
{
	"unwired",
	"pause game set textbox inverted", "dim unless two controllers", "controls update menu",
	"video menu update", "gamespy screen update", "common button bar update", "gamepad update menu",
	"server settings update", "audio menu update", "mp prof vehicles update", "solo map list update",
	"mp map list update", "gt select list update", "gt edit list update", "load game list update",
	"direct ip connect update", "network settings update",
	"port cloud saves status", "port settings help", "color picker update", "3wide player profile list update", "port lobby update",
	"port lobby preview update",
	/* (the gametype editor's: the Xbox's stops the game on the buttons'
	row; the Xbox's read only player_ui's gametype, not Server Setup's) */
	"game settings lists text update", "get edit game settings name", "mp edit profile set rule text",
	"port password update",
	/* (the profile settings' picture: the Xbox's of the button settings,
	on Gamepad Setup's row) */
	"port gamepad layout preview",
	/* (Direct Link's ENTER CODE screen) */
	"port code update",
};

static struct
{
	/* what is let go when the map unloads */
	void **blocks;
	long block_count;
	struct bitmap_data **bitmaps;
	long bitmap_count;
	struct menu_frame_placement *placements;
	long placement_count;
	struct cache_file_tag_instance *original_instances;
	long original_count;
	struct pc_menu_setting *settings;
	long setting_count;
	/* the patched lists' rows, each with its description's number (NONE:
	none), which their descriptions are shown by (pc_menu_patch_list_index) */
	struct menu_patch_index *patch_indices;
	long patch_index_count;
	/* the column lists that show a window of their rows (visible_rows=) */
	struct { long tag_index; short rows; } *windows;
	long window_count;
	boolean loaded;
	/* the Xbox look's set (display.menus not "pc"), else the PC version's */
	boolean xbox_set;
	char root[300];
} menu_tags;

/* the build under way */
static struct
{
	struct halo_menus const *menus;
	long first_index;
	long first_salt;
	long next;
	/* the tags counted for the table: next stays at most this */
	long total;
	/* each widget's, bitmap's and string list's tag index, and each
	widget's own text's and strings' (NONE: none) */
	long *widget_tags;
	long *bitmap_tags;
	long *strings_tags;
	long *text_tags;
	long *spinner_tags;
	boolean failed;
	/* a multiplayer map's build (the pause menu's SETTINGS): what the map
	has not got is none, not a failure */
	boolean game_map;
	boolean xbox_set;
} build;

/* ---------- private code */

/* (the game's allocator, which malloc and free are here: cseries.h) */
static void *allocate(long size)
{
	void *block = malloc(size > 0 ? size : 1);

	if (!block)
	{
		build.failed = TRUE;
		return NULL;
	}
	memset(block, 0, size > 0 ? size : 1);
	{
		void **blocks = realloc(menu_tags.blocks, (menu_tags.block_count + 1) * sizeof(*menu_tags.blocks));

		if (!blocks)
		{
			free(block);
			build.failed = TRUE;
			return NULL;
		}
		menu_tags.blocks = blocks;
	}
	menu_tags.blocks[menu_tags.block_count++] = block;
	return block;
}

static void problem(char const *file, long line, char const *message, char const *detail)
{
	if (!build.failed)
		halo_menus_log(file, line, message, detail);
	build.failed = TRUE;
}

static long name_index(char const *name, char const *const *names, long count)
{
	long index;

	for (index = 0; name && index < count; index++)
	{
		if (!strcmp(name, names[index]))
			return index;
	}
	return NONE;
}

/* the bits of the space-separated names */
static long flags_parse(char const *text, char const *const *names, long count, char const *file, long line)
{
	long flags = 0;

	while (text && *text)
	{
		char word[64];
		long length, bit;

		text += strspn(text, " \t\r\n");
		length = (long)strcspn(text, " \t\r\n");
		if (!length)
			break;
		if (length >= (long)sizeof(word))
			length = sizeof(word) - 1;
		memcpy(word, text, length);
		word[length] = 0;
		text += strcspn(text, " \t\r\n");
		bit = name_index(word, names, count);
		if (bit == NONE)
			problem(file, line, "there is no flag", word);
		else
			flags |= 1L << bit;
	}
	return flags;
}

static void reference_clear(struct tag_reference *reference, long group_tag)
{
	reference->group_tag = group_tag;
	reference->name = "";
	reference->name_length = 0;
	reference->index = NONE;
}

/* a reference to a tag of ours not yet in the table (a patch's row and its text, which instance_set writes once
the patch is made): no name, which the table cannot give it yet */
static void reference_set_pending(struct tag_reference *reference, long group_tag, long tag_index)
{
	reference->group_tag = group_tag;
	reference->name = "";
	reference->name_length = 0;
	reference->index = tag_index;
}

static void reference_set(struct tag_reference *reference, long group_tag, long tag_index)
{
	if (tag_index == NONE)
	{
		reference_clear(reference, group_tag);
		return;
	}
	reference->group_tag = group_tag;
	reference->name = (char *)tag_get_name(tag_index);
	reference->name_length = (long)strlen(reference->name);
	reference->index = tag_index;
}

static long find(char const *name, long count, char const *(*name_of)(long))
{
	long index;

	for (index = 0; name && index < count; index++)
	{
		if (!strcmp(name_of(index), name))
			return index;
	}
	return NONE;
}

static char const *widget_name(long index) { return build.menus->widgets[index].name; }
static char const *bitmap_name(long index) { return build.menus->bitmaps[index].name; }
static char const *strings_name(long index) { return build.menus->string_lists[index].name; }

static long widget_named(char const *name)
{
	return find(name, build.menus->widget_count, widget_name);
}

static long map_tag(long group_tag, char const *name, char const *file, long line)
{
	long index = tag_loaded(group_tag, name);

	/* (a game map has few of ui.map's: those are none) */
	if (index == NONE && !build.game_map)
		problem(file, line, "the map has no tag", name);
	return index;
}

/* a tag: the map's by its path (with a backslash), else one of ours */
static long tag_named(long group_tag, char const *name, char const *file, long line)
{
	long index = NONE;

	if (strchr(name, '\\'))
		return map_tag(group_tag, name, file, line);
	switch (group_tag)
	{
	case UI_WIDGET_DEFINITION_TAG:
		index = widget_named(name);
		index = index == NONE ? NONE : build.widget_tags[index];
		break;
	case BITMAP_GROUP_TAG:
		index = find(name, build.menus->bitmap_count, bitmap_name);
		index = index == NONE ? NONE : build.bitmap_tags[index];
		break;
	case UNICODE_STRING_LIST_TAG:
		index = find(name, build.menus->string_list_count, strings_name);
		index = index == NONE ? NONE : build.strings_tags[index];
		break;
	}
	if (index == NONE)
		problem(file, line, "there is nothing named", name);
	return index;
}

/* "unwired <name>": a function the PC version has for these widgets, not
yet written for this engine (port/assets/menus/UNWIRED.md), which does
nothing */
static boolean unwired(char const *name)
{
	return !strncmp(name, "unwired ", 8);
}

/* an event handler function by its name (the port's, else the game's), or
NONE */
static long function_lookup(char const *name)
{
	char const *function_name;
	long index;

	if (unwired(name))
		name = "unwired";
	index = name_index(name, port_function_names, NUMBEROF(port_function_names));
	if (index != NONE)
		return PC_MENU_FUNCTION_BASE + index;
	for (index = 0; (function_name = ui_widget_event_handler_function_name(index)) != NULL; index++)
	{
		if (!strcmp(name, function_name))
			return index;
	}
	return NONE;
}

static long function_index(char const *name, char const *file, long line)
{
	long index = function_lookup(name);

	if (index == NONE)
		problem(file, line, "there is no event handler function", name);
	return index;
}

static long game_data_input_index(char const *name, char const *file, long line)
{
	long index;

	if (unwired(name))
		name = "unwired";
	/* (ours first, as for the event handlers) */
	index = name_index(name, port_game_data_input_names, NUMBEROF(port_game_data_input_names));
	if (index != NONE)
		return PC_MENU_FUNCTION_BASE + index;
	index = name_index(name, game_data_input_names, NUMBEROF(game_data_input_names));
	if (index != NONE)
		return index;
	problem(file, line, "there is no game data input", name);
	return NONE;
}

static long hex_digit(char character)
{
	if (character >= '0' && character <= '9')
		return character - '0';
	if (character >= 'a' && character <= 'f')
		return character - 'a' + 10;
	if (character >= 'A' && character <= 'F')
		return character - 'A' + 10;
	return NONE;
}

/* "#RRGGBB" or "#AARRGGBB" */
static boolean parse_color(char const *text, real_argb_color *color)
{
	long length = text ? (long)strlen(text) : 0;
	long components[4] = { 255, 0, 0, 0 };
	long index;

	if ((length != 7 && length != 9) || text[0] != '#')
		return FALSE;
	for (index = 0; index < (length - 1) / 2; index++)
	{
		long high = hex_digit(text[1 + 2 * index]), low = hex_digit(text[2 + 2 * index]);

		if (high == NONE || low == NONE)
			return FALSE;
		components[length == 7 ? index + 1 : index] = high * 16 + low;
	}
	color->alpha = components[0] / 255.0f;
	color->red = components[1] / 255.0f;
	color->green = components[2] / 255.0f;
	color->blue = components[3] / 255.0f;
	return TRUE;
}

/* "top left bottom right" */
static void parse_bounds(char const *text, rectangle2d *bounds, char const *file, long line)
{
	long top, left, bottom, right;
	char extra;

	if (text && sscanf(text, "%ld %ld %ld %ld %c", &top, &left, &bottom, &right, &extra) == 4)
	{
		bounds->y0 = (short)top;
		bounds->x0 = (short)left;
		bounds->y1 = (short)bottom;
		bounds->x1 = (short)right;
	}
	else if (text)
	{
		problem(file, line, "bounds are \"top left bottom right\":", text);
	}
}

/* a 'ustr' of the strings */
static void *string_list_build(char const *const *strings, long count)
{
	struct string_list *list = allocate(sizeof(struct string_list));
	struct string_list_entry *entries = allocate(count * sizeof(struct string_list_entry));
	long index;

	if (!list || !entries)
		return list;
	list->strings.count = count;
	list->strings.address = entries;
	for (index = 0; index < count; index++)
	{
		long length = (long)strlen(strings[index]);
		unsigned short *characters = allocate((length * 2 + 2) * sizeof(unsigned short));
		long written;

		if (!characters)
			return list;
		written = halo_menus_utf16(strings[index], characters, length * 2 + 2);
		entries[index].string.size = written * (long)sizeof(unsigned short);
		entries[index].string.address = characters;
	}
	return list;
}

/* the "|"-separated pieces of text, copied (at most MAXIMUM_STRINGS) */
static long split(char const *text, char const **pieces)
{
	long count = 0;

	while (text && count < MAXIMUM_STRINGS)
	{
		long length = (long)strcspn(text, "|");
		char *piece = allocate(length + 1);

		if (!piece)
			break;
		memcpy(piece, text, length);
		pieces[count++] = piece;
		if (!text[length])
			break;
		text += length + 1;
	}
	return count;
}

/* a spinner's strings, or its values, split: its own, but Video Setup's
sizes are the display's (sdl_platform.c), shown "1920 x 1080" and set
"1920x1080": its resolutions after Resolution's own (NATIVE), and the
window sizes that fit it in place of Window Size's own */
static long spinner_split(struct halo_menu_widget const *widget, boolean values, char const **pieces)
{
	long count = split(values ? widget->values : widget->strings, pieces);
	long widths[MAXIMUM_STRINGS], heights[MAXIMUM_STRINGS];
	long added, index;

	if (widget->setting && !strcmp(widget->setting, "display.resolution"))
		added = platform_display_resolutions(widths, heights, (int)(MAXIMUM_STRINGS - count));
	else if (widget->setting && !strcmp(widget->setting, "display.window_size"))
	{
		added = platform_window_sizes(widths, heights, MAXIMUM_STRINGS);
		if (added > 0)
			count = 0;
	}
	else if (widget->setting && !strcmp(widget->setting, "display.frame_rate"))
	{
		/* the Mac app's Frame Rate: the display's own highest rate
		(HALO_DISPLAY_MAX_FPS, port/ios/host/host_main.m) before the last
		choice, Unlimited, when it is none of those listed */
		const char *rate;
		long fps;
		char text[16];
		char *piece;

		if (pc_menu_display_rate_refresh() > 0)
		{
			if (!values && count > 0)
				pieces[0] = display_rate_label;
			return count;
		}
		rate = getenv("HALO_DISPLAY_MAX_FPS");
		fps = rate ? atol(rate) : 0;

		if (fps < 1 || fps > 1000 || count < 1 || count >= MAXIMUM_STRINGS)
			return count;
		snprintf(text, sizeof(text), "%ld", fps);
		/* (the values are the numbers themselves, so both lists are searched alike) */
		for (index = 0; index < count; index++)
		{
			if (!strcmp(pieces[index], text))
				return count;
		}
		piece = allocate((long)strlen(text) + 1);
		if (!piece)
			return count;
		strcpy(piece, text);
		pieces[count] = pieces[count - 1];
		pieces[count - 1] = piece;
		return count + 1;
	}
	else
		return count;
	for (index = 0; index < added; index++)
	{
		char text[32];
		char *piece;

		snprintf(text, sizeof(text), values ? "%ldx%ld" : "%ld x %ld", widths[index], heights[index]);
		piece = allocate((long)strlen(text) + 1);
		if (!piece)
			break;
		strcpy(piece, text);
		pieces[count++] = piece;
	}
	return count;
}

/* a frame drawn from one of our PNGs, at width by height: its D3D texture
is a small one, which the PNG is drawn for (the game draws a bitmap in units
of its size, to whatever texture it has); FALSE when the texture could not be
made */
static boolean png_frame_build(struct bitmap_data *bitmap, long tag_index, char const *png, long width, long height)
{
	struct bitmap_data **bitmaps;

	bitmap->signature = BITMAP_GROUP_TAG;
	bitmap->width = PLACEHOLDER_SIZE;
	bitmap->height = PLACEHOLDER_SIZE;
	bitmap->depth = 1;
	bitmap->type = _bitmap_type_2d;
	bitmap->format = _bitmap_format_a8r8g8b8;
	bitmap->flags = FLAG(_bitmap_has_power_of_two_dimensions_bit);
	bitmap->tag_index = tag_index;
	bitmap->cache_block_index = NONE;
	if (!rasterizer_bitmap_new(bitmap))
		return FALSE;
	bitmap->width = (short)width;
	bitmap->height = (short)height;
	bitmaps = realloc(menu_tags.bitmaps, (menu_tags.bitmap_count + 1) * sizeof(*menu_tags.bitmaps));
	if (!bitmaps)
	{
		rasterizer_bitmap_delete(bitmap);
		return FALSE;
	}
	menu_tags.bitmaps = bitmaps;
	menu_tags.bitmaps[menu_tags.bitmap_count++] = bitmap;
	/* (none without a renderer: debug.null_renderer) */
	if (bitmap->hardware_format)
		halo_menus_art_register(bitmap->hardware_format, png);
	return TRUE;
}

static void *bitmap_build(struct halo_menu_bitmap const *source, long tag_index)
{
	struct bitmap_group *group = allocate(sizeof(struct bitmap_group));
	struct bitmap_group_sequence *sequence = allocate(sizeof(struct bitmap_group_sequence));
	struct bitmap_data *bitmaps = allocate(source->frame_count * sizeof(struct bitmap_data));
	long frame;

	if (build.failed)
		return group;
	if (!source->frame_count)
	{
		problem(source->file, source->line, "a bitmap has no frames:", source->name);
		return group;
	}
	sequence->first_bitmap_index = 0;
	sequence->bitmap_count = (short)source->frame_count;
	group->sequences.count = 1;
	group->sequences.address = sequence;
	group->bitmaps.count = source->frame_count;
	group->bitmaps.address = bitmaps;
	for (frame = 0; frame < source->frame_count; frame++)
	{
		struct halo_menu_frame const *data = &build.menus->frames[source->first_frame + frame];
		struct bitmap_data *bitmap = &bitmaps[frame];
		char const *png = data->png;
		long width = data->width, height = data->height;

		/* (a frame of the map's: a copy of its bitmap, which the texture cache
		loads from the map as its own; in a game map that has not got it, its
		png where it has one, else nothing) */
		if (data->map && build.game_map && tag_loaded(BITMAP_GROUP_TAG, data->map) == NONE)
		{
			if (!png)
			{
				png = BLANK_PNG;
				width = height = PLACEHOLDER_SIZE;
			}
		}
		else if (data->map)
		{
			long group_index = map_tag(BITMAP_GROUP_TAG, data->map, source->file, source->line);
			struct bitmap_group *group_source;

			if (group_index == NONE)
				return group;
			group_source = bitmap_group_get(group_index);
			if (data->index < 0 || data->index >= group_source->bitmaps.count)
			{
				/* (the map's bitmap can have fewer frames than the PC art the
				file names, ui.map's arrows among them, and a game map's copy
				fewer than ui.map's: its png then, as when the map has not got it) */
				if (!png)
				{
					problem(source->file, source->line, "the map's bitmap has no such frame:", data->map);
					return group;
				}
				goto frame_png;
			}
			memcpy(bitmap, (struct bitmap_data *)group_source->bitmaps.address + data->index, sizeof(*bitmap));
			bitmap->cache_block_index = NONE;
			bitmap->base_address = NULL;
			/* (scaled to a size of the file's: the texture stays the map's,
			drawn smaller or larger, ui_widget.c) */
			if (data->width > 0 && data->height > 0 && data->width <= 2048 && data->height <= 2048)
			{
				struct menu_frame_placement *placements = realloc(menu_tags.placements,
					(menu_tags.placement_count + 1) * sizeof(*menu_tags.placements));

				if (placements)
				{
					menu_tags.placements = placements;
					placements[menu_tags.placement_count].bitmap = bitmap;
					placements[menu_tags.placement_count].x = (short)PIN(data->x, -2048, 2048);
					placements[menu_tags.placement_count].y = (short)PIN(data->y, -2048, 2048);
					placements[menu_tags.placement_count].width = (short)data->width;
					placements[menu_tags.placement_count].height = (short)data->height;
					menu_tags.placement_count++;
				}
			}
			continue;
		}

	frame_png:
		if (width <= 0 || height <= 0 || width > 2048 || height > 2048)
		{
			problem(source->file, source->line, "a frame is 1 to 2048 wide and high:", png);
			return group;
		}
		if (!png_frame_build(bitmap, tag_index, png, width, height))
		{
			problem(source->file, source->line, "could not make the texture of bitmap", source->name);
			return group;
		}
	}
	return group;
}

/* adds a conditional widget to the definition, if it has not got it */
static void conditional_add(struct ui_widget_definition *definition, long tag_index, long flags)
{
	struct ui_widget_conditional_reference *conditionals =
		(struct ui_widget_conditional_reference *)definition->conditional_widgets.address;
	struct ui_widget_conditional_reference *grown;
	long count = definition->conditional_widgets.count, index;

	for (index = 0; index < count; index++)
	{
		if (conditionals[index].widget_tag.index == tag_index)
			return;
	}
	grown = allocate((count + 1) * sizeof(*grown));
	if (!grown)
		return;
	/* (the game's memcpy asserts on a NULL source, even for nothing) */
	if (count)
		memcpy(grown, conditionals, count * sizeof(*grown));
	reference_set(&grown[count].widget_tag, UI_WIDGET_DEFINITION_TAG, tag_index);
	grown[count].flags = flags;
	definition->conditional_widgets.address = grown;
	definition->conditional_widgets.count = count + 1;
}

static void handler_build(struct ui_widget_event_handler_reference *handler, struct halo_menu_handler const *source,
	long event_type, struct ui_widget_definition *definition)
{
	char const *targets[5];
	long target_count = 0;

	handler->event_type = (short)event_type;
	reference_clear(&handler->widget_tag, UI_WIDGET_DEFINITION_TAG);
	reference_clear(&handler->sound_effect, SOUND_TAG);
	if (source->run)
	{
		handler->function = (short)function_index(source->run, source->file, source->line);
		SET_FLAG(handler->flags, _event_handler_run_function_bit, TRUE);
	}
	if (source->script)
	{
		strncpy(handler->script, source->script, sizeof(handler->script) - 1);
		SET_FLAG(handler->flags, _event_handler_run_scenario_script_bit, TRUE);
	}
	else if (source->label)
	{
		strncpy(handler->script, source->label, sizeof(handler->script) - 1);
	}
	if (source->open)
	{
		targets[target_count++] = source->open;
		SET_FLAG(handler->flags, _event_handler_open_widget_bit, TRUE);
	}
	if (source->replace)
	{
		targets[target_count++] = source->replace;
		SET_FLAG(handler->flags, _event_handler_replace_self_with_widget_bit, TRUE);
	}
	if (source->focus)
	{
		targets[target_count++] = source->focus;
		SET_FLAG(handler->flags, _event_handler_give_focus_to_widget_bit, TRUE);
	}
	if (source->back)
		SET_FLAG(handler->flags, _event_handler_go_back_to_previous_widget_bit, TRUE);
	if (source->branch)
		SET_FLAG(handler->flags, _event_handler_try_to_branch_on_failure_bit, TRUE);
	if (source->close)
	{
		if (!strcmp(source->close, "current"))
			SET_FLAG(handler->flags, _event_handler_close_current_widget_bit, TRUE);
		else if (!strcmp(source->close, "all"))
			SET_FLAG(handler->flags, _event_handler_close_all_widgets_bit, TRUE);
		else if (!strcmp(source->close, "other") && source->widget)
			SET_FLAG(handler->flags, _event_handler_close_other_widget_bit, TRUE);
		else
			problem(source->file, source->line, "close is \"current\", \"all\" or \"other\" (with widget=)",
				source->close);
	}
	if (source->reload)
	{
		if (!strcmp(source->reload, "self"))
			SET_FLAG(handler->flags, _event_handler_reload_self_bit, TRUE);
		else if (!strcmp(source->reload, "other") && source->widget)
			SET_FLAG(handler->flags, _event_handler_reload_other_widget_bit, TRUE);
		else
			problem(source->file, source->line, "reload is \"self\" or \"other\" (with widget=)", source->reload);
	}
	if (source->widget)
		targets[target_count++] = source->widget;
	if (target_count > 1)
	{
		long index;

		/* (several flags may share the one widget) */
		for (index = 1; index < target_count; index++)
		{
			if (strcmp(targets[index], targets[0]))
				problem(source->file, source->line, "a handler names one widget (open, replace, focus or widget)",
					NULL);
		}
	}
	if (target_count)
	{
		reference_set(&handler->widget_tag, UI_WIDGET_DEFINITION_TAG,
			tag_named(UI_WIDGET_DEFINITION_TAG, targets[0], source->file, source->line));
	}
	if (source->sound)
		reference_set(&handler->sound_effect, SOUND_TAG, map_tag(SOUND_TAG, source->sound, source->file, source->line));
	if (source->otherwise)
	{
		if (!source->run)
			problem(source->file, source->line, "otherwise needs run (the function whose failure it is for)", NULL);
		SET_FLAG(handler->flags, _event_handler_try_to_branch_on_failure_bit, TRUE);
		conditional_add(definition, tag_named(UI_WIDGET_DEFINITION_TAG, source->otherwise, source->file, source->line), 1);
	}
}

static void setting_add(struct halo_menu_widget const *source, long definition_index)
{
	struct pc_menu_setting *setting;

	menu_tags.settings = realloc(menu_tags.settings, (menu_tags.setting_count + 1) * sizeof(*menu_tags.settings));
	setting = &menu_tags.settings[menu_tags.setting_count++];
	memset(setting, 0, sizeof(*setting));
	setting->definition_index = definition_index;
	setting->setting = source->setting;
	setting->loaded_index = NONE;
	setting->value_count = spinner_split(source, TRUE, setting->values);
}

/* a font: large, small, terminal, or the map's by its path */
static long font_tag(char const *font, char const *file, long line)
{
	char const *path = !strcmp(font, "large") ? "ui\\large_ui" :
		!strcmp(font, "small") ? "ui\\small_ui" :
		!strcmp(font, "terminal") ? "ui\\interstate" : font;

	return map_tag(FONT_TAG, path, file, line);
}

static void *widget_build(long widget_index)
{
	struct halo_menus const *menus = build.menus;
	struct halo_menu_widget const *source = &menus->widgets[widget_index];
	struct ui_widget_definition *definition = allocate(sizeof(struct ui_widget_definition));
	char const *leaf = strrchr(source->name, '/') ? strrchr(source->name, '/') + 1 : source->name;
	long child, handler, input, conditional, replace, count;

	if (!definition)
		return NULL;
	strncpy(definition->name, leaf, sizeof(definition->name) - 1);
	definition->type = _widget_type_container;
	if (source->type)
	{
		long type = name_index(source->type, widget_type_names, NUMBEROF(widget_type_names));

		if (type == NONE)
			problem(source->file, source->line, "there is no widget type", source->type);
		else
			definition->type = (short)type;
	}
	definition->controller_index = 4;
	if (source->controller && strcmp(source->controller, "any"))
	{
		if (strlen(source->controller) != 1 || source->controller[0] < '1' || source->controller[0] > '4')
			problem(source->file, source->line, "controller is 1 to 4, or \"any\":", source->controller);
		else
			definition->controller_index = (short)(source->controller[0] - '1');
	}
	definition->bounds.y0 = (short)source->top;
	definition->bounds.x0 = (short)source->left;
	definition->bounds.y1 = (short)(source->top + (source->has_height ? source->height : 480));
	definition->bounds.x1 = (short)(source->left + (source->has_width ? source->width : 640));
	definition->flags = flags_parse(source->flags, widget_flag_names, NUMBEROF(widget_flag_names),
		source->file, source->line);
	/* (in a network game the clock runs on) */
	if (build.game_map)
		definition->flags &= ~FLAG(1); /* pause_game */
	definition->milliseconds_to_auto_close = source->auto_close;
	definition->auto_close_fade_time = source->auto_close_fade;
	reference_clear(&definition->background_bitmap, BITMAP_GROUP_TAG);
	if (source->bitmap)
	{
		reference_set(&definition->background_bitmap, BITMAP_GROUP_TAG,
			tag_named(BITMAP_GROUP_TAG, source->bitmap, source->file, source->line));
	}
	/* the game data inputs */
	for (count = 0, input = source->first_input; input != HALO_MENU_NONE; input = menus->inputs[input].next)
		count++;
	if (count)
	{
		struct ui_widget_game_data_input_reference *inputs = allocate(count * sizeof(*inputs));

		definition->game_data_inputs.count = count;
		definition->game_data_inputs.address = inputs;
		for (count = 0, input = source->first_input; inputs && input != HALO_MENU_NONE; input = menus->inputs[input].next)
		{
			struct halo_menu_input const *data = &menus->inputs[input];

			inputs[count++].function = (short)game_data_input_index(data->input, data->file, data->line);
		}
	}
	/* search and replace */
	for (count = 0, replace = source->first_replace; replace != HALO_MENU_NONE; replace = menus->replaces[replace].next)
		count++;
	if (count)
	{
		struct ui_widget_search_and_replace_reference *replaces = allocate(count * sizeof(*replaces));

		definition->search_and_replace_functions.count = count;
		definition->search_and_replace_functions.address = replaces;
		for (count = 0, replace = source->first_replace; replaces && replace != HALO_MENU_NONE;
			replace = menus->replaces[replace].next)
		{
			struct halo_menu_replace const *item = &menus->replaces[replace];
			long function = item->function ? name_index(item->function, replace_names, NUMBEROF(replace_names)) : 0;

			if (function == NONE)
				problem(item->file, item->line, "a replace function is none or controller:", item->function);
			strncpy(replaces[count].search_string, item->search, sizeof(replaces[count].search_string) - 1);
			/* (the PC version's own are none here) */
			replaces[count++].replace_function = (short)(function == 1 ? 1 : 0);
		}
	}
	/* the text */
	reference_clear(&definition->text_label_string_list, UNICODE_STRING_LIST_TAG);
	reference_clear(&definition->text_font, FONT_TAG);
	if ((source->text != NULL) + (source->strings != NULL) + (source->string_list != NULL) > 1)
		problem(source->file, source->line, "a widget has one of text, strings and string_list:", source->name);
	if (build.text_tags[widget_index] != NONE)
	{
		reference_set(&definition->text_label_string_list, UNICODE_STRING_LIST_TAG, build.text_tags[widget_index]);
	}
	else if (build.spinner_tags[widget_index] != NONE)
	{
		reference_set(&definition->text_label_string_list, UNICODE_STRING_LIST_TAG, build.spinner_tags[widget_index]);
		definition->list_flags |= FLAG(_list_items_generated_from_string_list_tag_bit);
	}
	else if (source->string_list)
	{
		reference_set(&definition->text_label_string_list, UNICODE_STRING_LIST_TAG,
			tag_named(UNICODE_STRING_LIST_TAG, source->string_list, source->file, source->line));
	}
	definition->string_list_index = (short)source->string_index;
	/* (our own text is white in the large font, unless given) */
	if (source->text || source->strings)
	{
		definition->text_color.alpha = definition->text_color.red = 1.0f;
		definition->text_color.green = definition->text_color.blue = 1.0f;
		if (!source->font)
			reference_set(&definition->text_font, FONT_TAG, font_tag("large", source->file, source->line));
	}
	if (source->color && !parse_color(source->color, &definition->text_color))
		problem(source->file, source->line, "a color is #RRGGBB or #AARRGGBB:", source->color);
	if (source->font)
		reference_set(&definition->text_font, FONT_TAG, font_tag(source->font, source->file, source->line));
	if (source->align)
	{
		long align = name_index(source->align, align_names, NUMBEROF(align_names));

		if (align == NONE)
			problem(source->file, source->line, "align is \"left\", \"right\" or \"center\":", source->align);
		else
			definition->justification = (short)align;
	}
	definition->text_box_flags = (word)flags_parse(source->text_flags, text_flag_names, NUMBEROF(text_flag_names),
		source->file, source->line);
	definition->horizontal_offset = (short)source->text_x;
	definition->vertical_offset = (short)source->text_y;
	/* the list */
	definition->list_flags |= flags_parse(source->list_flags, list_flag_names, NUMBEROF(list_flag_names),
		source->file, source->line);
	if (source->strings && (definition->type != _widget_type_spinner_list || source->first_child != HALO_MENU_NONE))
		problem(source->file, source->line, "strings are for a spinner with no children:", source->name);
	if (TEST_FLAG(definition->list_flags, _list_items_generated_from_string_list_tag_bit) &&
		(definition->type != _widget_type_spinner_list || source->first_child != HALO_MENU_NONE ||
			definition->text_label_string_list.index == NONE))
		problem(source->file, source->line, "items_from_strings is for a spinner with strings and no children:",
			source->name);
	if (source->setting)
	{
		if (!source->strings || !source->values)
			problem(source->file, source->line, "a setting needs strings and values:", source->name);
		else
			setting_add(source, build.widget_tags[widget_index]);
	}
	if (source->visible_rows)
	{
		if (definition->type != _widget_type_column_list || source->visible_rows < 1 || source->visible_rows > 64)
			problem(source->file, source->line, "visible_rows is 1 to 64, for a column list:", source->name);
		else
		{
			menu_tags.windows = realloc(menu_tags.windows, (menu_tags.window_count + 1) * sizeof(*menu_tags.windows));
			menu_tags.windows[menu_tags.window_count].tag_index = build.widget_tags[widget_index];
			menu_tags.windows[menu_tags.window_count++].rows = (short)source->visible_rows;
		}
	}
	reference_clear(&definition->list_header_bitmap, BITMAP_GROUP_TAG);
	reference_clear(&definition->list_footer_bitmap, BITMAP_GROUP_TAG);
	if (source->header_bitmap)
	{
		reference_set(&definition->list_header_bitmap, BITMAP_GROUP_TAG,
			tag_named(BITMAP_GROUP_TAG, source->header_bitmap, source->file, source->line));
	}
	if (source->footer_bitmap)
	{
		reference_set(&definition->list_footer_bitmap, BITMAP_GROUP_TAG,
			tag_named(BITMAP_GROUP_TAG, source->footer_bitmap, source->file, source->line));
	}
	parse_bounds(source->header_bounds, &definition->list_header_bounds, source->file, source->line);
	parse_bounds(source->footer_bounds, &definition->list_footer_bounds, source->file, source->line);
	reference_clear(&definition->extended_description_widget, UI_WIDGET_DEFINITION_TAG);
	if (source->description)
	{
		if (definition->type != _widget_type_column_list)
			problem(source->file, source->line, "a description is for a column list:", source->name);
		reference_set(&definition->extended_description_widget, UI_WIDGET_DEFINITION_TAG,
			tag_named(UI_WIDGET_DEFINITION_TAG, source->description, source->file, source->line));
	}
	/* the children */
	for (count = 0, child = source->first_child; child != HALO_MENU_NONE; child = menus->children[child].next)
		count++;
	if (definition->type == _widget_type_spinner_list && count != 0 && count != 1 && count != 3)
		problem(source->file, source->line, "a spinner has 0, 1 or 3 children:", source->name);
	if (count)
	{
		struct ui_widget_child_reference *children = allocate(count * sizeof(*children));

		definition->child_widgets.count = count;
		definition->child_widgets.address = children;
		for (count = 0, child = source->first_child; children && child != HALO_MENU_NONE; child = menus->children[child].next)
		{
			struct halo_menu_child const *entry = &menus->children[child];
			long tag_index = entry->nested != HALO_MENU_NONE ? build.widget_tags[entry->nested] :
				tag_named(UI_WIDGET_DEFINITION_TAG, entry->widget, entry->file, entry->line);
			char const *name = entry->nested != HALO_MENU_NONE ? menus->widgets[entry->nested].name : entry->widget;
			char const *child_leaf = strrchr(name, '/') ? strrchr(name, '/') + 1 : name;

			reference_set(&children[count].widget_tag, UI_WIDGET_DEFINITION_TAG, tag_index);
			strncpy(children[count].name, child_leaf, sizeof(children[count].name) - 1);
			children[count].horizontal_offset = (short)entry->x;
			children[count].vertical_offset = (short)entry->y;
			if (entry->controller)
			{
				if (strlen(entry->controller) != 1 || entry->controller[0] < '1' || entry->controller[0] > '4')
					problem(entry->file, entry->line, "a child's controller is 1 to 4:", entry->controller);
				SET_FLAG(children[count].flags, _child_widget_use_custom_controller_index_bit, TRUE);
				children[count].custom_controller_index = (short)(entry->controller[0] - '1');
			}
			count++;
		}
	}
	/* the conditional widgets */
	for (conditional = source->first_conditional; conditional != HALO_MENU_NONE;
		conditional = menus->conditionals[conditional].next)
	{
		struct halo_menu_conditional const *item = &menus->conditionals[conditional];

		conditional_add(definition, tag_named(UI_WIDGET_DEFINITION_TAG, item->widget, item->file, item->line),
			item->if_failed ? 1 : 0);
	}
	/* the event handlers: one for each of each <on>'s events */
	for (count = 0, handler = source->first_handler; handler != HALO_MENU_NONE; handler = menus->handlers[handler].next)
		count++;
	if (count)
	{
		long total = 0;

		/* (count the events) */
		for (handler = source->first_handler; handler != HALO_MENU_NONE; handler = menus->handlers[handler].next)
		{
			char const *text = menus->handlers[handler].event;

			while (text && *text)
			{
				long length;

				text += strspn(text, " \t\r\n");
				length = (long)strcspn(text, " \t\r\n");
				if (!length)
					break;
				total++;
				text += length;
			}
		}
		if (total)
		{
			struct ui_widget_event_handler_reference *handlers = allocate(total * sizeof(*handlers));

			definition->event_handlers.count = total;
			definition->event_handlers.address = handlers;
			for (count = 0, handler = source->first_handler; handlers && handler != HALO_MENU_NONE;
				handler = menus->handlers[handler].next)
			{
				struct halo_menu_handler const *on = &menus->handlers[handler];
				char const *text = on->event;

				while (text && *text)
				{
					char word[64];
					long length, event;

					text += strspn(text, " \t\r\n");
					length = (long)strcspn(text, " \t\r\n");
					if (!length)
						break;
					if (length >= (long)sizeof(word))
						length = sizeof(word) - 1;
					memcpy(word, text, length);
					word[length] = 0;
					text += strcspn(text, " \t\r\n");
					event = name_index(word, event_names, NUMBEROF(event_names));
					if (event == NONE)
						problem(on->file, on->line, "there is no event", word);
					handler_build(&handlers[count++], on, event, definition);
				}
			}
		}
	}
	return definition;
}

/* the tag table with count more entries, the existing ones as they are */
static struct cache_file_tag_instance *instances_grow(long count, long *first_index, long *first_salt)
{
	long existing, index, salt = 0;
	struct cache_file_tag_instance *instances = cache_files_tag_instances(&existing);
	struct cache_file_tag_instance *grown;

	if (!instances)
		return NULL;
	/* (every tag's absolute index fits a tag index's 16 bits, short of
	NONE's, and the table's size cannot wrap: the map's count was checked
	as it loaded, cache_files.c, and is again) */
	if (existing < 0 || count < 0 || existing > UNSIGNED_SHORT_MAX - count)
	{
		platform_log("menus: %ld tags and the map's %ld are too many for a tag table", count, existing);
		return NULL;
	}
	grown = allocate((existing + count) * sizeof(*grown));
	if (!grown)
		return NULL;
	memcpy(grown, instances, existing * sizeof(*grown));
	for (index = 0; index < existing; index++)
	{
		long instance_salt = (unsigned long)instances[index].tag_index >> 16;

		if (instance_salt > salt)
			salt = instance_salt;
	}
	menu_tags.original_instances = instances;
	menu_tags.original_count = existing;
	*first_index = existing;
	*first_salt = salt + 1;
	return grown;
}

/* the next of the tags counted for the table (build.total), or NONE past
them, which would be written past the table */
static long next_tag(void)
{
	long tag_index;

	if (build.next >= build.total)
	{
		platform_log("menus: a tag past the %ld counted for the table", build.total);
		return NONE;
	}
	tag_index = ((build.first_salt + build.next) << 16) | (build.first_index + build.next);
	build.next++;
	return tag_index;
}

static void instance_set(struct cache_file_tag_instance *instances, long group_tag, long tag_index, char const *name,
	char const *suffix, void *definition)
{
	struct cache_file_tag_instance *instance;
	char *copy;

	if (tag_index == NONE)
		return;
	instance = &instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index)];
	copy = allocate((long)strlen(PC_MENU_TAG_PREFIX) + (long)strlen(name) + (long)strlen(suffix) + 1);
	if (copy)
	{
		char *character;

		strcpy(copy, PC_MENU_TAG_PREFIX);
		strcat(copy, name);
		strcat(copy, suffix);
		/* (named as the map's tags are) */
		for (character = copy; *character; character++)
		{
			if (*character == '/')
				*character = '\\';
		}
	}
	instance->group_tag = group_tag;
	instance->parent_group_tags[0] = NONE;
	instance->parent_group_tags[1] = NONE;
	instance->tag_index = tag_index;
	/* (the name is read by tag_loaded: of none, when the copy failed, which
	fails the build) */
	instance->name = copy ? copy : "";
	instance->base_address = definition;
}

static void menu_tags_release(void)
{
	long index;

	for (index = 0; index < menu_tags.bitmap_count; index++)
	{
		if (menu_tags.bitmaps[index]->hardware_format)
			rasterizer_bitmap_delete(menu_tags.bitmaps[index]);
	}
	halo_menus_art_forget();
	if (menu_tags.original_instances)
		cache_files_set_tag_instances(menu_tags.original_instances, menu_tags.original_count);
	/* (the game's free takes no NULL: settings is, with no setting spinners) */
	for (index = 0; index < menu_tags.block_count; index++)
		free(menu_tags.blocks[index]);
	if (menu_tags.blocks)
		free(menu_tags.blocks);
	if (menu_tags.bitmaps)
		free(menu_tags.bitmaps);
	if (menu_tags.placements)
		free(menu_tags.placements);
	if (menu_tags.settings)
		free(menu_tags.settings);
	if (menu_tags.patch_indices)
		free(menu_tags.patch_indices);
	if (menu_tags.windows)
		free(menu_tags.windows);
	memset(&menu_tags, 0, sizeof(menu_tags));
}

/* ---------- public code */

/* ---------- the in-game pause menu (a multiplayer map's)

Its list (RESUME GAME, LEAVE GAME; a custom map's own buttons too) gets
SETTINGS, the menus' settings (as the main menu's SETTINGS opens them), and
for the host END GAME, which ends the game as its time limit would: its
players stay, and the host picks the next map after the carnage report.
They are copies of the map's LEAVE GAME button (its look), put before it.
The map's pause screens move to fit them: on the Xbox's own (its box of
pausebox2 pieces), the box is the port's taller one (pause/pausebox_*:
tools/port_settings.py), centred where the old one was; on a custom map's,
what is below the list moves down. */

/* the screen SETTINGS opens, by set: the PC version's profile edit screen,
or the Xbox look's own hub (a game map has not got ui.map's screens) */
#define PAUSE_SETTINGS_SCREEN_PC "main_menu/settings_select/player_setup/player_profile_edit/player_profile_edit_screen"
#define PAUSE_SETTINGS_SCREEN_XBOX "xbox/settings/settings_screen"
#define PAUSE_BUTTON_SPACING 35
#define PAUSE_BOX_FIRST_BUTTONS 3

static long ui_function_named(char const *name)
{
	long index;

	for (index = 0; index < 512; index++)
	{
		char const *function = ui_widget_event_handler_function_name(index);

		if (!function)
			break;
		if (!strcmp(function, name))
			return index;
	}
	return NONE;
}

static boolean tag_name_ends(long tag_index, char const *end)
{
	char const *name = tag_index == NONE ? NULL : tag_get_name(tag_index);
	size_t length = name ? strlen(name) : 0, end_length = strlen(end);

	return name && length >= end_length && !strcmp(name + length - end_length, end);
}

/* the index of the list's button that runs the function (LEAVE GAME's),
else NONE */
static long pause_quit_button(struct ui_widget_definition const *list, long quit_function)
{
	struct ui_widget_child_reference const *children = list->child_widgets.address;
	long child;

	for (child = 0; child < list->child_widgets.count; child++)
	{
		struct ui_widget_definition const *button;
		struct ui_widget_event_handler_reference const *handlers;
		long handler;

		if (children[child].widget_tag.index == NONE)
			continue;
		button = tag_get(UI_WIDGET_DEFINITION_TAG, children[child].widget_tag.index);
		handlers = button->event_handlers.address;
		for (handler = 0; handler < button->event_handlers.count; handler++)
		{
			if (handlers[handler].function == quit_function &&
				TEST_FLAG(handlers[handler].flags, _event_handler_run_function_bit))
			{
				return child;
			}
		}
	}
	return NONE;
}

/* a button like the model (its look), its own text, each of the model's
events (its A and Start) running the function and opening the widget (if
not NONE) */
static void *pause_button(struct cache_file_tag_instance *instances, struct ui_widget_definition const *model,
	char const *name, char const *text, long function, long widget_tag)
{
	struct ui_widget_definition *button = allocate(sizeof(*button));
	struct ui_widget_event_handler_reference *handlers =
		allocate(model->event_handlers.count * sizeof(struct ui_widget_event_handler_reference));
	long text_tag = next_tag(), handler;

	if (!button || !handlers || text_tag == NONE)
		return NULL;
	memcpy(button, model, sizeof(*button));
	instance_set(instances, UNICODE_STRING_LIST_TAG, text_tag, name, " text", string_list_build(&text, 1));
	reference_set(&button->text_label_string_list, UNICODE_STRING_LIST_TAG, text_tag);
	button->string_list_index = 0;
	button->game_data_inputs.count = 0;
	button->game_data_inputs.address = NULL;
	/* (the game's memcpy asserts on a NULL source, even for nothing) */
	if (model->event_handlers.count)
		memcpy(handlers, model->event_handlers.address, model->event_handlers.count * sizeof(*handlers));
	for (handler = 0; handler < model->event_handlers.count; handler++)
	{
		handlers[handler].flags = FLAG(_event_handler_run_function_bit);
		handlers[handler].function = (short)function;
		handlers[handler].script[0] = 0;
		reference_set(&handlers[handler].widget_tag, UI_WIDGET_DEFINITION_TAG, widget_tag);
		if (widget_tag != NONE)
		{
			handlers[handler].flags |= FLAG(_event_handler_open_widget_bit) |
				FLAG(_event_handler_try_to_branch_on_failure_bit);
		}
	}
	button->event_handlers.address = handlers;
	return button;
}

/* the list's buttons: SETTINGS (and the host's END GAME) put before LEAVE
GAME (quit); returns how many were added */
static long pause_list_patch(struct cache_file_tag_instance *instances, struct ui_widget_definition *list, long quit,
	boolean host)
{
	struct ui_widget_child_reference *children = list->child_widgets.address;
	long count = list->child_widgets.count, added = host ? 2 : 1, child;
	struct ui_widget_child_reference *grown = allocate((count + added) * sizeof(struct ui_widget_child_reference));
	struct ui_widget_definition const *model = tag_get(UI_WIDGET_DEFINITION_TAG, children[quit].widget_tag.index);
	long settings_widget = widget_named(build.xbox_set ? PAUSE_SETTINGS_SCREEN_XBOX : PAUSE_SETTINGS_SCREEN_PC);
	short spacing = count >= 2 ? (short)(children[count - 1].vertical_offset - children[count - 2].vertical_offset) :
		PAUSE_BUTTON_SPACING;
	void *buttons[2];
	long tags[2];

	if (!grown || settings_widget == NONE || spacing <= 0)
		return 0;
	tags[0] = next_tag();
	buttons[0] = pause_button(instances, model, "pause/settings_button", "SETTINGS",
		function_index("profile set edit begin", "pause", 0), build.widget_tags[settings_widget]);
	tags[1] = host ? next_tag() : NONE;
	buttons[1] = host ? pause_button(instances, model, "pause/end_game_button", "END GAME",
		function_index("port pause end game", "pause", 0), NONE) : NULL;
	if (tags[0] == NONE || !buttons[0] || (host && (tags[1] == NONE || !buttons[1])))
		return 0;
	for (child = 0; child < added; child++)
	{
		instance_set(instances, UI_WIDGET_DEFINITION_TAG, tags[child],
			child ? "pause/end_game_button" : "pause/settings_button", "", buttons[child]);
	}
	if (quit)
		memcpy(grown, children, quit * sizeof(*grown));
	for (child = 0; child < added; child++)
	{
		grown[quit + child] = children[quit];
		reference_set(&grown[quit + child].widget_tag, UI_WIDGET_DEFINITION_TAG, tags[child]);
		grown[quit + child].vertical_offset = (short)(children[quit].vertical_offset + child * spacing);
	}
	for (child = quit; child < count; child++)
	{
		grown[child + added] = children[child];
		grown[child + added].vertical_offset = (short)(children[child].vertical_offset + added * spacing);
	}
	list->child_widgets.address = grown;
	list->child_widgets.count = count + added;
	/* (the list draws within its bounds) */
	list->bounds.y1 = (short)(list->bounds.y1 + added * spacing);
	return added;
}

/* the Xbox's own box: a widget of the three pausebox2 pieces */
static boolean pause_box_stock(struct ui_widget_definition const *box)
{
	struct ui_widget_child_reference const *pieces = box->child_widgets.address;

	return box->child_widgets.count == 3 && tag_name_ends(pieces[0].widget_tag.index, "\\pausebox2_left") &&
		tag_name_ends(pieces[1].widget_tag.index, "\\pausebox2_left_center") &&
		tag_name_ends(pieces[2].widget_tag.index, "\\pausebox2_right");
}

/* the stock box's pieces drawn with the port's taller box: its frame for
the number of buttons */
static void pause_box_redraw(struct ui_widget_definition const *box, long buttons)
{
	static char const *const names[] = { "pause/pausebox_left", "pause/pausebox_center", "pause/pausebox_right" };
	struct ui_widget_child_reference const *pieces = box->child_widgets.address;
	long piece;

	for (piece = 0; piece < 3; piece++)
	{
		struct ui_widget_definition *definition = tag_get(UI_WIDGET_DEFINITION_TAG, pieces[piece].widget_tag.index);
		long bitmap = find(names[piece], build.menus->bitmap_count, bitmap_name);
		struct bitmap_group *group;
		struct bitmap_group_sequence *sequence;

		if (bitmap == NONE)
			continue;
		group = bitmap_group_get(build.bitmap_tags[bitmap]);
		sequence = group->sequences.address;
		/* (one frame: the one for the number of buttons) */
		sequence->first_bitmap_index = (short)PIN(buttons - PAUSE_BOX_FIRST_BUTTONS, 0, group->bitmaps.count - 1);
		sequence->bitmap_count = 1;
		reference_set(&definition->background_bitmap, BITMAP_GROUP_TAG, build.bitmap_tags[bitmap]);
		definition->bounds.y1 = (short)(definition->bounds.y0 + 256);
	}
}

static void pause_patch(struct cache_file_tag_instance *instances)
{
	long collection = tag_loaded('Soul', MULTIPLAYER_COLLECTION);
	long quit_function = ui_function_named("mp game player quit");
	boolean host = global_network_game_server_get() != NULL;
	struct tag_block const *screens;
	long patched_list = NONE, added = 0, buttons = 0, screen;
	boolean box_redrawn = FALSE;

	if (collection == NONE || quit_function == NONE)
		return;
	screens = tag_get('Soul', collection);
	for (screen = 0; screen < screens->count; screen++)
	{
		long screen_tag = ((struct tag_reference const *)screens->address)[screen].index;
		struct ui_widget_definition *definition;
		struct ui_widget_child_reference *children;
		long child, list_child = NONE, box_child = NONE;
		short grow, list_top;

		if (screen_tag == NONE)
			continue;
		definition = tag_get(UI_WIDGET_DEFINITION_TAG, screen_tag);
		children = definition->child_widgets.address;
		for (child = 0; child < definition->child_widgets.count && list_child == NONE; child++)
		{
			struct ui_widget_definition *list;
			long quit;

			if (children[child].widget_tag.index == NONE)
				continue;
			list = tag_get(UI_WIDGET_DEFINITION_TAG, children[child].widget_tag.index);
			if (list->type != _widget_type_column_list)
				continue;
			/* (one list, which the screens share: patched once) */
			if (children[child].widget_tag.index == patched_list)
			{
				list_child = child;
				continue;
			}
			quit = pause_quit_button(list, quit_function);
			if (quit == NONE || patched_list != NONE)
				continue;
			added = pause_list_patch(instances, list, quit, host);
			if (!added)
				return;
			buttons = list->child_widgets.count;
			patched_list = children[child].widget_tag.index;
			list_child = child;
		}
		if (list_child == NONE)
			continue;
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			if (child != list_child && children[child].widget_tag.index != NONE &&
				pause_box_stock(tag_get(UI_WIDGET_DEFINITION_TAG, children[child].widget_tag.index)))
			{
				box_child = child;
			}
		}
		/* the stock box: taller, centred where it was, the list with it, and
		what is below it moved down; else only what is below the list */
		grow = (short)(added * PAUSE_BUTTON_SPACING);
		list_top = children[list_child].vertical_offset;
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			if (box_child != NONE && (child == box_child || child == list_child))
				children[child].vertical_offset -= grow / 2;
			else if (children[child].vertical_offset > list_top)
				children[child].vertical_offset += box_child != NONE ? grow - grow / 2 : grow;
		}
		if (box_child != NONE && !box_redrawn)
		{
			pause_box_redraw(tag_get(UI_WIDGET_DEFINITION_TAG, children[box_child].widget_tag.index), buttons);
			box_redrawn = TRUE;
		}
	}
	if (patched_list != NONE)
		platform_log("menus: the pause menu has SETTINGS%s", host ? " and END GAME" : "");
}

/* ---------- the patch table (the Xbox look's, in ui.map)

Each <patch> (halo_menus.h) changes one of the map's column lists on one of
its screens: rows added after or before a row, taken out, or replaced. An
added or replacing row is a copy of a row of the map's (its look, bounds,
font, color and sounds) with its own name, its own text, and only A and
Start, which run its function (run=) and open its screen (open=), and
created if it has one; the copy is one of our tags, so it may run the port's
functions, which a widget of the map's may not.

The Xbox's lists keep a row's place in its own bounds, the child reference's
offset adding to it, so a row is at the list's offset plus its bounds' top
plus its offset. A row put after another is a row's pitch below it, the
pitch being how far that row is below the one before it (or pitch=); the
rows after it move down by the pitch, and so do the screen's children whose
top is at or below the new row, above the footer (footer=, else the screen's
a_butn) and left of the list's right edge (the separator lines, not the
legend or the profile's name). Taking a row out moves them up. The rows have
to fit above the footer.

The lists show a description and picture by the focused row's number. A
patched list's rows keep the numbers they had (pc_menu_patch_list_index); a
row with a description or picture gets the next number past the
description's strings and the picture's frames, its text and frame added to
grown copies of their blocks. picture= is a frame of the picture's own by
its number, or png: and one of our PNGs, drawn at its first frame's size.

Anything that does not resolve, does not fit or is not the shape the patch
expects is logged and that patch is not made: its list stays the map's. The
tags its rows would have used stay empty. */

#define PATCH_MAXIMUM_ROWS 64
#define PATCH_FOOTER_DEFAULT 480

/* a description panel's text and picture, and the number past both */
struct patch_panel
{
	boolean resolved;
	struct string_list *strings;
	struct bitmap_group *group;
	struct bitmap_group_sequence *sequence;
	long group_tag;
	short frames;
	short base;
};

/* a patch under way: the list's rows and the screen's children's offsets
as they will be, and what it makes, none of it written until it all holds */
struct patch_work
{
	struct halo_menu_patch const *patch;
	struct ui_widget_definition *screen, *list;
	long list_tag;
	long list_child;
	short list_x, list_y;
	struct ui_widget_child_reference rows[PATCH_MAXIMUM_ROWS];
	struct ui_widget_definition const *definitions[PATCH_MAXIMUM_ROWS];
	char const *names[PATCH_MAXIMUM_ROWS];
	/* each row's tag in the map (a replaced row's too), NONE for one added */
	long map_tags[PATCH_MAXIMUM_ROWS];
	short descriptions[PATCH_MAXIMUM_ROWS];
	long count;
	short *screen_y;
	short footer, right;
	/* the rows made: their tags, definitions, names and texts */
	long made_widgets[PATCH_MAXIMUM_ROWS], made_texts[PATCH_MAXIMUM_ROWS];
	struct ui_widget_definition *made_definitions[PATCH_MAXIMUM_ROWS];
	char const *made_names[PATCH_MAXIMUM_ROWS], *made_strings[PATCH_MAXIMUM_ROWS];
	long made_count;
	/* the help strings of the rows that are our widgets, added in order to the description's text */
	struct string_list *help_strings;
	char const *helps[PATCH_MAXIMUM_ROWS];
	long help_count;
	/* the descriptions and pictures added, from panel.base */
	struct patch_panel panel;
	char const *added_descriptions[PATCH_MAXIMUM_ROWS], *added_pictures[PATCH_MAXIMUM_ROWS];
	long added_count;
};

static boolean patch_fail(struct patch_work const *work, char const *file, long line, char const *message,
	char const *detail)
{
	halo_menus_log(file, line, message, detail);
	platform_log("menus: %s:%ld: the patch of %s is not made; the list stays the map's", work->patch->file,
		work->patch->line, work->patch->list);
	return FALSE;
}

/* a widget an open= names: ours by its name, or the map's by its path */
static long widget_tag_lookup(char const *name)
{
	long index;

	if (strchr(name, '\\'))
		return tag_loaded(UI_WIDGET_DEFINITION_TAG, name);
	index = widget_named(name);
	return index == NONE ? NONE : build.widget_tags[index];
}

static short patch_row_y(struct patch_work const *work, long row)
{
	return (short)(work->list_y + work->definitions[row]->bounds.y0 + work->rows[row].vertical_offset);
}

static short patch_row_x(struct patch_work const *work, long row)
{
	return (short)(work->list_x + work->definitions[row]->bounds.x0 + work->rows[row].horizontal_offset);
}

/* a row by the map's path (a replaced one too) or by a patch row's name,
or NONE */
static long patch_row_find(struct patch_work const *work, char const *name)
{
	long row, tag = strchr(name, '\\') ? tag_loaded(UI_WIDGET_DEFINITION_TAG, name) : NONE;

	for (row = 0; row < work->count; row++)
	{
		if (tag != NONE ? work->map_tags[row] == tag : work->names[row] && !strcmp(work->names[row], name))
			return row;
	}
	return NONE;
}

/* how far apart the rows are at row: the patch row's pitch=, else how far
it is below the row before it (or the second below the first), else its
height */
static short patch_pitch(struct patch_work const *work, struct halo_menu_patch_row const *source, long row,
	struct ui_widget_definition const *model)
{
	if (source->has_pitch)
		return (short)source->pitch;
	if (row >= 1 && row < work->count)
		return (short)(patch_row_y(work, row) - patch_row_y(work, row - 1));
	if (work->count >= 2)
		return (short)(patch_row_y(work, 1) - patch_row_y(work, 0));
	return (short)(model->bounds.y1 - model->bounds.y0);
}

/* moves the screen's children between top and the footer, left of the
list's right edge, by delta (at_top: those at top too) */
static void patch_screen_move(struct patch_work *work, short top, boolean at_top, short delta)
{
	struct ui_widget_child_reference const *children = work->screen->child_widgets.address;
	long child;

	for (child = 0; child < work->screen->child_widgets.count; child++)
	{
		struct ui_widget_definition const *definition;
		short y, x;

		if (child == work->list_child || children[child].widget_tag.index == NONE)
			continue;
		definition = tag_get(UI_WIDGET_DEFINITION_TAG, children[child].widget_tag.index);
		y = (short)(definition->bounds.y0 + work->screen_y[child]);
		x = (short)(definition->bounds.x0 + children[child].horizontal_offset);
		if ((y > top || (at_top && y == top)) && y < work->footer && x < work->right)
			work->screen_y[child] = (short)(work->screen_y[child] + delta);
	}
}

/* the list's description panel: its text's string list and its picture's
bitmap, each if it has one, and the number past both */
static boolean patch_panel_resolve(struct patch_work *work, struct halo_menu_patch_row const *source)
{
	struct patch_panel *panel = &work->panel;
	struct ui_widget_definition const *description;
	struct ui_widget_child_reference const *children;
	long child;
	short strings = 0;

	if (panel->resolved)
		return TRUE;
	if (work->list->extended_description_widget.index == NONE)
		return patch_fail(work, source->file, source->line, "the list has no description for", source->name);
	description = tag_get(UI_WIDGET_DEFINITION_TAG, work->list->extended_description_widget.index);
	children = description->child_widgets.address;
	for (child = 0; child < description->child_widgets.count; child++)
	{
		struct ui_widget_definition const *part;

		if (children[child].widget_tag.index == NONE)
			continue;
		part = tag_get(UI_WIDGET_DEFINITION_TAG, children[child].widget_tag.index);
		if (part->type == _widget_type_text_box && part->text_label_string_list.index != NONE && !panel->strings)
			panel->strings = tag_get(UNICODE_STRING_LIST_TAG, part->text_label_string_list.index);
		else if (part->type == _widget_type_container && part->background_bitmap.index != NONE && !panel->group)
		{
			panel->group_tag = part->background_bitmap.index;
			panel->group = bitmap_group_get(part->background_bitmap.index);
		}
	}
	if (panel->group)
	{
		/* (a frame is the first sequence's, which has to end the bitmaps for
		one to be added to it) */
		if (panel->group->sequences.count > 0)
		{
			panel->sequence = panel->group->sequences.address;
			if (panel->sequence->bitmap_count <= 0 ||
				panel->sequence->first_bitmap_index + panel->sequence->bitmap_count != panel->group->bitmaps.count)
			{
				return patch_fail(work, source->file, source->line,
					"the description's picture is not one sequence at the end of its bitmaps for", source->name);
			}
			panel->frames = panel->sequence->bitmap_count;
		}
		else
		{
			panel->frames = (short)panel->group->bitmaps.count;
		}
		if (panel->frames <= 0)
			return patch_fail(work, source->file, source->line, "the description's picture has no frames for",
				source->name);
	}
	if (panel->strings)
		strings = (short)panel->strings->strings.count;
	panel->base = strings > panel->frames ? strings : panel->frames;
	panel->resolved = TRUE;
	return TRUE;
}

/* a row like the model (its look, bounds, font, color and sounds), named
name, with its own text and only A and Start (running run, opening open)
and created (running created) if given: the model's other handlers, game
data inputs and conditional widgets are not the row's */
static struct ui_widget_definition *patch_row_build(struct ui_widget_definition const *model, char const *name,
	long text_tag, long run, long open, long created, long bitmap)
{
	struct ui_widget_definition *row = allocate(sizeof(*row));
	long count = created != NONE ? 3 : 2, handler;
	struct ui_widget_event_handler_reference *handlers = allocate(count * sizeof(*handlers));
	struct ui_widget_event_handler_reference const *model_handlers = model->event_handlers.address;
	struct tag_reference sound;
	short const events[3] =
	{
		(short)name_index("a", event_names, NUMBEROF(event_names)),
		(short)name_index("start", event_names, NUMBEROF(event_names)),
		(short)name_index("created", event_names, NUMBEROF(event_names)),
	};

	if (!row || !handlers)
		return NULL;
	memcpy(row, model, sizeof(*row));
	memset(row->name, 0, sizeof(row->name));
	strncpy(row->name, name, sizeof(row->name) - 1);
	reference_set_pending(&row->text_label_string_list, UNICODE_STRING_LIST_TAG, text_tag);
	/* (a picture of its own, which the main menu's labels are) */
	if (bitmap != NONE)
		reference_set(&row->background_bitmap, BITMAP_GROUP_TAG, bitmap);
	row->string_list_index = 0;
	row->game_data_inputs.count = 0;
	row->game_data_inputs.address = NULL;
	row->conditional_widgets.count = 0;
	row->conditional_widgets.address = NULL;
	/* (A's sound, the model's) */
	reference_clear(&sound, SOUND_TAG);
	for (handler = 0; handler < model->event_handlers.count; handler++)
	{
		if (model_handlers[handler].event_type == events[0])
		{
			sound = model_handlers[handler].sound_effect;
			break;
		}
	}
	for (handler = 0; handler < count; handler++)
	{
		struct ui_widget_event_handler_reference *made = &handlers[handler];

		made->event_type = events[handler];
		reference_clear(&made->widget_tag, UI_WIDGET_DEFINITION_TAG);
		reference_clear(&made->sound_effect, SOUND_TAG);
		if (handler == 2)
		{
			made->flags = FLAG(_event_handler_run_function_bit);
			made->function = (short)created;
			continue;
		}
		made->sound_effect = sound;
		if (run != NONE)
		{
			made->flags |= FLAG(_event_handler_run_function_bit);
			made->function = (short)run;
		}
		if (open != NONE)
		{
			made->flags |= FLAG(_event_handler_open_widget_bit);
			reference_set(&made->widget_tag, UI_WIDGET_DEFINITION_TAG, open);
			/* (opened only when the function succeeds) */
			if (run != NONE)
				made->flags |= FLAG(_event_handler_try_to_branch_on_failure_bit);
		}
	}
	row->event_handlers.count = count;
	row->event_handlers.address = handlers;
	return row;
}

/* a <row> or <replace>: its tags, its functions and screen, and its
definition, made (not yet in the table) */
static struct ui_widget_definition *patch_row_make(struct patch_work *work, struct halo_menu_patch_row const *source,
	struct ui_widget_definition const *model, long *widget_tag)
{
	long run = NONE, open = NONE, created = NONE, text_tag, bitmap = NONE;
	struct ui_widget_definition *definition;

	if (patch_row_find(work, source->name) != NONE)
		return patch_fail(work, source->file, source->line, "two rows are named", source->name), NULL;
	if (source->run && (run = function_lookup(source->run)) == NONE)
		return patch_fail(work, source->file, source->line, "there is no event handler function", source->run), NULL;
	if (source->created && (created = function_lookup(source->created)) == NONE)
	{
		return patch_fail(work, source->file, source->line, "there is no event handler function", source->created),
			NULL;
	}
	if (source->open && (open = widget_tag_lookup(source->open)) == NONE)
		return patch_fail(work, source->file, source->line, "there is no widget named", source->open), NULL;
	if (source->bitmap)
	{
		long index = find(source->bitmap, build.menus->bitmap_count, bitmap_name);

		bitmap = index == NONE ? NONE : build.bitmap_tags[index];
	}
	if (source->bitmap && bitmap == NONE)
		return patch_fail(work, source->file, source->line, "there is no bitmap named", source->bitmap), NULL;
	if (work->made_count >= PATCH_MAXIMUM_ROWS)
		return patch_fail(work, source->file, source->line, "a patch makes too many rows:", source->name), NULL;
	*widget_tag = next_tag();
	text_tag = next_tag();
	if (*widget_tag == NONE || text_tag == NONE)
		return patch_fail(work, source->file, source->line, "there is no tag left for", source->name), NULL;
	definition = patch_row_build(model, source->name, text_tag, run, open, created, bitmap);
	if (!definition)
		return patch_fail(work, source->file, source->line, "out of memory for", source->name), NULL;
	work->made_widgets[work->made_count] = *widget_tag;
	work->made_texts[work->made_count] = text_tag;
	work->made_definitions[work->made_count] = definition;
	work->made_names[work->made_count] = source->name;
	work->made_strings[work->made_count] = source->text;
	work->made_count++;
	return definition;
}

/* the map's widget a row is modeled on, by its path */
static struct ui_widget_definition const *patch_model(struct patch_work const *work,
	struct halo_menu_patch_row const *source)
{
	long tag = tag_loaded(UI_WIDGET_DEFINITION_TAG, source->model);

	if (tag == NONE)
		return patch_fail(work, source->file, source->line, "the map has no widget", source->model), NULL;
	return tag_get(UI_WIDGET_DEFINITION_TAG, tag);
}

static boolean patch_row_add(struct patch_work *work, struct halo_menu_patch_row const *source)
{
	char const *anchor_name = source->after ? source->after : source->before;
	long anchor = patch_row_find(work, anchor_name), at, row, widget_tag;
	struct ui_widget_definition const *model;
	struct ui_widget_definition *definition;
	struct ui_widget_child_reference reference;
	short pitch, y, x, description = NONE;

	if (anchor == NONE)
		return patch_fail(work, source->file, source->line, "the list has no row", anchor_name);
	if (work->count >= PATCH_MAXIMUM_ROWS)
		return patch_fail(work, source->file, source->line, "the list has too many rows for", source->name);
	/* (after the row, or before it: after the one before it) */
	at = source->after ? anchor + 1 : anchor;
	anchor = at - 1;
	model = source->model ? patch_model(work, source) : work->definitions[anchor >= 0 ? anchor : at];
	if (!model)
		return FALSE;
	pitch = patch_pitch(work, source, anchor >= 0 ? anchor : at, model);
	if (pitch <= 0)
		return patch_fail(work, source->file, source->line, "the rows are not one below another at", anchor_name);
	y = anchor >= 0 ? (short)(patch_row_y(work, anchor) + pitch) : patch_row_y(work, at);
	x = patch_row_x(work, anchor >= 0 ? anchor : at);
	if (source->widget)
	{
		/* (one of our widgets as it is: no copy, no tags of its own, no description) */
		if (patch_row_find(work, source->name) != NONE)
			return patch_fail(work, source->file, source->line, "two rows are named", source->name);
		widget_tag = strchr(source->widget, '\\') ? NONE : widget_tag_lookup(source->widget);
		definition = widget_tag == NONE ? NULL : tag_get(UI_WIDGET_DEFINITION_TAG, widget_tag);
		if (!definition)
			return patch_fail(work, source->file, source->line, "there is no widget named", source->widget);
		if (source->help)
		{
			/* (a list whose description is one text, which its rows' values' strings follow in turn: those of a
			row added at the end go after the map's) */
			struct ui_widget_definition const *text = work->list->extended_description_widget.index == NONE ? NULL :
				tag_get(UI_WIDGET_DEFINITION_TAG, work->list->extended_description_widget.index);

			if (!text || text->type != _widget_type_text_box || text->text_label_string_list.index == NONE)
				return patch_fail(work, source->file, source->line, "the list's description is not one text for",
					source->name);
			work->help_strings = tag_get(UNICODE_STRING_LIST_TAG, text->text_label_string_list.index);
			work->helps[work->help_count++] = source->help;
		}
	}
	else
	{
		if (source->description || source->picture)
		{
			if (!patch_panel_resolve(work, source))
				return FALSE;
			if (source->description && !work->panel.strings)
				return patch_fail(work, source->file, source->line, "the list's description has no text for", source->name);
			if (work->panel.group && !source->picture)
				return patch_fail(work, source->file, source->line, "the list's description has a picture: give one for",
					source->name);
			if (source->picture && !work->panel.group)
				return patch_fail(work, source->file, source->line, "the list's description has no picture for", source->name);
			if (source->picture && strncmp(source->picture, "png:", 4) && atol(source->picture) >= work->panel.frames)
				return patch_fail(work, source->file, source->line, "the picture has no such frame:", source->picture);
			if (work->panel.base + work->added_count > 32767)
				return patch_fail(work, source->file, source->line, "too many descriptions for", source->name);
			description = (short)(work->panel.base + work->added_count);
		}
		definition = patch_row_make(work, source, model, &widget_tag);
		if (!definition)
			return FALSE;
	}
	if (description != NONE)
	{
		work->added_descriptions[work->added_count] = source->description;
		work->added_pictures[work->added_count] = source->picture;
		work->added_count++;
	}
	/* (as the row it is put by, at its place) */
	reference = work->rows[anchor >= 0 ? anchor : at];
	if (source->widget)
		reference_set(&reference.widget_tag, UI_WIDGET_DEFINITION_TAG, widget_tag);
	else
		reference_set_pending(&reference.widget_tag, UI_WIDGET_DEFINITION_TAG, widget_tag);
	memset(reference.name, 0, sizeof(reference.name));
	strncpy(reference.name, source->name, sizeof(reference.name) - 1);
	reference.vertical_offset = (short)(y - work->list_y - definition->bounds.y0);
	reference.horizontal_offset = (short)(x - work->list_x - definition->bounds.x0);
	for (row = at; row < work->count; row++)
		work->rows[row].vertical_offset = (short)(work->rows[row].vertical_offset + pitch);
	patch_screen_move(work, y, TRUE, pitch);
	for (row = work->count; row > at; row--)
	{
		work->rows[row] = work->rows[row - 1];
		work->definitions[row] = work->definitions[row - 1];
		work->names[row] = work->names[row - 1];
		work->map_tags[row] = work->map_tags[row - 1];
		work->descriptions[row] = work->descriptions[row - 1];
	}
	work->rows[at] = reference;
	work->definitions[at] = definition;
	work->names[at] = source->name;
	work->map_tags[at] = NONE;
	work->descriptions[at] = description;
	work->count++;
	return TRUE;
}

static boolean patch_row_remove(struct patch_work *work, struct halo_menu_patch_row const *source)
{
	long removed = patch_row_find(work, source->child), row;
	short pitch, y;

	if (removed == NONE)
		return patch_fail(work, source->file, source->line, "the list has no row", source->child);
	pitch = patch_pitch(work, source, removed, work->definitions[removed]);
	if (pitch <= 0)
		return patch_fail(work, source->file, source->line, "the rows are not one below another at", source->child);
	y = patch_row_y(work, removed);
	for (row = removed; row + 1 < work->count; row++)
	{
		work->rows[row] = work->rows[row + 1];
		work->definitions[row] = work->definitions[row + 1];
		work->names[row] = work->names[row + 1];
		work->map_tags[row] = work->map_tags[row + 1];
		work->descriptions[row] = work->descriptions[row + 1];
		work->rows[row].vertical_offset = (short)(work->rows[row].vertical_offset - pitch);
	}
	work->count--;
	patch_screen_move(work, y, FALSE, (short)-pitch);
	return TRUE;
}

static boolean patch_row_replace(struct patch_work *work, struct halo_menu_patch_row const *source)
{
	long replaced = patch_row_find(work, source->child), widget_tag;
	struct ui_widget_definition const *model;
	struct ui_widget_definition *definition;
	short y, x;

	if (replaced == NONE)
		return patch_fail(work, source->file, source->line, "the list has no row", source->child);
	model = source->model ? patch_model(work, source) : work->definitions[replaced];
	if (!model)
		return FALSE;
	y = patch_row_y(work, replaced);
	x = patch_row_x(work, replaced);
	definition = patch_row_make(work, source, model, &widget_tag);
	if (!definition)
		return FALSE;
	/* (in its place, keeping its description) */
	reference_set_pending(&work->rows[replaced].widget_tag, UI_WIDGET_DEFINITION_TAG, widget_tag);
	memset(work->rows[replaced].name, 0, sizeof(work->rows[replaced].name));
	strncpy(work->rows[replaced].name, source->name, sizeof(work->rows[replaced].name) - 1);
	work->rows[replaced].vertical_offset = (short)(y - work->list_y - definition->bounds.y0);
	work->rows[replaced].horizontal_offset = (short)(x - work->list_x - definition->bounds.x0);
	work->definitions[replaced] = definition;
	work->names[replaced] = source->name;
	return TRUE;
}

/* the text, as a string list entry of ours */
static boolean patch_string_set(struct string_list_entry *entry, char const *text)
{
	long length = (long)strlen(text);
	unsigned short *characters = allocate((length * 2 + 2) * sizeof(unsigned short));

	if (!characters)
		return FALSE;
	entry->string.size = halo_menus_utf16(text, characters, length * 2 + 2) * (long)sizeof(unsigned short);
	entry->string.address = characters;
	return TRUE;
}

/* the panel's strings and frames, grown to the added ones' numbers: those
between its own and panel.base empty (strings) or its first frame */
static boolean patch_panel_write(struct patch_work *work)
{
	struct patch_panel const *panel = &work->panel;
	long count = panel->base + work->added_count, index;

	if (panel->strings)
	{
		struct string_list_entry *entries = allocate(count * sizeof(*entries));
		long own = panel->strings->strings.count;

		if (!entries)
			return FALSE;
		if (own)
			memcpy(entries, panel->strings->strings.address, own * sizeof(*entries));
		for (index = own; index < count; index++)
		{
			char const *text = index >= panel->base ? work->added_descriptions[index - panel->base] : NULL;

			if (!patch_string_set(&entries[index], text ? text : ""))
				return FALSE;
		}
		panel->strings->strings.address = entries;
		panel->strings->strings.count = count;
	}
	if (panel->group)
	{
		long first = panel->sequence ? panel->sequence->first_bitmap_index : 0;
		long own = panel->group->bitmaps.count, total = first + count;
		struct bitmap_data *bitmaps = allocate(total * sizeof(*bitmaps));
		struct bitmap_data const *frames = panel->group->bitmaps.address;

		if (!bitmaps)
			return FALSE;
		memcpy(bitmaps, frames, own * sizeof(*bitmaps));
		for (index = first + panel->frames; index < total; index++)
		{
			char const *picture = index - first >= panel->base ?
				work->added_pictures[index - first - panel->base] : NULL;

			/* (a frame of its own: a copy, which the texture cache loads as
			its own; or one of our PNGs, at its first frame's size) */
			if (picture && !strncmp(picture, "png:", 4))
			{
				memset(&bitmaps[index], 0, sizeof(bitmaps[index]));
				if (!png_frame_build(&bitmaps[index], panel->group_tag, picture + 4, frames[first].width,
					frames[first].height))
				{
					return FALSE;
				}
				continue;
			}
			bitmaps[index] = frames[first + (picture ? atol(picture) : 0)];
			bitmaps[index].cache_block_index = NONE;
			bitmaps[index].base_address = NULL;
		}
		/* (each frame's texture is loaded again, from its new place) */
		for (index = 0; index < own; index++)
		{
			bitmaps[index].cache_block_index = NONE;
			bitmaps[index].base_address = NULL;
		}
		panel->group->bitmaps.address = bitmaps;
		panel->group->bitmaps.count = total;
		if (panel->sequence)
			panel->sequence->bitmap_count = (short)count;
	}
	return TRUE;
}

/* the help strings of the rows that are our widgets, added after the description text's own, in order */
static boolean patch_help_write(struct patch_work *work)
{
	long own = work->help_strings->strings.count, count = own, index, piece;
	struct string_list_entry *entries;

	for (index = 0; index < work->help_count; index++)
	{
		count++;
		for (char const *bar = strchr(work->helps[index], '|'); bar; bar = strchr(bar + 1, '|'))
			count++;
	}
	entries = allocate(count * sizeof(*entries));
	if (!entries)
		return FALSE;
	if (own)
		memcpy(entries, work->help_strings->strings.address, own * sizeof(*entries));
	for (index = 0, piece = own; index < work->help_count; index++)
	{
		char const *start = work->helps[index], *bar;

		do
		{
			char text[256];
			size_t length;

			bar = strchr(start, '|');
			length = bar ? (size_t)(bar - start) : strlen(start);
			if (length >= sizeof(text) || !(memcpy(text, start, length), text[length] = '\0', patch_string_set(&entries[piece++], text)))
				return FALSE;
			start = bar ? bar + 1 : NULL;
		} while (start);
	}
	work->help_strings->strings.address = entries;
	work->help_strings->strings.count = count;
	return TRUE;
}

/* one patch: worked out whole, then written (the list's rows, the
screen's children's offsets, the rows' tags, the description's strings and
frames), or not made at all */
static void patch_apply(struct cache_file_tag_instance *instances, struct halo_menu_patch const *patch)
{
	struct halo_menus const *menus = build.menus;
	struct patch_work *work = malloc(sizeof(*work));
	struct ui_widget_child_reference *children, *grown;
	long screen_tag, row, child, made, bottom;

	if (!work)
	{
		platform_log("menus: out of memory for the patch of %s", patch->list);
		return;
	}
	memset(work, 0, sizeof(*work));
	work->patch = patch;
	screen_tag = tag_loaded(UI_WIDGET_DEFINITION_TAG, patch->screen);
	work->list_tag = tag_loaded(UI_WIDGET_DEFINITION_TAG, patch->list);
	if (screen_tag == NONE || work->list_tag == NONE)
	{
		patch_fail(work, patch->file, patch->line, "the map has no widget",
			screen_tag == NONE ? patch->screen : patch->list);
		goto done;
	}
	for (made = 0; made < menu_tags.patch_index_count; made++)
	{
		if (menu_tags.patch_indices[made].list_tag == work->list_tag)
		{
			patch_fail(work, patch->file, patch->line, "a list is patched once:", patch->list);
			goto done;
		}
	}
	work->screen = tag_get(UI_WIDGET_DEFINITION_TAG, screen_tag);
	work->list = tag_get(UI_WIDGET_DEFINITION_TAG, work->list_tag);
	children = work->screen->child_widgets.address;
	work->list_child = NONE;
	for (child = 0; child < work->screen->child_widgets.count && work->list_child == NONE; child++)
	{
		if (children[child].widget_tag.index == work->list_tag)
			work->list_child = child;
	}
	if (work->list_child == NONE || work->list->type != _widget_type_column_list ||
		work->list->child_widgets.count > PATCH_MAXIMUM_ROWS || work->list->child_widgets.count < 1)
	{
		patch_fail(work, patch->file, patch->line, "the screen has no column list (of 1 to 64 rows)", patch->list);
		goto done;
	}
	work->list_x = children[work->list_child].horizontal_offset;
	work->list_y = children[work->list_child].vertical_offset;
	/* (the game's malloc, which takes no 0) */
	work->screen_y = malloc((work->screen->child_widgets.count + 1) * sizeof(*work->screen_y));
	if (!work->screen_y)
		goto done;
	work->footer = patch->has_footer ? (short)patch->footer : PATCH_FOOTER_DEFAULT;
	for (child = 0; child < work->screen->child_widgets.count; child++)
	{
		work->screen_y[child] = children[child].vertical_offset;
		if (!patch->has_footer)
		{
			long tag = children[child].widget_tag.index;
			struct ui_widget_definition const *definition;

			if (tag == NONE)
				continue;
			definition = tag_get(UI_WIDGET_DEFINITION_TAG, tag);
			if (tag_name_ends(tag, "\\a_butn"))
			{
				long top = definition->bounds.y0 + children[child].vertical_offset;

				if (top < work->footer)
					work->footer = (short)top;
			}
			else if (definition->type == _widget_type_container)
			{
				struct ui_widget_child_reference const *parts = definition->child_widgets.address;

				/* The Xbox's button_key and button_key_options keep the legend one level down. */
				for (long part = 0; part < definition->child_widgets.count; part++)
				{
					if (parts[part].widget_tag.index != NONE && tag_name_ends(parts[part].widget_tag.index, "\\a_butn"))
					{
						struct ui_widget_definition const *button = tag_get(UI_WIDGET_DEFINITION_TAG, parts[part].widget_tag.index);
						long top = children[child].vertical_offset + parts[part].vertical_offset + button->bounds.y0;

						if (top < work->footer)
							work->footer = (short)top;
					}
				}
			}
		}
	}
	/* the rows, each with its own number */
	work->count = work->list->child_widgets.count;
	memcpy(work->rows, work->list->child_widgets.address, work->count * sizeof(*work->rows));
	work->right = -32768;
	for (row = 0; row < work->count; row++)
	{
		short right;

		if (work->rows[row].widget_tag.index == NONE)
		{
			patch_fail(work, patch->file, patch->line, "the list has a row of no widget:", patch->list);
			goto done;
		}
		work->definitions[row] = tag_get(UI_WIDGET_DEFINITION_TAG, work->rows[row].widget_tag.index);
		work->map_tags[row] = work->rows[row].widget_tag.index;
		work->descriptions[row] = (short)row;
		right = (short)(work->list_x + work->definitions[row]->bounds.x1 + work->rows[row].horizontal_offset);
		if (right > work->right)
			work->right = right;
	}
	for (row = patch->first_row; row < patch->first_row + patch->row_count; row++)
	{
		struct halo_menu_patch_row const *source = &menus->patch_rows[row];
		boolean done = source->kind == HALO_MENU_PATCH_ROW ? patch_row_add(work, source) :
			source->kind == HALO_MENU_PATCH_REMOVE ? patch_row_remove(work, source) :
			patch_row_replace(work, source);

		if (!done)
			goto done;
	}
	/* (the rows above the footer) */
	for (row = 0, bottom = -32768; row < work->count; row++)
	{
		long row_bottom = patch_row_y(work, row) + work->definitions[row]->bounds.y1 - work->definitions[row]->bounds.y0;

		if (row_bottom > bottom)
			bottom = row_bottom;
	}
	if (bottom > work->footer)
	{
		char detail[32];

		snprintf(detail, sizeof(detail), "%ld > %d", bottom, work->footer);
		patch_fail(work, patch->file, patch->line, "the rows pass the footer:", detail);
		goto done;
	}
	/* written: the rows' tags first, which the list names */
	if ((work->added_count && !patch_panel_write(work)) || (work->help_count && !patch_help_write(work)))
	{
		patch_fail(work, patch->file, patch->line, "out of memory for the description of", patch->list);
		goto done;
	}
	grown = allocate(work->count * sizeof(*grown));
	{
		struct menu_patch_index *indices = realloc(menu_tags.patch_indices,
			(menu_tags.patch_index_count + work->count) * sizeof(*indices));

		if (!grown || !indices)
		{
			if (indices)
				menu_tags.patch_indices = indices;
			patch_fail(work, patch->file, patch->line, "out of memory for", patch->list);
			goto done;
		}
		menu_tags.patch_indices = indices;
	}
	for (made = 0; made < work->made_count; made++)
	{
		char name[48];

		/* (pc\patch\<name>) */
		snprintf(name, sizeof(name), "patch/%s", work->made_names[made]);
		instance_set(instances, UNICODE_STRING_LIST_TAG, work->made_texts[made], name, " text",
			string_list_build(&work->made_strings[made], 1));
		instance_set(instances, UI_WIDGET_DEFINITION_TAG, work->made_widgets[made], name, "",
			work->made_definitions[made]);
	}
	memcpy(grown, work->rows, work->count * sizeof(*grown));
	work->list->child_widgets.address = grown;
	work->list->child_widgets.count = work->count;
	/* (the list draws within its bounds) */
	if (bottom - work->list_y > work->list->bounds.y1)
		work->list->bounds.y1 = (short)(bottom - work->list_y);
	for (child = 0; child < work->screen->child_widgets.count; child++)
		children[child].vertical_offset = work->screen_y[child];
	for (row = 0; row < work->count; row++)
	{
		struct menu_patch_index *index = &menu_tags.patch_indices[menu_tags.patch_index_count++];

		index->list_tag = work->list_tag;
		index->row_tag = work->rows[row].widget_tag.index;
		index->description = work->descriptions[row];
	}
	platform_log("menus: %s: %ld rows patched (%ld made)", patch->list, work->count, work->made_count);

done:
	/* (the game's free, which takes no NULL) */
	if (work->screen_y)
		free(work->screen_y);
	free(work);
}

/* the patch table's tags: a widget and its text for each <row> and
<replace> (none for a <remove>) */
static long patch_tag_count(struct halo_menus const *menus)
{
	long row, count = 0;

	for (row = 0; row < menus->patch_row_count; row++)
		count += menus->patch_rows[row].kind == HALO_MENU_PATCH_REMOVE || menus->patch_rows[row].widget ? 0 : 2;
	return count;
}

void menu_tags_loaded(
	char const *map_name)
{
	struct halo_menus const *menus;
	struct cache_file_tag_instance *instances;
	long widget_count, own_lists = 0, total, index;
	/* (the Xbox look's patches of ui.map's lists) */
	boolean patching;

	boolean game_map = strcmp(map_name, "ui") != 0;

	/* (ui.map, and a multiplayer map: its pause menu's SETTINGS) */
	if (game_map && tag_loaded('Soul', MULTIPLAYER_COLLECTION) == NONE)
		return;
	menus = halo_menus_load();
	if (!menus)
		return;
	memset(&build, 0, sizeof(build));
	build.menus = menus;
	build.game_map = game_map;
	build.xbox_set = strcmp(config_string("display.menus"), "pc") != 0;
	patching = !game_map && build.xbox_set && menus->patch_count;
	widget_count = menus->widget_count;
	build.widget_tags = malloc((widget_count + 1) * sizeof(long));
	build.text_tags = malloc((widget_count + 1) * sizeof(long));
	build.spinner_tags = malloc((widget_count + 1) * sizeof(long));
	build.bitmap_tags = malloc((menus->bitmap_count + 1) * sizeof(long));
	build.strings_tags = malloc((menus->string_list_count + 1) * sizeof(long));
	if (!build.widget_tags || !build.text_tags || !build.spinner_tags || !build.bitmap_tags || !build.strings_tags)
		goto failed;
	for (index = 0; index < widget_count; index++)
	{
		own_lists += (menus->widgets[index].text != NULL) + (menus->widgets[index].strings != NULL);
		/* (names are unique) */
		if (widget_named(menus->widgets[index].name) != index)
			problem(menus->widgets[index].file, menus->widgets[index].line, "two widgets are named",
				menus->widgets[index].name);
	}
	total = widget_count + own_lists + menus->string_list_count + menus->bitmap_count +
		(game_map ? PAUSE_PATCH_TAGS : 0) + (patching ? patch_tag_count(menus) : 0);
	build.total = total;
	instances = build.failed ? NULL : instances_grow(total, &build.first_index, &build.first_salt);
	if (!instances)
		goto failed;
	/* each new tag's index */
	for (index = 0; index < widget_count; index++)
	{
		build.widget_tags[index] = next_tag();
		build.text_tags[index] = menus->widgets[index].text ? next_tag() : NONE;
		build.spinner_tags[index] = menus->widgets[index].strings ? next_tag() : NONE;
	}
	for (index = 0; index < menus->string_list_count; index++)
		build.strings_tags[index] = next_tag();
	for (index = 0; index < menus->bitmap_count; index++)
		build.bitmap_tags[index] = next_tag();
	/* the tags: the bitmaps and strings first, which the widgets name; the
	widgets' names in the table before any is built, which they find there */
	for (index = 0; index < menus->bitmap_count && !build.failed; index++)
	{
		instance_set(instances, BITMAP_GROUP_TAG, build.bitmap_tags[index], menus->bitmaps[index].name, "",
			bitmap_build(&menus->bitmaps[index], build.bitmap_tags[index]));
	}
	for (index = 0; index < menus->string_list_count && !build.failed; index++)
	{
		struct halo_menu_strings const *list = &menus->string_lists[index];

		instance_set(instances, UNICODE_STRING_LIST_TAG, build.strings_tags[index], list->name, "",
			string_list_build(menus->strings + list->first, list->count));
	}
	for (index = 0; index < widget_count && !build.failed; index++)
	{
		struct halo_menu_widget const *widget = &menus->widgets[index];

		if (widget->text)
		{
			instance_set(instances, UNICODE_STRING_LIST_TAG, build.text_tags[index], widget->name, " text",
				string_list_build(&widget->text, 1));
		}
		if (widget->strings)
		{
			char const *pieces[MAXIMUM_STRINGS];
			long count = spinner_split(widget, FALSE, pieces);
			struct string_list *list = string_list_build(pieces, count);

			if (widget->setting && !strcmp(widget->setting, "display.frame_rate") &&
				count > 0 && !build.failed && pc_menu_display_rate_refresh() > 0)
			{
				struct string_list_entry *entries = list->strings.address;

				entries[0].string.address = display_rate_unicode;
				entries[0].string.size = sizeof(display_rate_unicode);
			}
			instance_set(instances, UNICODE_STRING_LIST_TAG, build.spinner_tags[index], widget->name, " strings", list);
		}
		instance_set(instances, UI_WIDGET_DEFINITION_TAG, build.widget_tags[index], widget->name, "", NULL);
	}
	if (build.failed)
		goto failed;
	/* (pause_patch's and the patches', until they make them: nothing a name
	finds) */
	for (index = build.first_index + build.next; index < build.first_index + total; index++)
	{
		instances[index].group_tag = NONE;
		instances[index].tag_index = NONE;
		instances[index].name = "";
	}
	cache_files_set_tag_instances(instances, build.first_index + total);
	for (index = 0; index < widget_count && !build.failed; index++)
		instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(build.widget_tags[index])].base_address = widget_build(index);
	if (build.failed)
		goto failed;
	if (game_map)
	{
		pause_patch(instances);
	}
	else if (build.xbox_set)
	{
		/* (the Xbox look's root is the map's own main menu) */
		menu_tags.root[0] = '\0';
		for (index = 0; patching && index < menus->patch_count && !build.failed; index++)
			patch_apply(instances, &menus->patches[index]);
	}
	else if (widget_named(menus->root) != NONE)
	{
		snprintf(menu_tags.root, sizeof(menu_tags.root), "%s%s", PC_MENU_TAG_PREFIX, menus->root);
		for (index = 0; menu_tags.root[index]; index++)
		{
			if (menu_tags.root[index] == '/')
				menu_tags.root[index] = '\\';
		}
	}
	else
	{
		platform_log("menus: there is no widget named %s, the main menu", menus->root);
	}
	if (build.failed)
		goto failed;
	/* (the tags made: a patch not made used fewer than were counted) */
	if (build.next > total)
	{
		platform_log("menus: %ld tags made, %ld counted", build.next, total);
		goto failed;
	}
	cache_files_set_tag_instances(instances, build.first_index + build.next);
	menu_tags.loaded = TRUE;
	menu_tags.xbox_set = build.xbox_set;
	platform_log("menus: %ld widgets, %ld string lists and %ld bitmaps added to the map's %ld tags",
		widget_count, own_lists + menus->string_list_count, menus->bitmap_count, build.first_index);
	goto done;

failed:
	platform_log("menus: not added; using the game's own menus");
	menu_tags_release();

done:
	free(build.widget_tags);
	free(build.text_tags);
	free(build.spinner_tags);
	free(build.strings_tags);
	free(build.bitmap_tags);
	memset(&build, 0, sizeof(build));
}

void menu_tags_unloaded(
	void)
{
	if (menu_tags.loaded || menu_tags.block_count)
		menu_tags_release();
}

char const *pc_menus_root_name(
	void)
{
	/* (debug.menu_open: a screen to see. An XML screen by its name,
	main_menu/settings_select/..., with a profile being edited for the
	settings' screens; or a widget of the map's by its path, with its
	backslashes, which the game loads itself, with the player's profile
	being edited, or the first gametype for the gametype editors) */
	char const *open = config_string("debug.menu_open");

	if (*open)
	{
		static char name[300];
		extern boolean pc_menu_profile_edit_begin(void);
		extern boolean pc_menu_gametype_edit_begin(void);

		if (strchr(open, '\\'))
		{
			snprintf(name, sizeof(name), "%s", open);
			if (strstr(name, "\\playlist_edit\\"))
				pc_menu_gametype_edit_begin();
			else
				pc_menu_profile_edit_begin();
			platform_log("menus: debug.menu_open: the map's %s", name);
			return name;
		}
		if (menu_tags.loaded && build.menus == NULL)
		{
			long index;

			snprintf(name, sizeof(name), "%s%s", PC_MENU_TAG_PREFIX, open);
			for (index = 0; name[index]; index++)
			{
				if (name[index] == '/')
					name[index] = '\\';
			}
			if (tag_loaded(UI_WIDGET_DEFINITION_TAG, name) != NONE)
			{
				pc_menu_profile_edit_begin();
				return name;
			}
		}
		platform_log("menus: debug.menu_open: there is no widget named %s", open);
	}
	return menu_tags.loaded && menu_tags.root[0] ? menu_tags.root : "ui\\shell\\main_menu\\main_menu";
}

/* whether the list is one a patch changed (the patch table), and then the
row's description's number: the number it had among the map's rows, a
patch row's own, or NONE (none: its description stays as it was) */
boolean pc_menu_patch_list_index(
	long list_tag,
	long row_tag,
	short *description)
{
	long index;
	boolean patched = FALSE;

	*description = NONE;
	for (index = 0; menu_tags.loaded && list_tag != NONE && index < menu_tags.patch_index_count; index++)
	{
		if (menu_tags.patch_indices[index].list_tag != list_tag)
			continue;
		patched = TRUE;
		if (row_tag != NONE && menu_tags.patch_indices[index].row_tag == row_tag)
		{
			*description = menu_tags.patch_indices[index].description;
			break;
		}
	}
	return patched;
}

boolean pc_menu_tag(
	long tag_index)
{
	return menu_tags.loaded && tag_index != NONE &&
		DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index) >= menu_tags.original_count;
}

/* how many rows of a menu file's column list show at once
(visible_rows=); 0: all of them (ui_widget.c scrolls the rest in) */
short pc_menu_list_window(
	long tag_index)
{
	long index;

	for (index = 0; menu_tags.loaded && tag_index != NONE && index < menu_tags.window_count; index++)
	{
		if (menu_tags.windows[index].tag_index == tag_index)
			return menu_tags.windows[index].rows;
	}
	return 0;
}

/* where in its widget, and how large, a frame of ui.map's that a menu file
scales is drawn (<frame map=... width= height= x= y=>); FALSE for any other
bitmap, drawn as it is (ui_widget.c) */
boolean pc_menu_frame_placement(
	struct bitmap_data const *bitmap,
	short *x,
	short *y,
	short *width,
	short *height)
{
	long index;

	for (index = 0; index < menu_tags.placement_count; index++)
	{
		if (menu_tags.placements[index].bitmap == bitmap)
		{
			*x = menu_tags.placements[index].x;
			*y = menu_tags.placements[index].y;
			*width = menu_tags.placements[index].width;
			*height = menu_tags.placements[index].height;
			return TRUE;
		}
	}
	return FALSE;
}

char const *pc_menu_function_name(
	long function_index)
{
	return function_index >= 0 && function_index < (long)NUMBEROF(port_function_names) ?
		port_function_names[function_index] : NULL;
}

/* a widget definition's string_index (port settings help: its label's) */
short pc_menu_string_index(
	long definition_index)
{
	struct ui_widget_definition *definition = tag_get(UI_WIDGET_DEFINITION_TAG, definition_index);

	return definition ? definition->string_list_index : NONE;
}

/* the screen the engine opens by name after a network game (the Xbox's),
or ours where the PC version's menus are up: the host's map select, the
lobby */
char const *pc_menus_screen(
	char const *name)
{
	static struct
	{
		char const *game_name;
		char const *menu_name;
	} const screens[] =
	{
		{ "ui\\shell\\main_menu\\multiplayer_type_select\\connected\\connected_map_select_postgame_wrapper",
			"pc\\main_menu\\multiplayer_type_select\\connected\\connected_map_select_wrapper" },
		{ "ui\\shell\\main_menu\\multiplayer_type_select\\connected\\pregame\\connected_pregame_screen",
			"pc\\main_menu\\multiplayer_type_select\\lobby\\lobby_screen" },
	};
	/* (the Xbox look's lobby, for games that are not system link) */
	static char const xbox_lobby[] = "pc\\xbox\\main_menu\\multiplayer_type_select\\lobby\\lobby_screen";
	short index;

	if (!menu_tags.loaded)
		return name;
	if (menu_tags.xbox_set)
	{
		/* (the Xbox's screens, but the pregame of an Internet or Direct Link game: the lobby, which has the
		invites, the password and the players to add; system link keeps the Xbox's) */
		if (!strcmp(name, screens[1].game_name) && !pc_menu_multiplayer_lan() &&
			tag_loaded(UI_WIDGET_DEFINITION_TAG, xbox_lobby) != NONE)
			return xbox_lobby;
		return name;
	}
	for (index = 0; index < NUMBEROF(screens); index++)
	{
		if (!strcmp(name, screens[index].game_name))
			return screens[index].menu_name;
	}
	return name;
}

char const *pc_menu_game_data_input_name(
	long function_index)
{
	return function_index >= 0 && function_index < (long)NUMBEROF(port_game_data_input_names) ?
		port_game_data_input_names[function_index] : NULL;
}

struct pc_menu_setting *pc_menu_setting_get(
	long definition_index)
{
	long index;

	pc_menu_display_rate_refresh();
	for (index = 0; index < menu_tags.setting_count; index++)
	{
		if (menu_tags.settings[index].definition_index == definition_index)
			return &menu_tags.settings[index];
	}
	return NULL;
}
