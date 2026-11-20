"""The patched disc lists still meet what the disc's code requires of them (tools/disc_contract.py)."""
from pathlib import Path
import sys
import tempfile
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

import disc_contract  # noqa: E402
import ui_map_tags  # noqa: E402

# Violations that are known and being fixed elsewhere, by (list, function), each with its reason. The test fails
# when one of them goes away, so the entry is removed with the fix; nothing new belongs here.
OPEN = {}
POSITIONAL = {f'playlist_profile_{step}_{kind}' for step in ('initialize', 'change')
              for kind in ('ctf_rules', 'koth_rules', 'oddball_rules', 'racing_rules', 'slayer_rules',
                           'indicator_options')} | {
    'player_profile_initialize_controller_settings', 'player_profile_change_controller_settings'}


def test_patched_lists_meet_the_disc_functions_requirements():
    found = {(leaf, name) for leaf, name, _ in disc_contract.violations()}
    assert found == set(OPEN), sorted(found ^ set(OPEN))


def test_the_requirements_are_read_from_the_source():
    events, inputs = disc_contract.function_tables()
    assert len(events) == 102 and len(inputs) == 41
    handlers = disc_contract.HANDLERS.read_text(errors='replace')
    data = disc_contract.DATA_INPUTS.read_text(errors='replace')
    positional = {name for name in events if (disc_contract.requirement(handlers, name) or ('',))[0] == 'positional'}
    # the disc's option-list handlers that still walk their rows: the per-type rule lists, the indicators and the
    # controller settings, none of which a patch changes. The player, item and advanced controls handlers look
    # their rows up by name (option_list_row_spinner), so they are not among them.
    assert positional == POSITIONAL, sorted(positional ^ POSITIONAL)
    # the help updater hands a patched list to the port's code before its own walk, so it requires nothing
    assert 'expected a spinner list somewhere' in disc_contract.function_body(data, 'game_options_menu_update_text_desc')
    assert disc_contract.requirement(data, 'game_options_menu_update_text_desc') is None


def test_an_unguarded_row_walk_is_a_requirement():
    """The same function without the port's early return for patched lists asserts a spinner in every row."""
    data = disc_contract.DATA_INPUTS.read_text(errors='replace')
    guard = disc_contract.PATCHED_LIST_GUARDS[0]
    assert guard in data
    assert disc_contract.requirement(data.replace(guard, 'if (never('), 'game_options_menu_update_text_desc') == (
        'spinner_in_every_row',)


def synthetic_patch(list_path, ops):
    patches = ET.Element('menus')
    patch = ET.SubElement(patches, 'patch', list=list_path, screen=list_path)
    for tag, attributes in ops:
        ET.SubElement(patch, tag, attributes)
    file = Path(tempfile.mkdtemp()) / 'patches.xml'
    ET.ElementTree(patches).write(file)
    return file


INDICATORS = 'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\indicator_options_edit\\indicator_options_menu'


def test_a_patch_that_changes_a_walked_row_is_reported():
    layout = ui_map_tags.load_layout()
    rows = [child['widget'] for child in layout[INDICATORS]['children']]
    expected = {'playlist_profile_initialize_indicator_options', 'playlist_profile_change_indicator_options'}
    # removing a walked row, and putting a row before one, both move the rows the handlers count to
    for ops in ([('remove', {'child': rows[0]})],
                [('row', {'name': 'op_added', 'before': rows[1], 'text': 'ADDED'})]):
        found = {name for _, name, _ in disc_contract.violations(layout, synthetic_patch(INDICATORS, ops))}
        assert expected <= found, ops


def test_a_row_without_a_spinner_is_allowed_where_the_port_takes_over_the_help():
    """RESPAWN and CHOOSE WEAPONS have no spinner; the disc's help updater returns early for patched lists."""
    names = {name for _, name, _ in disc_contract.violations()}
    assert 'game_options_menu_update_text_desc' not in names


def test_a_row_appended_after_the_walked_rows_is_allowed():
    """The shipped advanced controls patch adds IN-GAME HELP after the five rows the handlers walk."""
    found = {leaf for leaf, _, _ in disc_contract.violations()}
    assert 'advanced_controls_menu' not in found
