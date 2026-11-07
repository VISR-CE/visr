"""Exercise the graphics modes' menu rows with the production functions of port/linux/game/menu_functions.c: the
hub's mode word, the CRT effect's row shown only in Hardcore, the settings Hardcore replaces reading HARDCORE with
their rows selectable, and the help of the mode shown."""
from pathlib import Path
import subprocess

from test_menu_cancel import block

ROOT = Path(__file__).resolve().parents[1]

PROGRAM = r'''
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* (the game's wide characters are 16 bits) */
#define wchar_t unsigned short
#define _stricmp strcasecmp
#include <strings.h>
typedef int boolean;
typedef unsigned short word;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define MAXIMUM_STRINGS 64
#define NUMBEROF(a) ((long)(sizeof(a) / sizeof((a)[0])))
#include "PORT_GRAPHICS_MODES"
struct widget_instance
{
    long definition_tag_index;
    char const *name;
    short type, local_player_index;
    boolean visible, disabled;
    struct widget_instance *next, *previous, *parent, *child, *focused_child;
    struct { short current_frame_index, first_frame_index, last_frame_index, number_of_sprite_frames; } animation;
    union
    {
        struct { wchar_t *text; short string_list_index; } text_box;
        struct { short selected_index; short last_list_tab_direction; void *list_items; word number_of_items;
                 struct widget_instance *extended_description; wchar_t *item_text; } list;
    } parameters;
};
struct pc_menu_setting { long definition_index; char const *setting; long value_count;
                         char const *values[MAXIMUM_STRINGS]; short loaded_index; };
static struct pc_menu_setting settings[16];
static int setting_total;
static struct pc_menu_setting *setting_for(struct widget_instance const *spinner);
struct pc_menu_setting *pc_menu_setting_get(long definition_index)
{
    for (int i = 0; i < setting_total; i++)
        if (settings[i].definition_index == definition_index) return &settings[i];
    return NULL;
}
/* the game's: a mode in force and what it replaces */
static int stored_mode = GRAPHICS_MODE_ENHANCED;
int config_graphics_mode(void) { return stored_mode; }
char const *config_graphics_mode_name(void) { return graphics_mode_names[stored_mode]; }
char const *config_graphics_replaces(char const *name)
{
    return graphics_mode_value(stored_mode, name) ? graphics_mode_names[stored_mode] : NULL;
}
/* the game's settings: a profile that sets some, and what the player chose */
static char const *profile_sets_name, *profile_campaign, *explicit_name;
int config_profile_sets(char const *name) { return profile_sets_name && !strcmp(name, profile_sets_name); }
int config_explicit(char const *name) { return explicit_name && !strcmp(name, explicit_name); }
int config_default(char const *name, char *text, unsigned int size)
{
    (void)name; snprintf(text, size, "%s", profile_campaign ? profile_campaign : ""); return 1;
}
int config_text(char const *name, char *text, unsigned int size) { (void)name; snprintf(text, size, "stereo"); return 1; }
static short text_value_of(wchar_t const *line, char *out)
{
    short n = 0;
    for (; line[n]; n++) out[n] = (char)line[n];
    out[n] = 0;
    /* (the help's line breaks are read as the spaces they replaced) */
    for (short i = 0; i < n; i++) if (out[i] == '\n' || out[i] == '\r') out[i] = ' ';
    return n;
}
static void *ui_widget_realloc(void *pointer, size_t size, char const *file, int line)
{ (void)file; (void)line; return realloc(pointer, size); }
static void ustrncpy(wchar_t *out, wchar_t const *text, short length)
{ short i = 0; for (; i < length && text[i]; i++) out[i] = text[i]; for (; i < length; i++) out[i] = 0; }
static boolean spinner_item(struct widget_instance const *widget)
{
    return widget->parent && widget->parent->definition_tag_index == widget->definition_tag_index;
}
static struct widget_instance *screen_of(struct widget_instance *widget)
{
    while (widget->parent) widget = widget->parent;
    return widget;
}
#define _ui_widget_type_spinner_list 2
#define _ui_widget_type_column_list 5
#define _widget_pass_unhandled_events_to_children_bit 0
#define _widget_dpad_updown_tabs_thru_children_bit 1
#define _widget_dpad_leftright_tabs_thru_children_bit 2
#define _ui_audio_feedback_cursor 1
#define TEST_FLAG(f, b) (((f) >> (b)) & 1)
#define _list_items_generated_from_string_list_tag 0
#define _error_silent 0
#define _ui_audio_feedback_flag_failure 4
#define _gamepad_analog_button_a 0
#define _event_type_button 3
#define _event_type_left_stick 1
#define _widget_event_dpad_left 10
#define _widget_event_dpad_right 11
#define match_assert(file, line, condition) assert(condition)
#define match_vassert(file, line, condition, ...) assert(condition)
struct event_record { short type; union { struct { short index, value; } button; struct { short x, y; } stick; } data; };
struct ui_widget_definition { long flags, list_flags; struct { long count; } event_handlers, child_widgets; };
static int refusals;
static void ui_play_audio_feedback_sound(short sound) { if (sound == 4) refusals++; else assert(sound == 1); }
static void error(int level, char const *message, ...) { abort(); }
static struct widget_instance *widget_instance_get_topmost_parent(struct widget_instance *widget)
{ return screen_of(widget); }
static struct widget_instance *widget_instance_get_nth_child(struct widget_instance *widget, long index)
{ struct widget_instance *child = widget->child; while (child && index--) child = child->next; return child; }
static struct ui_widget_definition definitions[64];
static struct ui_widget_definition *ui_widget_definition_get(long tag) { return &definitions[tag]; }
static boolean pc_menu_tag(long tag) { (void)tag; return TRUE; }
static struct widget_instance *widget_instance_get_tail_child_widget(struct widget_instance *widget)
{
    struct widget_instance *child = widget->child;
    while (child && child->next) child = child->next;
    return child;
}
/* PRODUCTION */

static struct widget_instance widgets[64];
static struct pc_menu_setting *setting_for(struct widget_instance const *spinner)
{
    return pc_menu_setting_get(spinner->definition_tag_index);
}
static int widget_total;
static struct widget_instance *make(char const *name, struct widget_instance *parent, short type)
{
    struct widget_instance *widget = &widgets[widget_total];
    memset(widget, 0, sizeof(*widget));
    widget->definition_tag_index = widget_total++;
    widget->name = name;
    widget->type = type;
    widget->visible = TRUE;
    widget->animation.number_of_sprite_frames = 2;
    definitions[widget->definition_tag_index].flags = 1;
    widget->parent = parent;
    if (parent)
    {
        struct widget_instance **tail = &parent->child;
        while (*tail) { widget->previous = *tail; tail = &(*tail)->next; }
        *tail = widget;
    }
    return widget;
}
static struct widget_instance *spinner(struct widget_instance *row, char const *name, char const *setting,
                                       char const *values)
{
    struct widget_instance *widget = make(name, row, 2);
    struct pc_menu_setting *entry = &settings[setting_total++];
    char *copy = strdup(values);
    memset(entry, 0, sizeof(*entry));
    entry->definition_index = widget->definition_tag_index;
    entry->setting = setting;
    for (char *piece = strtok(copy, "|"); piece; piece = strtok(NULL, "|")) entry->values[entry->value_count++] = piece;
    return widget;
}

int main(void)
{
    struct widget_instance *screen = make("graphics_screen", NULL, 0);
    struct widget_instance *list = make("options_menu", screen, 5);
    struct widget_instance *help = make("help", screen, 1);
    struct widget_instance *mode_help = make("graphics_help", screen, 1);
    struct widget_instance *op_mode = make("op_graphics", list, 0);
    struct widget_instance *mode = spinner(op_mode, "graphics_spinner", "display.graphics", "hardcore|classic|enhanced");
    struct widget_instance *op_crt = make("op_crt_effect", list, 0);
    struct widget_instance *op_rate = make("op_frame_rate", list, 0);
    struct widget_instance *rate = spinner(op_rate, "frame_rate_spinner", "display.frame_rate", "display|30|60");
    struct widget_instance *rate_replaced = make("frame_rate_replaced", op_rate, 1);
    struct widget_instance *op_sync = make("op_vsync", list, 0);
    struct widget_instance *sync = spinner(op_sync, "vsync_spinner", "display.vsync", "true|false");
    wchar_t text[16];
    struct widget_instance *row_help = make("row_help", screen, 1);
    struct widget_instance *op_motion = make("op_interpolation", list, 0);
    struct widget_instance *motion = spinner(op_motion, "interpolation_spinner", "display.interpolation", "true|false");
    struct widget_instance *motion_replaced = make("interpolation_replaced", op_motion, 1);
    char line[300];

    list->parameters.list.extended_description = help;
    list->focused_child = op_mode;
    spinner(op_crt, "crt_effect_spinner", "display.crt_effect", "true|false");

    /* the hub's status line: "Graphics: <Mode>", the word as a name (Hardcore) */
    for (stored_mode = 0; stored_mode < NUMBER_OF_GRAPHICS_MODES; stored_mode++)
    {
        struct widget_instance *state = make("graphics_label", NULL, 1);
        graphics_state_show(state);
        for (int i = 0; i < 16; i++) text[i] = state->parameters.text_box.text[i];
        for (char const *expected = graphics_mode_names[stored_mode]; *expected; expected++)
            assert(text[expected - graphics_mode_names[stored_mode]] ==
                (wchar_t)(expected == graphics_mode_names[stored_mode] ? toupper(*expected) : *expected));
    }
    /* the word replaces the profile's name while GRAPHICS is the hub's row chosen */
    {
        struct widget_instance *hub = make("settings_screen", NULL, 0), *hub_list = make("settings_list", hub, 5);
        struct widget_instance *name = make("profile_label", hub, 1), *word = make("graphics_label", hub, 1);
        struct widget_instance *controls = make("controls", hub_list, 1), *graphics = make("graphics", hub_list, 1);

        hub_list->focused_child = controls;
        hub_status_show(hub_list);
        assert(name->visible && !word->visible);
        hub_list->focused_child = graphics;
        hub_status_show(hub_list);
        assert(!name->visible && word->visible);
    }

    /* Enhanced, stored and shown: the CRT row hidden, the frame rate row ordinary */
    stored_mode = GRAPHICS_MODE_ENHANCED;
    mode->parameters.list.selected_index = 2;
    list->focused_child = op_rate;
    graphics_rows_show(list);
    assert(!op_crt->visible);
    assert(rate->visible && !rate_replaced->visible);
    assert(sync->visible);
    /* the help is the row's own while another row is chosen, the mode's while the mode's is */
    assert(help->visible && !mode_help->visible);
    list->focused_child = op_mode;
    graphics_rows_show(list);
    assert(!help->visible && mode_help->visible && mode_help->parameters.text_box.string_list_index == 2);
    list->focused_child = op_rate;
    graphics_rows_show(list);
    assert(help->visible && !mode_help->visible);

    /* Hardcore chosen on the screen but not saved: the rows show it already (the table's, not the game's) */
    mode->parameters.list.selected_index = 0;
    graphics_rows_show(list);
    assert(op_crt->visible);
    /* (the value in force, as the spinner names it: 30 is the second of display|30|60; OFF the second of true|false) */
    assert(!rate->visible && rate_replaced->visible && !op_rate->disabled);
    assert(rate_replaced->parameters.text_box.string_list_index == 1);
    assert(!op_motion->disabled);
    assert(!motion->visible && motion_replaced->visible && motion_replaced->parameters.text_box.string_list_index == 1);
    /* Event dispatch updates the bitmap BEFORE navigation; render-time help must agree with its outline. */
    list->focused_child = op_mode;
    list->parameters.list.selected_index = 0;
    for (int i = 0; i < 10; i++)
    {
        column_list_update(list, ui_widget_definition_get(list->definition_tag_index));
        if (i < 5) widget_instance_tab_to_next_valid_widget(list);
        else widget_instance_tab_to_previous_valid_widget(list);
        graphics_rows_show(list);
        for (struct widget_instance *row = list->child; row; row = row->next)
            assert(row->animation.current_frame_index == (row == list->focused_child));
        assert(row_help->visible == (list->focused_child == op_rate || list->focused_child == op_motion));
    }
    /* Child-tab navigation must preserve both the row and its physical index, including wrap-around. */
    list->focused_child = op_crt;
    list->parameters.list.selected_index = 1;
    widget_instance_tab_to_next_valid_widget(list);
    assert(list->focused_child == op_rate && list->parameters.list.selected_index == 2);
    widget_instance_tab_to_next_valid_widget(list);
    assert(list->focused_child == op_sync && list->parameters.list.selected_index == 3);
    widget_instance_tab_to_next_valid_widget(list);
    assert(list->focused_child == op_motion && list->parameters.list.selected_index == 4);
    widget_instance_tab_to_next_valid_widget(list);
    assert(list->focused_child == op_mode && list->parameters.list.selected_index == 0);
    widget_instance_tab_to_previous_valid_widget(list);
    assert(list->focused_child == op_motion && list->parameters.list.selected_index == 4);
    widget_instance_tab_to_previous_valid_widget(list);
    widget_instance_tab_to_previous_valid_widget(list);
    assert(list->focused_child == op_rate && list->parameters.list.selected_index == 2);
    /* Mix pointer focus, child tabs and item steps: the old stale index wrapped early to the first row. */
    list->parameters.list.number_of_items = 5;
    ui_mouse_give_focus(op_motion);
    widget_instance_tab_to_previous_valid_widget(list);
    widget_instance_tab_to_previous_valid_widget(list);
    {
        boolean deleted = FALSE;
        widget_event_function_list_widget_goto_next_item(list, NULL, &deleted);
        assert(list->focused_child == op_sync && list->parameters.list.selected_index == 3);
    }
    /* Pointer hover and tap focus the row even though its spinner is hidden. */
    for (struct widget_instance *target = op_rate; target; target = target == op_rate ? op_motion : NULL)
    {
        assert(ui_mouse_widget_is_item(target));
        ui_mouse_give_focus(target);
        assert(list->focused_child == target);
        assert(list->parameters.list.selected_index == (target == op_rate ? 2 : 4));
        graphics_rows_show(list);
        assert(row_help->visible && !help->visible);
        assert(target->animation.current_frame_index == 1);
        assert(op_sync->animation.current_frame_index == 0);
    }
    /* Even a direct spinner step (including A-to-cycle handlers) cannot wrap a locked value. */
    definitions[rate->definition_tag_index].list_flags = 1;
    for (int i = 0; i < 3; i++)
    {
        boolean deleted = FALSE;
        struct event_record event = { .type = _event_type_button, .data.button = { _widget_event_dpad_right, 1 } };
        rate->parameters.list.number_of_items = 3;
        rate->parameters.list.selected_index = i;
        assert(widget_instance_port_refuse_graphics_edit(op_rate, &event));
        assert(widget_event_function_list_widget_goto_next_item(rate, &event, &deleted));
        assert(rate->parameters.list.selected_index == i);
        assert(widget_event_function_list_widget_goto_previous_item(rate, &event, &deleted));
        assert(rate->parameters.list.selected_index == i && !deleted);
        event.data.button.index = _gamepad_analog_button_a;
        assert(widget_instance_port_refuse_graphics_edit(op_rate, &event));
        event.data.button.index = _widget_event_dpad_left;
        assert(widget_instance_port_refuse_graphics_edit(op_rate, &event));
        event.type = _event_type_left_stick;
        event.data.stick.x = -32768;
        assert(widget_instance_port_refuse_graphics_edit(op_rate, &event));
        event.data.stick.x = 0;
        event.data.stick.y = 32767;
        assert(!widget_instance_port_refuse_graphics_edit(op_rate, &event));
        assert(!ui_widget_port_graphics_locked(op_sync));
    }
    assert(refusals == 18);
    assert(ui_widget_port_graphics_locked(motion));
    definitions[motion->definition_tag_index].list_flags = 1;
    motion->parameters.list.number_of_items = 2;
    for (int i = 0; i < 2; i++)
    {
        boolean deleted = FALSE;
        motion->parameters.list.selected_index = i;
        widget_event_function_list_widget_goto_next_item(motion, NULL, &deleted);
        widget_event_function_list_widget_goto_previous_item(motion, NULL, &deleted);
        assert(motion->parameters.list.selected_index == i);
    }
    assert(refusals == 22);
    /* vertical sync is no setting the mode replaces */
    assert(sync->visible && !op_sync->disabled);
    /* a fixed row can be chosen, for its help, and says what it is fixed to */
    list->focused_child = op_rate;
    graphics_rows_show(list);
    assert(row_help->visible && !help->visible && !mode_help->visible);
    text_value_of(row_help->parameters.text_box.text, line);
    assert(!strcmp(line, "Fixed to 30 FPS in Hardcore mode."));
    list->focused_child = op_motion;
    graphics_rows_show(list);
    text_value_of(row_help->parameters.text_box.text, line);
    assert(!strcmp(line, "Frames are drawn at the game's 30 ticks a second in Hardcore mode."));
    list->focused_child = op_sync;
    graphics_rows_show(list);
    assert(!row_help->visible && help->visible);
    list->focused_child = op_mode;
    graphics_rows_show(list);
    assert(mode_help->visible && mode_help->parameters.text_box.string_list_index == 0);

    /* Hardcore saved: the game's own answer */
    stored_mode = GRAPHICS_MODE_HARDCORE;
    graphics_rows_show(list);
    assert(op_crt->visible && !rate->visible && rate_replaced->visible);

    /* Classic replaces none of these rows, and hides the CRT effect's */
    mode->parameters.list.selected_index = 1;
    stored_mode = GRAPHICS_MODE_CLASSIC;
    list->focused_child = op_crt;
    /* Hiding a focused CRT row moves focus during drawing, after the animation update. */
    column_list_update(list, ui_widget_definition_get(list->definition_tag_index));
    graphics_rows_show(list);
    assert(op_mode->animation.current_frame_index == 1 && op_crt->animation.current_frame_index == 0);
    assert(!op_crt->visible && list->focused_child == op_mode && list->parameters.list.selected_index == 0);
    assert(rate->visible && !rate_replaced->visible && !op_rate->disabled);
    assert(mode_help->parameters.text_box.string_list_index == 1);

    assert(!ui_widget_port_graphics_locked(rate));
    {
        boolean deleted = FALSE;
        rate->parameters.list.selected_index = 2;
        definitions[rate->definition_tag_index].list_flags = 1;
        widget_event_function_list_widget_goto_next_item(rate, NULL, &deleted);
        assert(rate->parameters.list.selected_index == 0);
        widget_event_function_list_widget_goto_previous_item(rate, NULL, &deleted);
        assert(rate->parameters.list.selected_index == 2);
    }
    /* a setting the device's profile sets (display|30|60: campaign 60) and the player has not chosen shows the
    profile's campaign value, with a help that says so; the rows are not fixed */
    profile_sets_name = "display.frame_rate";
    profile_campaign = "60";
    rate->parameters.list.selected_index = 0;
    setting_for(rate)->loaded_index = 0;
    list->focused_child = op_rate;
    setenv("HALO_HOST_PHONE_SCALE", "3.00", 1);
    graphics_rows_show(list);
    assert(rate->visible && rate->parameters.list.selected_index == 2 && setting_for(rate)->loaded_index == 2);
    assert(row_help->visible && !help->visible);
    text_value_of(row_help->parameters.text_box.text, line);
    assert(!strncmp(line, "Optimized setting for your iPhone. Certain modes like", 53));
    assert(strstr(line, "Multiplayer or Co-Op may change this for better performance."));
    unsetenv("HALO_HOST_PHONE_SCALE");
    graphics_rows_show(list);
    text_value_of(row_help->parameters.text_box.text, line);
    assert(strstr(line, "your iPad."));
    setenv("HALO_HOST_TVOS", "1", 1);
    graphics_rows_show(list);
    text_value_of(row_help->parameters.text_box.text, line);
    assert(strstr(line, "your Apple TV."));
    unsetenv("HALO_HOST_TVOS");
    /* once the player turns it, the chosen value shows with the row's normal help */
    rate->parameters.list.selected_index = 1;
    graphics_rows_show(list);
    assert(rate->parameters.list.selected_index == 1 && !row_help->visible && help->visible);
    /* and a value the player chose earlier (config_explicit) is left as it was loaded */
    rate->parameters.list.selected_index = 1;
    setting_for(rate)->loaded_index = 1;
    explicit_name = "display.frame_rate";
    graphics_rows_show(list);
    assert(rate->parameters.list.selected_index == 1 && !row_help->visible);
    explicit_name = NULL;
    profile_sets_name = NULL;

    /* on visionOS the Hardcore help carries the note (the string after the modes') */
    setenv("HALO_HOST_VISIONOS", "1", 1);
    mode->parameters.list.selected_index = 0;
    stored_mode = GRAPHICS_MODE_HARDCORE;
    list->focused_child = op_mode;
    graphics_rows_show(list);
    assert(mode_help->parameters.text_box.string_list_index == 3);
    mode->parameters.list.selected_index = 2;
    graphics_rows_show(list);
    assert(mode_help->parameters.text_box.string_list_index == 2);
    unsetenv("HALO_HOST_VISIONOS");

    /* a screen without the mode's row is left alone */
    {
        struct widget_instance *other = make("options_menu", NULL, 5);
        struct widget_instance *row = make("op_frame_rate", other, 0);
        graphics_rows_show(other);
        assert(row->visible && !row->disabled);
    }
    return 0;
}
'''


