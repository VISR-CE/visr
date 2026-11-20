"""The disc's option-list handlers find their rows by name once a patch moves them.

`playlist_profile_initialize_player_options` and the other gametype option handlers
walked the list's rows in the disc's order and halted on the first row without a
spinner. The Xbox look's Player Options patch replaces the four respawn rows with one
RESPAWN row, so opening PLAYER OPTIONS halted with "expected 'respawn time growth'
option spinner list". Each handler of a patched option list now asks for its rows by
name (`option_list_row_spinner`) and leaves a removed row to its replacement. A patched list's
description also takes each row's help line from the patch (`pc_menu_patch_row_help`): the disc's
help updater counted one spinner a row in the disc's order and halted on RESPAWN, which has none.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
HANDLERS = ROOT / 'source/interface/ui_widget_event_handler_functions.c'
PATCHES = ['port/assets/menus/xbox/patches.xml', 'port/assets/menus/xbox/patches.compact.xml']

# Each disc option list a patch changes: its handlers and the disc's rows in order.
OPTION_LISTS = {
    'player_options_menu': (
        ['playlist_profile_initialize_player_options', 'playlist_profile_change_player_options'],
        ['op_number_of_lives', 'op_maximum_health', 'op_shields', 'op_respawn_time',
         'op_respawn_time_growth', 'op_odd_man_out', 'op_invisible_players', 'op_suicide_penalty']),
    'advanced_controls_menu': (
        ['player_profile_initialize_advanced_controller_settings',
         'player_profile_change_advanced_controller_settings'],
        ['op_invert_joystick', 'op_controller_sensitivity', 'op_controller_vibration',
         'op_flight_stick_controls', 'op_auto_center']),
    'item_options_menu': (
        ['playlist_profile_initialize_item_options', 'playlist_profile_change_item_options'],
        ['op_infinite_grenades', 'op_vehicle_set', 'op_weapon_set', 'op_starting_equipment']),
}


def _function_body(source, name):
    match = re.search(r'static boolean %s\(\n[^;{]*\)\n\{' % name, source)
    assert match, name
    return source[match.start():source.index('\n}\n', match.start())]


def _patched_option_lists():
    for path in PATCHES:
        for patch in ET.parse(ROOT / path).getroot().iter('patch'):
            removed = [r.get('child').split('\\')[-1] for r in patch.iter('remove')]
            if any(name.startswith('op_') for name in removed):
                yield path, patch.get('list').split('\\')[-1], removed


def test_every_patched_option_list_has_name_based_handlers():
    found = list(_patched_option_lists())
    assert found, 'no patch removes an option row any more'
    for path, list_name, removed in found:
        assert list_name in OPTION_LISTS, f'{path}: {list_name} drops {removed}; check its disc handlers'
        assert set(removed) <= set(OPTION_LISTS[list_name][1])


def test_option_handlers_look_up_each_disc_row_by_name():
    source = HANDLERS.read_text()
    for functions, rows in OPTION_LISTS.values():
        for function in functions:
            body = _function_body(source, function)
            assert 'list_item' not in body, f'{function} still walks the rows in the disc order'
            assert re.findall(r'option_list_row_spinner\(widget, "(\w+)"\)', body) == rows, function
            # a missing row is skipped, never dereferenced
            assert body.count('option_spinner = option_list_row_spinner(') == \
                len(re.findall(r'option_list_row_spinner\(widget, "\w+"\);\n(?:[^\n]*\n)?\t\tif \(option_spinner\)', body))


def test_row_lookup_matches_the_disc_tag_leaf():
    source = HANDLERS.read_text()
    helper = source.split('static struct widget_instance *option_list_row_spinner(struct widget_instance *list, '
                          'char const *row_name)\n{', 1)[1].split('\n}\n', 1)[0]
    assert 'tag_get_name(row->definition_tag_index)' in helper
    assert "strrchr(name, '\\\\')" in helper
    assert 'spinner->type != 2' in helper


def test_option_help_updaters_ask_the_patch_first():
    updater = (ROOT / 'source/interface/ui_widget_game_data_input_functions.c').read_text()
    body = updater.split('static void game_options_menu_update_text_desc(\n', 1)[1].split('\n}\n', 1)[0]
    assert body.index('if (ui_patched_options_help_update(widget))') < body.index('spinner_list = column->child;')
    port = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    body = port.split('static void gametype_option_help(struct widget_instance *list)\n{', 1)[1].split('\n}\n', 1)[0]
    assert 'if (ui_patched_options_help_update(list))' in body
