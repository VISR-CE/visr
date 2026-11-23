"""The patched disc lists still meet what the disc's code requires of them (tools/disc_contract.py)."""
import atexit
from pathlib import Path
import shutil
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
    folder = Path(tempfile.mkdtemp())
    atexit.register(shutil.rmtree, folder, ignore_errors=True)
    file = folder / 'patches.xml'
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


# Disc option lists the game shows that do not fit with every row tall, with (rows, where they would end, the
# limit). Each needs a decision (fewer rows, a scrolling window, or the help moved) before the engine restacks it;
# the test fails when one of them starts to fit or a new one stops fitting.
TALL_OVERFLOW = {
    'controller_options_menu': (2, 151, 140),
}
# Disc option lists on screens the game never shows (a redirect replaces the screen, or a patch removes the only
# row that opened it). They are not laid out: a fit test of a layout no player sees is the bug class here.
NEVER_SHOWN = {
    'ctf_options_menu', 'koth_options_menu', 'oddball_options_menu', 'race_options_menu', 'slayer_options_menu',
    'indicator_options_menu'}


def shown_option_lists():
    """Every disc option list on a screen the game can show, laid out after the shipped patches with every row
    tall: {list leaf: (rows, where they end, the limit)}, and the leaves of the lists it never shows."""
    import xbox_menus

    root = ET.parse(disc_contract.XBOX / 'patches.xml').getroot()
    # (the generator marks the lists that fit rows="tall"; laid out here from the patches without the marks' own
    # empty patches, so the limit of an unpatched list is its help text's top)
    for patch in list(root.iter('patch')):
        if not len(patch):
            root.remove(patch)
    layout, ours = ui_map_tags.load_layout(), disc_contract.xbox_widgets()
    redirects = {source.replace('/', '\\'): target.replace('/', '\\')
                 for source, target in xbox_menus.SCREEN_ALWAYS + xbox_menus.SCREEN_REPLACED}
    shown = disc_contract.reachable_screens(layout, root, ours, redirects, disc_contract.screens_named_in_the_game())
    found = disc_contract.option_lists_tall(layout, root, ours)
    return ({path.rsplit('\\', 1)[-1]: fit[:3] for path, fit in found.items() if fit[3] in shown},
            {path.rsplit('\\', 1)[-1] for path, fit in found.items() if fit[3] not in shown})


def test_every_disc_option_list_the_game_shows_fits_with_every_row_tall():
    """Every row of every screen is the standard tall row, the disc's own rows included (the engine restacks a
    disc option list at the standard pitch from its first row, patched or not). Laid out as the game shows it
    (after the shipped patches, on the screens it can reach), each list must end by its limit: its patch's
    footer, else the top of its help text. A list that would not fit is listed in TALL_OVERFLOW with its numbers."""
    import menu_geometry

    assert disc_contract.TALL_PITCH == menu_geometry.COMFORTABLE_PITCH
    checked, hidden = shown_option_lists()
    assert set(checked) == {
        'advanced_controls_menu', 'item_options_menu', 'player_options_menu', 'controller_options_menu'}, sorted(checked)
    assert hidden == NEVER_SHOWN, sorted(hidden)
    over = {name: fit for name, fit in checked.items() if fit[1] > fit[2]}
    assert over == TALL_OVERFLOW, over


def test_a_redirect_or_a_removed_row_takes_a_screen_out_of_the_layout():
    """The reach follows the game: a redirected screen is replaced by its target, a row a patch removes opens
    nothing, and a row a patch adds opens its target."""
    layout = {
        'ui\\shell\\main_menu\\main_menu': {'handlers': [], 'children': [{'widget': 'a'}, {'widget': 'b'}]},
        'a': {'handlers': [{'widget': 'ui\\one'}], 'children': []},
        'b': {'handlers': [{'widget': 'ui\\two'}], 'children': []},
        'ui\\one': {'handlers': [], 'children': []},
        'ui\\two': {'handlers': [{'widget': 'ui\\three'}], 'children': []},
        'ui\\three': {'handlers': [], 'children': []},
    }
    main = 'ui\\shell\\main_menu\\main_menu'
    patches = ET.fromstring('<menus/>')
    assert disc_contract.reachable_screens(layout, patches, {}, {}) == {main, 'ui\\one', 'ui\\two', 'ui\\three'}
    ours = {'xbox/two': ET.fromstring('<widget name="xbox/two"><on event="a" open="xbox/four"/></widget>'),
            'xbox/four': ET.fromstring('<widget name="xbox/four"/>')}
    assert disc_contract.reachable_screens(layout, patches, ours, {'ui\\two': 'pc\\xbox\\two'}) == {
        main, 'ui\\one', 'pc\\xbox\\two', 'pc\\xbox\\four'}
    patches = ET.fromstring(f'<menus><patch list="l" screen="{main}"><remove child="a"/>'
                            '<row after="b" name="n" open="xbox/four"/></patch></menus>')
    assert disc_contract.reachable_screens(layout, patches, ours, {}) == {
        main, 'ui\\two', 'ui\\three', 'pc\\xbox\\four'}


