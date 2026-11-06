"""Server Setup edits a temporary gametype through both menu looks."""
from pathlib import Path
import re
import subprocess

from test_menu_cancel import block

ROOT = Path(__file__).resolve().parents[1]


def test_setup_profile_actions_and_stock_function_routing(tmp_path):
    menus = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    tags = (ROOT / 'port/linux/game/menu_tags.c').read_text()
    actions = '\n'.join(block(menus, 'else if (!strcmp(name, "' + name + '"))') for name in (
        'mp profile save changes', 'close if not editing profile', 'mp profile change name',
        'mp profile end editing', 'port delete request'))
    tables = '\n'.join(re.search(r'static char const \*const ' + name + r'\[\] =\s*\{.*?\n\};', tags, re.S).group()
                       for name in ('port_function_names', 'port_game_data_input_names', 'game_data_input_names'))
    routing = '\n'.join(block(tags, marker) for marker in (
        'static long name_index(', 'long pc_menu_setup_event_function(', 'long pc_menu_setup_data_function('))
    program = r'''
#include <assert.h>
#include <string.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define PC_MENU_FUNCTION_BASE 1000
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
struct widget_instance { int unused; };
struct event_record { int unused; };
static struct { boolean setup; } gametype_edit;
static int back_count, saved, original, deleted, errors;
boolean pc_menu_gametype_setup_active(void) { return gametype_edit.setup; }
static void ui_widget_port_go_back(struct widget_instance *w) { back_count++; }
static boolean ui_widget_port_gametype_save(struct widget_instance *w, boolean *d) { saved++; return TRUE; }
static boolean game_function(char const *n, struct widget_instance *w, struct event_record *e, boolean *d)
{ original++; return TRUE; }
static boolean ui_widget_port_delete_request(void) { deleted++; return TRUE; }
static boolean campaign_fail(void) { errors++; return FALSE; }
static char const *const originals[] = {"close if not editing profile", "mp profile change name",
    "mp profile save changes", "mp profile end editing", "mp profile init player opts"};
static char const *ui_widget_event_handler_function_name(long index)
{ return index >= 0 && index < NUMBEROF(originals) ? originals[index] : NULL; }
/* TABLES */
/* ROUTING */
static boolean invoke(char const *name, struct widget_instance *widget, struct event_record *event, boolean *widget_deleted)
{
    if (!name) return FALSE;
    /* ACTIONS */
    return FALSE;
}
int main(void)
{
    struct widget_instance w = {0}; struct event_record e = {0}; boolean gone = FALSE;
    gametype_edit.setup = TRUE;
    assert(invoke("close if not editing profile", &w, &e, &gone));
    assert(!gone && !original);
    assert(!invoke("mp profile change name", &w, &e, &gone));
    assert(!invoke("port delete request", &w, &e, &gone));
    assert(errors == 2 && !saved && !original && !deleted);
    assert(invoke("mp profile end editing", &w, &e, &gone));
    assert(!original && gametype_edit.setup);
    assert(invoke("mp profile save changes", &w, &e, &gone));
    assert(gone && back_count == 1 && gametype_edit.setup && !saved && !original);
    for (long i = 0; i < NUMBEROF(originals); i++) {
        long mapped = pc_menu_setup_event_function(i);
        assert(mapped >= PC_MENU_FUNCTION_BASE);
        assert(!strcmp(port_function_names[mapped - PC_MENU_FUNCTION_BASE], originals[i]));
    }
    for (long i = 0; i < NUMBEROF(game_data_input_names); i++) {
        if (strcmp(game_data_input_names[i], "get edit game settings name") &&
            strcmp(game_data_input_names[i], "mp edit profile set rule text")) continue;
        long mapped = pc_menu_setup_data_function(i);
        assert(mapped >= PC_MENU_FUNCTION_BASE);
        assert(!strcmp(port_game_data_input_names[mapped - PC_MENU_FUNCTION_BASE], game_data_input_names[i]));
    }
    gametype_edit.setup = FALSE; gone = FALSE;
    assert(invoke("mp profile save changes", &w, &e, &gone) && saved == 1 && !gone);
    assert(invoke("mp profile change name", &w, &e, &gone) && original == 1);
    assert(invoke("close if not editing profile", &w, &e, &gone) && original == 2);
    assert(invoke("mp profile end editing", &w, &e, &gone) && original == 3);
    assert(invoke("port delete request", &w, &e, &gone) && deleted == 1);
    for (long i = 0; i < NUMBEROF(originals); i++) assert(pc_menu_setup_event_function(i) == i);
    for (long i = 0; i < NUMBEROF(game_data_input_names); i++) assert(pc_menu_setup_data_function(i) == i);
}
'''.replace('/* TABLES */', tables).replace('/* ROUTING */', routing).replace('/* ACTIONS */', actions)
    source, binary = tmp_path / 'setup.c', tmp_path / 'setup'
    source.write_text(program)
    subprocess.run(['cc', '-Wall', '-Werror', str(source), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_setup_copy_is_applied_only_at_start(tmp_path):
    menus = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    functions = '\n'.join(block(menus, marker) for marker in (
        'static struct game_variant *edit_variant(void)', 'static struct game_variant_options *edit_options(void)',
        'static void gametype_setup_begin(void)\n{', 'static void gametype_setup_end(void)\n{',
        'static boolean gametype_setup_apply(void)\n{'))
    program = r'''
#include <assert.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
struct game_variant { int rule; };
struct game_variant_options { int option; };
static struct game_variant game = {1}, saved = {2};
static struct game_variant_options options = {3}, saved_options = {4};
static struct { boolean setup; struct game_variant setup_variant;
    struct game_variant_options setup_options; int vehicle_side; } gametype_edit;
static int applied;
static struct game_variant *player_ui_get_edit_playlist_profile(void) { return &saved; }
static struct game_variant_options *player_ui_get_edit_playlist_options(void) { return &saved_options; }
static boolean player_ui_game_variant_specified(struct game_variant *v) { *v = game; return TRUE; }
static struct game_variant_options *player_ui_get_game_variant_options(void) { return &options; }
static boolean ui_widget_port_game_variant_set(struct game_variant *v, struct game_variant_options const *o)
{ game = *v; options = *o; applied++; return TRUE; }
/* FUNCTIONS */
int main(void)
{
    assert(edit_variant() == &saved && edit_options() == &saved_options);
    gametype_setup_begin();
    assert(edit_variant() == &gametype_edit.setup_variant && edit_options() == &gametype_edit.setup_options);
    edit_variant()->rule = 9; edit_options()->option = 10;
    gametype_setup_begin(); /* Returning from a sub-screen must preserve edits. */
    assert(edit_variant()->rule == 9 && edit_options()->option == 10);
    assert(game.rule == 1 && options.option == 3 && saved.rule == 2 && saved_options.option == 4 && !applied);
    assert(gametype_setup_apply());
    assert(!gametype_edit.setup && applied == 1 && game.rule == 9 && options.option == 10);
    assert(saved.rule == 2 && saved_options.option == 4);
    gametype_setup_begin(); edit_variant()->rule = 20;
    gametype_setup_end();
    assert(!gametype_edit.setup && game.rule == 9 && applied == 1);
}
'''.replace('/* FUNCTIONS */', functions)
    source, binary = tmp_path / 'scope.c', tmp_path / 'scope'
    source.write_text(program)
    subprocess.run(['cc', '-Wall', '-Werror', str(source), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_invite_pointer_target_posts_x_without_list_focus(tmp_path):
    widgets = (ROOT / 'source/interface/ui_widget.c').read_text()
    function = block(widgets, 'static short ui_mouse_key_button(\n')
    program = r'''
#include <assert.h>
#include <string.h>
#define NONE (-1)
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
enum { _gamepad_analog_button_a, _gamepad_analog_button_b, _gamepad_analog_button_x,
    _gamepad_analog_button_y, _gamepad_analog_button_black, _gamepad_analog_button_white,
    _gamepad_binary_button_start, _gamepad_binary_button_back };
struct widget_instance { long definition_tag_index; };
static char const *name;
static char const *tag_get_name(long tag) { return name; }
/* FUNCTION */
int main(void)
{
    struct widget_instance widget = {0};
    name = "pc\\main_menu\\multiplayer_type_select\\server_settings\\x_butn_invite";
    assert(ui_mouse_key_button(&widget) == _gamepad_analog_button_x);
    name = "pc\\xbox\\main_menu\\multiplayer_type_select\\server_settings\\x_butn_invite";
    assert(ui_mouse_key_button(&widget) == _gamepad_analog_button_x);
    name = "pc\\main_menu\\multiplayer_type_select\\server_settings\\invite_value";
    assert(ui_mouse_key_button(&widget) == NONE);
}
'''.replace('/* FUNCTION */', function)
    source, binary = tmp_path / 'pointer.c', tmp_path / 'pointer'
    source.write_text(program)
    subprocess.run(['cc', '-Wall', '-Werror', str(source), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_coop_merged_setting_dispatch():
    source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    tags = (ROOT / 'port/linux/game/menu_tags.c').read_text()
    assert '"coop options init"' in tags and '"coop options save"' in tags
    init = block(source, 'else if (!strcmp(name, "coop options init"))')
    save = block(source, 'else if (!strcmp(name, "coop options save"))')
    assert 'coop_options_init(screen_of(widget));' in init
    assert 'settings_each(screen_of(widget), setting_changed_save);' in save
    assert 'coop_options_save(screen_of(widget));' in save
    settings = block(source, 'static void settings_each(')
    assert 'pc_menu_setting_get(child->definition_tag_index)' in settings
    assert 'if (setting)' in settings and 'visit(child, setting);' in settings
    metadata = block(tags, 'if (source->setting)')
    assert 'setting_add(source, build.widget_tags[widget_index]);' in metadata
