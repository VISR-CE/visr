"""Exercise the graphics modes' menu rows with the production functions of port/linux/game/menu_functions.c: the
hub's mode word, the CRT effect's row shown only in Hardcore, the settings Hardcore replaces reading HARDCORE with
their rows passed over, and the help of the mode shown."""
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
    short type;
    boolean visible, disabled;
    struct widget_instance *next, *parent, *child, *focused_child;
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
long halo_menus_utf16(char const *utf8, unsigned short *out, long capacity)
{
    long n = 0;
    for (; utf8[n] && n < capacity - 1; n++) out[n] = (unsigned char)utf8[n];
    out[n] = 0;
    return n;
}
static void text_set_length(struct widget_instance *box, wchar_t const *text, short length)
{
    if (!box->parameters.text_box.text) box->parameters.text_box.text = calloc(length, sizeof(wchar_t));
    for (short i = 0; i < length - 1 && text[i]; i++) box->parameters.text_box.text[i] = text[i];
}
static boolean spinner_item(struct widget_instance const *widget)
{
    return widget->parent && widget->parent->definition_tag_index == widget->definition_tag_index;
}
static struct widget_instance *screen_of(struct widget_instance *widget)
{
    while (widget->parent) widget = widget->parent;
    return widget;
}
/* PRODUCTION */

static struct widget_instance widgets[64];
static int widget_total;
static struct widget_instance *make(char const *name, struct widget_instance *parent, short type)
{
    struct widget_instance *widget = &widgets[widget_total];
    memset(widget, 0, sizeof(*widget));
    widget->definition_tag_index = widget_total++;
    widget->name = name;
    widget->type = type;
    widget->visible = TRUE;
    widget->parent = parent;
    if (parent)
    {
        struct widget_instance **tail = &parent->child;
        while (*tail) tail = &(*tail)->next;
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

    list->parameters.list.extended_description = help;
    list->focused_child = op_mode;
    spinner(op_crt, "crt_effect_spinner", "display.crt_effect", "true|false");

    /* the hub's row: the mode in force, in capitals */
    for (stored_mode = 0; stored_mode < NUMBER_OF_GRAPHICS_MODES; stored_mode++)
    {
        struct widget_instance *state = make("graphics_state", NULL, 1);
        graphics_state_show(state);
        for (int i = 0; i < 16; i++) text[i] = state->parameters.text_box.text[i];
        for (char const *expected = graphics_mode_names[stored_mode]; *expected; expected++)
            assert(text[expected - graphics_mode_names[stored_mode]] == (wchar_t)toupper(*expected));
    }

    /* Enhanced, stored and shown: the CRT row hidden, the frame rate row ordinary */
    stored_mode = GRAPHICS_MODE_ENHANCED;
    mode->parameters.list.selected_index = 2;
    list->focused_child = op_rate;
    graphics_rows_show(list);
    assert(!op_crt->visible);
    assert(rate->visible && !rate_replaced->visible && !op_rate->disabled);
    assert(sync->visible && !op_sync->disabled);
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
    assert(!rate->visible && rate_replaced->visible && op_rate->disabled);
    /* vertical sync is no setting the mode replaces */
    assert(sync->visible && !op_sync->disabled);
    list->focused_child = op_mode;
    graphics_rows_show(list);
    assert(mode_help->visible && mode_help->parameters.text_box.string_list_index == 0);

    /* Hardcore saved: the game's own answer */
    stored_mode = GRAPHICS_MODE_HARDCORE;
    graphics_rows_show(list);
    assert(op_crt->visible && !rate->visible && rate_replaced->visible && op_rate->disabled);

    /* Classic replaces none of these rows, and hides the CRT effect's */
    mode->parameters.list.selected_index = 1;
    stored_mode = GRAPHICS_MODE_CLASSIC;
    list->focused_child = op_crt;
    graphics_rows_show(list);
    assert(!op_crt->visible && list->focused_child == op_mode && list->parameters.list.selected_index == 0);
    assert(rate->visible && !rate_replaced->visible && !op_rate->disabled);
    assert(mode_help->parameters.text_box.string_list_index == 1);

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
        'static struct widget_instance *descendant(', 'static struct widget_instance *named(',
        'static void graphics_state_show(', 'static boolean graphics_in_vr(', 'static short graphics_mode_shown(', 'static boolean graphics_replaces(',
        'static void graphics_rows_show('))
    # the hook that runs them is the settings screens' data function, ahead of the help
    invoke = block(source[source.rindex('void pc_menu_game_data_function_invoke('):],
                   'void pc_menu_game_data_function_invoke(')
    assert 'graphics_state_show(widget)' in invoke
    assert invoke.index('graphics_rows_show(widget)') < invoke.index('settings_help(widget)')
    program = PROGRAM.replace('PORT_GRAPHICS_MODES', str(ROOT / 'port/linux/src/port_graphics_modes.h')).replace(
        '/* PRODUCTION */', functions)
    probe, binary = tmp_path / 'graphics.c', tmp_path / 'graphics'
    probe.write_text(program)
    subprocess.run(['cc', '-Wall', '-Werror', '-Wno-unused-function', '-Wno-unused-const-variable',
                    '-fsanitize=address,undefined', str(probe), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