def test_no_path_reaches_the_step_one_gametype_list():
    """GAME OPTIONS holds the five TYPE rows, so the game's own list of gametypes ("step 1") and the rule screens
    behind it are never shown: every screen that asks for one gets ours."""
    import xbox_menus

    root = ET.parse(disc_contract.XBOX / 'patches.xml').getroot()
    layout, ours = ui_map_tags.load_layout(), disc_contract.xbox_widgets()
    redirects = {source.replace('/', '\\'): target.replace('/', '\\')
                 for source, target in xbox_menus.SCREEN_ALWAYS + xbox_menus.SCREEN_REPLACED}
    shown = disc_contract.reachable_screens(layout, root, ours, redirects, disc_contract.screens_named_in_the_game())
    step_one = 'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\playlist_edit\\game_type_select\\gametype_select_screen'
    assert step_one in layout and step_one not in shown
    assert redirects[step_one] == 'pc\\xbox\\main_menu\\multiplayer_type_select\\game_options_edit\\game_options_screen'
    assert redirects[step_one] in shown
    # (something still asks for it, so the redirect is what keeps it off the screen: the map's own rows, and
    # ours that open it by its name)
    asked = [path for path, widget in layout.items()
             if any(handler.get('widget') == step_one for handler in widget['handlers'])]
    asked += [name for name, widget in ours.items()
              if any(element.get('open') == step_one for element in widget.iter('on'))]
    assert asked


def test_the_patches_make_tall_exactly_the_shown_lists_that_fit():
    """rows="tall" is on every disc list of a standard row kind the game shows that fits and is not exempted, and on
    no other, each with its row kind, that kind's tall art and its screen."""
    import xbox_menus

    root = ET.parse(disc_contract.XBOX / 'patches.xml').getroot()
    layout = ui_map_tags.load_layout()
    marked = {patch.get('list').rsplit('\\', 1)[-1]: patch for patch in root.iter('patch') if patch.get('rows')}
    for patch in marked.values():
        kind = disc_contract.row_kind(layout, patch.get('list'))
        art, top_art = xbox_menus.TALL_ART[kind]
        assert (patch.get('rows'), patch.get('row_kind'), patch.get('tall_art'), patch.get('tall_top_art')) == (
            'tall', kind, art, top_art), patch.get('list')
    checked, _ = shown_option_lists()
    pictures, _ = shown_picture_lists()
    exempt = {path.rsplit('\\', 1)[-1] for path in xbox_menus.DISC_LIST_EXEMPTIONS}
    assert set(marked) == (set(checked) - set(TALL_OVERFLOW)) | (set(pictures) - exempt) == {
        'advanced_controls_menu', 'item_options_menu', 'player_options_menu',
        'multiplayer_type_select_list', 'difficulty_select_list'}
    for patch in marked.values():
        assert any(child['widget'] == patch.get('list') for child in layout[patch.get('screen')]['children'])


# The platforms a patch row can name (platform="tvos", "!mac_app", ...), as each device shows them.
PLATFORMS = {'iphone': set(), 'ipad_keyboard': {'keyboard'}, 'tvos': {'tvos'}, 'mac_app': {'mac_app', 'keyboard'}}


def shows(platform, active):
    """whether a row with platform= (None for every platform) shows where the active platforms are"""
    if not platform:
        return True
    return (platform.lstrip('!') in active) != platform.startswith('!')


def shown_picture_lists():
    """Every disc picture list on a screen the game can show: {list leaf: {device: (rows, where they end, the
    limit)}} after the shipped patches with every row tall, each device counting the patch rows it shows; and the
    leaves of the picture lists it never shows."""
    import xbox_menus

    root = ET.parse(disc_contract.XBOX / 'patches.xml').getroot()
    layout, ours = ui_map_tags.load_layout(), disc_contract.xbox_widgets()
    redirects = {source.replace('/', '\\'): target.replace('/', '\\')
                 for source, target in xbox_menus.SCREEN_ALWAYS + xbox_menus.SCREEN_REPLACED}
    shown = disc_contract.reachable_screens(layout, root, ours, redirects, disc_contract.screens_named_in_the_game())
    found, hidden = {}, set()
    for device, active in PLATFORMS.items():
        filtered = ET.fromstring(ET.tostring(root))
        for patch in filtered.iter('patch'):
            for op in list(patch):
                if not shows(op.get('platform'), active):
                    patch.remove(op)
        for path, (rows, bottom, limit, screen, kind) in disc_contract.standard_lists_tall(layout, filtered, ours).items():
            if kind != 'picture':
                continue
            if screen in shown:
                found.setdefault(path.rsplit('\\', 1)[-1], {})[device] = (rows, bottom, limit)
            else:
                hidden.add(path.rsplit('\\', 1)[-1])
    return found, hidden


def test_every_disc_picture_list_the_game_shows_fits_with_every_row_tall_on_every_device():
    """The disc's picture lists (text rows beside a picture panel) are restacked as standard tall rows, keeping the
    disc's gaps and never reordering a row (the difficulty list's handler reads the chosen row's place as the
    difficulty). Laid out as each device shows them, every list the game shows ends above the bottom bar, except
    the lists in the generator's DISC_LIST_EXEMPTIONS, each with its reason."""
    import xbox_menus

    found, hidden = shown_picture_lists()
    assert hidden == {'edit_playlist_select_list', 'gametype_select_list', 'profile_edit_select_list'}, sorted(hidden)
    assert set(found) == {'multiplayer_type_select_list', 'difficulty_select_list', 'server_list'}, sorted(found)
    exempt = {path.rsplit('\\', 1)[-1]: reason for path, reason in xbox_menus.DISC_LIST_EXEMPTIONS.items()}
    assert set(exempt) == {'server_list'} and all(exempt.values())
    over = {(leaf, device): fit for leaf, devices in found.items() for device, fit in devices.items()
            if fit[1] > fit[2] and leaf not in exempt}
    assert over == {}, over
    # (the multiplayer type list keeps its 22-unit gap before its last row: 6 rows from 78, 39 apart, + 22)
    assert found['multiplayer_type_select_list']['iphone'] == (6, 78 + 6 * 39 + 22, 414)
    assert found['difficulty_select_list']['tvos'] == (4, 78 + 4 * 39, 414)