def test_the_graphics_rows_follow_the_mode(tmp_path):
    source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    functions = '\n'.join(block(source, marker) for marker in (
        'static void text_set_length(', 'static struct widget_instance *descendant(', 'static struct widget_instance *named(',
        'static boolean text_is_number(', 'static short setting_value_index(',
        'static void graphics_state_show(', 'static void hub_status_show(', 'static boolean graphics_in_vr(',
        'static short graphics_mode_shown(', 'static boolean graphics_replaces(',
        'boolean ui_widget_port_graphics_locked(', 'static char const *graphics_device_name(',
        'static void graphics_help_text(', 'static void graphics_fixed_help(', 'static void graphics_rows_show('))
    functions = block((ROOT / 'port/linux/src/menu_files.c').read_text(), 'long halo_menus_utf16(') + '\n' + functions
    ui = (ROOT / 'source/interface/ui_widget.c').read_text()
    functions += '\n' + '\n'.join(block(ui[ui.rindex(marker):], marker) for marker in (
        'static __inline boolean widget_instance_can_handle_events(',
        'static boolean widget_instance_port_is_label(',
        'static boolean widget_instance_can_receive_events(',
        'static long ui_mouse_child_index(',
        'static boolean ui_mouse_widget_is_item(',
        'static boolean ui_mouse_widget_has_focus(',
        'static struct widget_instance *widget_instance_find_by_tag_index_recursive(',
        'static void widget_instance_give_focus_directly(',
        'static void ui_mouse_give_focus(',
        'static void widget_instance_give_focus_by_tag(',
        'static boolean widget_instance_port_refuse_graphics_edit(',
        'boolean widget_event_function_list_widget_goto_next_item(',
        'boolean widget_event_function_list_widget_goto_previous_item(',
        'static void column_list_update(', 'static void widget_instance_port_focus_child(',
        'static void widget_instance_tab_to_next_valid_widget(',
        'static void widget_instance_tab_to_previous_valid_widget('))
    # the hook that runs them is the settings screens' data function, ahead of the help
    invoke = block(source[source.rindex('void pc_menu_game_data_function_invoke('):],
                   'void pc_menu_game_data_function_invoke(')
    assert 'graphics_state_show(widget)' in invoke and 'hub_status_show(widget)' in invoke
    assert invoke.index('graphics_rows_show(widget)') < invoke.index('settings_help(widget)')
    program = PROGRAM.replace('PORT_GRAPHICS_MODES', str(ROOT / 'port/linux/src/port_graphics_modes.h')).replace(
        '/* PRODUCTION */', functions)
    probe, binary = tmp_path / 'graphics.c', tmp_path / 'graphics'
    probe.write_text(program)
    subprocess.run(['cc', '-Wall', '-Werror', '-Wno-unused-function', '-Wno-unused-const-variable',
                    '-fsanitize=address,undefined', str(probe), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_fixed_help_preserves_complete_text_and_renderable_line_breaks(tmp_path):
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    functions = "\n".join(block(source, marker) for marker in (
        "static void text_set_length(", "static void graphics_help_text(", "static void graphics_fixed_help("))
    functions += "\n" + block((ROOT / "port/linux/src/menu_files.c").read_text(), "long halo_menus_utf16(")
    # Keep the real UTF-16 conversion and text allocation/copy; a raw LF is not
    # a line break in draw_string.c's parse_string, whereas CR is.
    boundary = PROGRAM[:PROGRAM.index("/* PRODUCTION */")]
    boundary = boundary.replace('#define wchar_t unsigned short',
                                '#define wchar_t unsigned short\nlong halo_menus_utf16(char const *, unsigned short *, long);')
    program = boundary + functions + r'''
int main(void)
{
    struct widget_instance help = {0};
    char line[300];
    struct { char const *setting, *value, *expected; int length, breaks; } cases[] = {
        {"display.interpolation", "false", "Frames are drawn at the game's 30 ticks a second in Hardcore mode.", 66, 1},
        {"display.frame_rate", "30", "Fixed to 30 FPS in Hardcore mode.", 33, 0},
        {"display.direct_camera", "false", "Fixed to false in Hardcore mode.", 32, 0},
        {"display.resolution_scaling", "original", "Fixed to original in Hardcore mode.", 35, 0},
        {"display.interpolation", "false", "Frames are drawn at the game's 30 ticks a second in Hardcore mode.", 66, 1},
    };
    for (int i = 0; i < NUMBEROF(cases); i++)
    {
        graphics_fixed_help(&help, GRAPHICS_MODE_HARDCORE, cases[i].setting, cases[i].value);
        short length = text_value_of(help.parameters.text_box.text, line);
        assert(length == cases[i].length);
        assert(!strcmp(line, cases[i].expected));
        int breaks = 0, column = 0;
        for (int j = 0; j < length; j++)
        {
            wchar_t ch = help.parameters.text_box.text[j];
            assert(ch != '\n');
            if (ch == '\r') { assert(column <= 48); breaks++; column = 0; }
            else column++;
        }
        assert(column <= 48);
        assert(breaks == cases[i].breaks);
    }
    free(help.parameters.text_box.text);
    return 0;
}
'''
    program = program.replace('PORT_GRAPHICS_MODES', str(ROOT / 'port/linux/src/port_graphics_modes.h'))
    probe, binary = tmp_path / "help.c", tmp_path / "help"
    probe.write_text(program)
    subprocess.run(['cc', '-Wall', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
                    '-Wno-unused-const-variable', '-fsanitize=address,undefined', str(probe),
                    '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
