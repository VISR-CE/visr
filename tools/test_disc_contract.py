"""The patched disc lists still meet what the disc's code requires of them (tools/disc_contract.py)."""
from pathlib import Path
import sys
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

import disc_contract  # noqa: E402
import ui_map_tags  # noqa: E402

# Violations that are open on this branch, each fixed elsewhere. The test fails when one of them goes away, so the
# entry is removed with the fix; nothing new belongs here.
PLAYER_OPTIONS_FIX = 'fixed by the name lookups of the player and item option handlers (option_list_row_spinner)'
OPEN = {
    ('player_options_menu', 'playlist_profile_initialize_player_options'): PLAYER_OPTIONS_FIX,
    ('player_options_menu', 'playlist_profile_change_player_options'): PLAYER_OPTIONS_FIX,
    ('item_options_menu', 'playlist_profile_initialize_item_options'): PLAYER_OPTIONS_FIX,
    ('item_options_menu', 'playlist_profile_change_item_options'): PLAYER_OPTIONS_FIX,
    ('player_options_menu', 'game_options_menu_update_text_desc'): 'RESPAWN has no spinner: focusing it halts',
    ('item_options_menu', 'game_options_menu_update_text_desc'): 'CHOOSE WEAPONS has no spinner: focusing it halts',
}


def test_patched_lists_meet_the_disc_functions_requirements():
    found = {(leaf, name) for leaf, name, _ in disc_contract.violations()}
    assert found == set(OPEN), sorted(found ^ set(OPEN))


def test_the_requirements_are_read_from_the_source():
    events, inputs = disc_contract.function_tables()
    assert len(events) == 102 and len(inputs) == 41
    handlers = disc_contract.HANDLERS.read_text(errors='replace')
    data = disc_contract.DATA_INPUTS.read_text(errors='replace')
    positional = {name for name in events if (disc_contract.requirement(handlers, name) or ('',))[0] == 'positional'}
    # the option-list handlers of the disc: every one walks its rows unless it was changed to look them up
    assert len(positional) >= 18, sorted(positional)
    assert disc_contract.requirement(data, 'game_options_menu_update_text_desc') == ('spinner_in_every_row',)


def test_a_patch_that_removes_a_walked_row_is_reported():
    layout = ui_map_tags.load_layout()
    path = 'ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\advanced_controls\\advanced_controls_menu'
    first = layout[path]['children'][0]['widget']
    patches = ET.Element('menus')
    patch = ET.SubElement(patches, 'patch', list=path, screen=path.replace('_menu', '_screen'))
    ET.SubElement(patch, 'remove', child=first)
    file = Path(__import__('tempfile').mkdtemp()) / 'patches.xml'
    ET.ElementTree(patches).write(file)
    found = {name for _, name, _ in disc_contract.violations(layout, file)}
    assert {'player_profile_initialize_advanced_controller_settings',
            'player_profile_change_advanced_controller_settings'} <= found


def test_a_row_appended_after_the_walked_rows_is_allowed():
    """The shipped advanced controls patch adds IN-GAME HELP after the five rows the handlers walk."""
    found = {leaf for leaf, _, _ in disc_contract.violations()}
    assert 'advanced_controls_menu' not in found
