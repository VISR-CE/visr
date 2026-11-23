"""Xbox settings generation preserves wiring and needs no extracted assets."""
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import zlib
from xml.etree import ElementTree as ET

import pytest

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'
sys.path.insert(0, str(ROOT / 'tools'))
from ui_map_tags import check_menus


def generator():
    spec = importlib.util.spec_from_file_location('xbox_menus', ROOT / 'tools/xbox_menus.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def looks():
    """The generator's source rows written out as files under `sources/`, beside the generated `xbox/`: the tests that
    compare a screen with its source read both the same way."""
    folder = Path(tempfile.mkdtemp())
    (folder / 'sources').mkdir()
    module = generator()
    for name in module.source_names():
        (folder / 'sources' / name).write_text(ET.tostring(module.source(name), encoding='unicode'), encoding='utf-8')
    (folder / 'xbox').symlink_to(MENUS / 'xbox')
    return folder


LOOKS = looks()


def test_shared_layout_geometry_is_used():
    module = generator()
    coop = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.coop_options.xml')
    options = next(w for w in coop.iter('widget') if w.get('name', '').endswith('/options_menu'))
    ys = [int(c.get('y')) for c in options.findall('child') if '/op_' in c.get('widget')]
    assert ys == list(range(73, 73 + len(ys) * 39, 39)), 'coop_options: row pitch'
    for page in module.PAGES:
        root = ET.parse(MENUS / f'xbox/settings.{page}.xml')
        screen = root.find('widget')
        # (the bar is declared; the engine places it in the shared slots, menu_geometry.h)
        assert bar_actions(screen) == ({'b': 'back', 'a': 'select'} if page == 'about' else
                                       {'x': 'global', 'b': 'back', 'a': 'select'}), page
        assert not [c for c in screen.findall('child') if c.get('widget').endswith('_butn')], page


def bar_actions(screen):
    """{slot: scope} of a screen's declared button bar"""
    return {action.get('slot'): action.get('scope') for action in screen.iterfind('bar/action')}


def snapshot(path):
    return {p.relative_to(path).as_posix(): p.read_bytes() for p in path.rglob('*') if p.is_file()}


def test_generation_is_deterministic_and_checked_in(tmp_path):
    assets = tmp_path / 'menus'
    shutil.copytree(MENUS / 'pictures', assets / 'pictures')
    # (the art the generator draws from: tools/xbox_menu_art.py's)
    shutil.copytree(MENUS / 'xbox/art', assets / 'xbox/art')
    shutil.copy(MENUS / 'menus.json', assets / 'menus.json')
    module = generator()
    module.generate(assets)
    first = snapshot(assets)
    module.generate(assets)
    assert snapshot(assets) == first
    assert snapshot(assets / 'xbox') == snapshot(MENUS / 'xbox')
    assert (assets / 'menus.json').read_bytes() == (MENUS / 'menus.json').read_bytes()
    assert not check_menus(assets / 'xbox')
    manifest = json.loads((assets / 'menus.json').read_text())['files']
    assert len(manifest) == len(set(manifest))
    for file in (assets / 'xbox').glob('*.xml'):
        assert file.relative_to(assets).as_posix() in manifest
        for frame in ET.parse(file).iter('frame'):
            assert frame.get('png') in manifest
    assert manifest == module.manifest_files(assets)
    assert not [name for name in manifest if name.startswith('sources/') and name.endswith('.xml')]
    assert 'pictures/port/pause/pausebox_center__1.png' in manifest


def test_settings_keep_pc_values_and_handlers():
    module = generator()
    for folder in module.PAGES:
        source = ET.parse(LOOKS / 'sources' / f'{module.PE.replace("/", ".")}.{folder}.xml')
        output = ET.parse(MENUS / 'xbox' / f'settings.{folder}.xml')
        expected = {w.get('setting'): (w.get('values'), w.get('strings'))
                    for w in source.iter('widget') if w.get('setting')
                    and w.get('name').rsplit('/', 1)[-1] not in module.XBOX_DROPPED_ROWS.get(folder, ())}
        # (the clones for the advanced page say ON|OFF where the controls page says YES|NO: a switch's state word)
        actual = {w.get('setting'): (w.get('values'),
                                     expected[w.get('setting')][1] if w.get('name').endswith('_spinner_advanced')
                                     and w.get('strings') == 'ON|OFF' and expected.get(w.get('setting'), ('', ''))[1] == 'YES|NO'
                                     else w.get('strings'))
                  for w in output.iter('widget') if w.get('setting')}
        assert actual == expected
        retained = {e.get('run') for e in source.iter('on') if e.get('run')} - {
            'port settings save', 'port settings defaults',
            'controls screen change set',
            'controls screen defaults', 'controls screen init',
            'mouse emit accept event'}  # (the PC page's button click: a bar action takes clicks by its slot)
        assert retained <= {e.get('run') for e in output.iter('on') if e.get('run')} | {
            action.get('run') for action in output.iter('action') if action.get('run')}
        assert {'b': 'back', 'a': 'select'}.items() <= bar_actions(output.find('widget')).items(), folder
        assert all('\\' not in w.get('name') for w in output.iter('widget'))
    video = ET.parse(MENUS / 'xbox/settings.video_settings.xml')
    assert not [w for w in video.iter('widget') if w.get('setting') == 'display.resolution_scaling']
    assert not [c for c in video.iter('child') if 'resolution_scaling' in c.get('widget', '')]
    hub = ET.parse(MENUS / 'xbox/settings.xml')
    assert {w.get('text') for w in hub.iter('widget') if w.find('on[@open]') is not None} == {
        'CONTROLS', 'VIDEO', 'GRAPHICS', 'AUDIO', 'NETWORK', 'PROFILES', 'KEYBOARD & MOUSE'}
    keyboard = next(w for w in hub.iter('child') if w.get('widget') == 'xbox/settings/keyboard')
    assert keyboard.get('platform') == 'keyboard'


def test_missing_fallback_is_rejected(tmp_path):
    module = generator()
    root = ET.fromstring('<menus><bitmap name="bad"><frame map="ui\\bad"/></bitmap></menus>')
    with pytest.raises(ValueError, match='fallback'):
        module.validate(root, tmp_path)


def test_generator_rejects_map_widget_definitions(tmp_path):
    module = generator()
    root = ET.fromstring('<menus><widget name="ui\\shell\\screen" /></menus>')
    with pytest.raises(ValueError, match='map widget'):
        module.validate(root, tmp_path)


def test_settings_apply_without_save_and_offer_defaults_on_x():
    module = generator()
    for path in (MENUS / 'xbox').glob('settings.*.xml'):
        if path.name == 'settings.about.xml':  # (nothing to apply or reset)
            continue
        widgets = {w.get('name'): w for w in ET.parse(path).iter('widget')}
        root = ET.parse(path).getroot()
        widgets = {w.get('name'): w for w in root.iter('widget')}
        screen = root.find('widget')
        assert not any(w.get('text') == 'SAVE' for w in widgets.values())
        defaults = screen.find("bar/action[@slot='x']")
        assert defaults.get('scope') == 'global' and defaults.get('text') == 'DEFAULTS' and defaults.get('run')
        for event in ('b', 'back'):
            assert screen.find(f'on[@event="{event}"][@back="true"]') is not None
        captions = {w.get('text') for w in widgets.values() if w.get('type') == 'text'}
        captions |= {action.get('text') for action in screen.iterfind('bar/action')}
        assert {'SELECT', 'DEFAULTS', 'BACK'} <= captions
        assert not any(text and '=' in text for text in captions)
        # (the bar's X runs on the key, a click and a tap alike: no second handler on the screen)
        assert screen.find('on[@event="x"]') is None
        for spinner in root.iter('widget'):
            if not spinner.get('setting'):
                continue
            for event in ('left', 'right', 'stick_left', 'stick_right'):
                assert spinner.find(f'on[@event="{event}"][@run="port setting changed"]') is not None
            assert spinner.find('on[@event="deleted"]') is None
        if 'video_settings' in path.name:
            assert screen.find('on[@event="deleted"][@run="port settings close"]') is not None


def test_hosting_spinners_apply_on_change_without_an_ok_action():
    for page in ('server_settings', 'coop_options'):
        root = ET.parse(MENUS / f'xbox/main_menu.multiplayer_type_select.{page}.xml')
        for spinner in root.iter('widget'):
            if spinner.get('setting'):
                for event in ('left', 'right', 'stick_left', 'stick_right'):
                    assert spinner.find(f'on[@event="{event}"][@run="port setting changed"]') is not None
                assert spinner.find('on[@event="deleted"]') is None
        assert not any(e.get('run') == 'coop options save' for e in root.iter('on'))


@pytest.mark.parametrize('path', sorted((MENUS / 'xbox').glob('settings.*_*.xml')))
def test_option_rows_end_above_the_help_text(path):
    root = ET.parse(path).getroot()
    help_top = None
    row_ys = []
    for widget in root.iter('widget'):
        name = widget.get('name', '')
        if name.endswith('/help'):
            help_top = int(widget.get('top'))
        if name.endswith('/options_menu'):
            row_ys = sorted({int(child.get('y')) for child in widget.findall('child')
                             if child.get('y') and not child.get('widget').endswith('/button_bar')
                             and '/scroll_' not in child.get('widget')})
            # (the places the list's rows take, which the leading rows' own y values may skip past)
            if len(row_ys) > 1:
                row_ys = list(range(row_ys[0], row_ys[-1] + 1, row_ys[1] - row_ys[0]))
            # a list that scrolls shows visible_rows of its rows at once
            row_ys = row_ys[:int(widget.get('visible_rows', len(row_ys)))]
    if help_top is not None:
        assert max(row_ys) + 28 <= help_top


def test_game_map_frames_keep_their_png_next_to_the_map_bitmap():
    root = ET.parse(MENUS / 'xbox/bitmaps.xml').getroot()
    frames = [frame for frame in root.iter('frame') if frame.get('map')]
    assert frames
    assert all(frame.get('png') for frame in frames)


MP_FILES = tuple('main_menu.multiplayer_type_select.' + name for name in (
    'join_game', 'join_game.filters', 'join_game.port', 'direct_ip', 'server_settings', 'coop_options', 'lobby')) + (
    'main_menu.quit_select', 'main_menu.new_select.port')


def test_multiplayer_files_preserve_pc_wiring_and_widget_names():
    module = generator()
    for filename in MP_FILES:
        source = ET.parse(LOOKS / 'sources' / (filename + '.xml'))
        path = MENUS / 'xbox' / (filename + '.xml')
        assert path.is_file(), filename
        output = ET.parse(path)
        widgets = {w.get('name'): w for w in output.iter('widget')}
        for original in source.iter('widget'):
            name = 'xbox/' + original.get('name')
            if filename.endswith('.join_game'):
                name = name.replace('/join_game/', '/join_game_legacy/')
            # Quit's original text panel sits inside a new confirmation modal.
            if name == 'xbox/main_menu/quit_select/quit_screen':
                name = 'xbox/main_menu/quit_select/quit_panel'
            if filename.endswith('coop_options') and name.endswith(('/button_bar', '/button_ok')):
                continue
            # DIRECT IP's JOIN GAME is a row of the list, not a button bar.
            # Its password row is gone (the PASSWORD screen asks after connecting).
            if filename.endswith('direct_ip') and (name.endswith('_button_bar') or 'server_password' in name):
                continue
            copied = widgets[name]
            for tag, key in (('on', 'run'), ('data', 'input')):
                if original.get('setting') and filename.endswith(('server_settings', 'coop_options')) and tag == 'on':
                    assert copied.find('on[@event="created"][@run="port setting load"]') is not None
                    continue
                expected = [(e.get('event'), e.get(key)) for e in original.findall(tag)]
                actual = [(e.get('event'), e.get(key)) for e in copied.findall(tag)]
                if '/lobby/lobby_button_' in name or name.endswith('/lobby/lobby_screen'):
                    # The distinct shortcuts are checked by the lobby action test.
                    if tag == 'on':
                        continue
                assert all(item in actual for item in expected), (name, expected, actual)
            if '/header_' in original.get('name'):
                # (their size is their picture's: tools/test_xbox_titles.py)
                for key in ('left', 'top'):
                    assert copied.get(key) == original.get(key)
                if original.get('bitmap'):
                    expected_bitmap = ('xbox/browser/header_sort_filter' if name.endswith('/filters/header_filters')
                                       else 'xbox/' + original.get('bitmap'))
                    assert copied.get('bitmap') == expected_bitmap
        module.validate(output.getroot(), MENUS)


def browser_c_widgets():
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml')
    prefix = 'xbox/main_menu/multiplayer_type_select/join_game/'
    return root, {w.get('name').removeprefix(prefix): w for w in root.iter('widget')
                  if w.get('name').startswith(prefix)}


def test_browser_c_has_six_large_rows_chips_and_detail_panel():
    root, widgets = browser_c_widgets()
    rows = widgets['join_game_items_list'].findall('child')
    assert len(rows) == 6
    assert widgets['join_game_items_list'].get('visible_rows') == '6'
    assert [(int(c.get('x')), int(c.get('y'))) for c in rows] == [
        (24, 110 + 46 * i) for i in range(6)]
    bitmaps = {b.get('name'): b for b in root.iter('bitmap')}
    for index in range(1, 7):
        row = widgets[f'server_item_{index}']
        assert (row.get('width'), row.get('height')) == ('270', '43')
        assert row.get('bitmap') == 'xbox/browser/c_row'
        assert row.find('on[@event="a"][@run="gamespy select item"]') is not None
        for frame in bitmaps[row.get('bitmap')]:
            assert (frame.get('width'), frame.get('height')) == ('270', '50')
    assert [c.get('widget').rsplit('/', 1)[-1] for c in widgets['browser_chips'].findall('child')] == [
        'browser_chip_all', 'browser_chip_multiplayer', 'browser_chip_coop', 'browser_filter']
    for key in ('browser_chip_all', 'browser_chip_multiplayer', 'browser_chip_coop', 'browser_filter'):
        assert widgets[key].get('height') == '54'
        run = 'port browser filters open' if key == 'browser_filter' else 'port browser chip select'
        assert widgets[key].find('on[@event="a"][@run="%s"]' % run) is not None
    panel = widgets['browser_detail']
    assert (panel.get('left'), panel.get('top'), panel.get('width'), panel.get('height')) == ('304', '110', '312', '273')
    assert all(len(name.rsplit('/', 1)[-1]) <= 31 for name in widgets)
    assert widgets['browser_connection_status'].get('height') == '26'
    assert widgets['join_game_screen'].get('type') == 'column_list'
    assert widgets['browser_detail'].get('type') == 'column_list'
    assert [c.get('widget').rsplit('/', 1)[-1] for c in panel.findall('child')] == [
        'join_game_button_join', 'browser_detail_content', 'browser_detail_status']
    content = widgets['browser_detail_content']
    assert content.get('flags') == 'no_focused_child'
    assert (content.get('left'), content.get('top'), content.get('width'), content.get('height')) == (
        '304', '110', '312', '273')
    names = {c.get('widget').rsplit('/', 1)[-1] for c in content.findall('child')}
    assert {'lobby_map_pic', 'replay_level_right_pic', 'browser_connection_status',
            'browser_rules_label', 'browser_detail_rules', 'browser_detail_name'} <= names
    assert 'browser_detail_status' not in names
    assert not content.findall('data')
    assert widgets['join_game_button_join'].get('height') == '45'
    assert widgets['join_game_button_join'].get('bitmap') == 'xbox/browser/join'
    assert not {'join_game_header', 'browser_filters', 'join_game_button_refresh'} & widgets.keys()
    assert widgets['server_item_server_name'].get('font') == r'ui\large_ui'
    assert widgets['server_item_map'].get('font') == r'ui\small_ui'
    assert len(bitmaps['xbox/browser/gauge'].findall('frame')) == 33
    assert len(bitmaps['xbox/browser/gametype'].findall('frame')) == 6
    # The detail panel's gametype picture: six gametypes plus the "?" frame, beside the map picture.
    assert len(bitmaps['xbox/browser/gametype_large'].findall('frame')) == 7
    picture = widgets['browser_detail_gametype']
    assert picture.get('bitmap') == 'xbox/browser/gametype_large'
    assert (picture.get('width'), picture.get('height')) == ('124', '100')
    placed = {c.get('widget').rsplit('/', 1)[-1]: c for c in content.findall('child')}
    assert int(placed['lobby_map_pic'].get('x')) + 145 < int(placed['browser_detail_gametype'].get('x'))
    assert placed['browser_detail_gametype'].get('y') == placed['lobby_map_pic'].get('y')
    for leaf, art, count in (('lobby_map_pic', 'preview_mp', 20), ('replay_level_right_pic', 'preview_sp', 10)):
        assert widgets[leaf].get('bitmap') == 'xbox/browser/' + art
        frames = bitmaps['xbox/browser/' + art].findall('frame')
        assert len(frames) == count
        assert all((f.get('x'), f.get('y'), f.get('width'), f.get('height')) == ('0', '0', '265', '133')
                   for f in frames)
        assert (widgets[leaf].get('width'), widgets[leaf].get('height')) == ('124', '100')
        assert all(f.get('map') and f.get('png') for f in frames)
    assert [int(f.get('index')) for f in bitmaps['xbox/browser/preview_mp']] == list(range(13)) + [13] * 7
    for key in ('browser_chip_all', 'browser_chip_multiplayer', 'browser_chip_coop', 'browser_filter'):
        assert widgets[key + '_white'].get('color') == '#ffffff'
    assert 'up_down_tabs_children' not in widgets['join_game_items_list'].get('flags')



def test_browser_code_filled_text_has_no_definition_string():
    _, widgets = browser_c_widgets()
    static = {'browser_rules_label', 'browser_equipment_label', 'browser_host_label',
              'browser_ip_address_caption', 'browser_create_inter_caption', 'browser_back_caption'}
    dynamic = {name: w for name, w in widgets.items() if w.get('type') == 'text' and name not in static}
    assert {'browser_detail_name', 'browser_detail_status', 'browser_join_caption',
            'browser_chip_all_label', 'browser_chip_all_white', 'browser_filter_label',
            'server_item_players', 'server_item_capacity'} <= dynamic.keys()
    # widget_instance_render_text_box reloads a definition string every frame,
    # overwriting text_set even when that definition string is empty.
    for name, widget in dynamic.items():
        assert not {'text', 'string_list', 'strings'} & widget.attrib.keys(), name


def test_browser_gauge_text_fits_inside_ring():
    _, widgets = browser_c_widgets()
    row = widgets['server_item_1']
    positions = {c.get('widget').rsplit('/', 1)[-1]: (int(c.get('x')), int(c.get('y')))
                 for c in row.findall('child')}
    for leaf in ('server_item_players', 'server_item_capacity'):
        text = widgets[leaf]
        assert text.get('font') == r'ui\small_ui'
        assert text.get('align') == 'center'
        assert int(text.get('width')) == 34
        assert positions[leaf][0] + 17 == positions['server_item_gauge'][0] + 21
    assert (positions['server_item_players'][1] + int(widgets['server_item_players'].get('height'))
            <= positions['server_item_capacity'][1])
    # both lines sit inside the 42-unit ring, as a block centered on it (a three-digit capacity is the widest line)
    ring_top = positions['server_item_gauge'][1]
    top = positions['server_item_players'][1]
    bottom = positions['server_item_capacity'][1] + int(widgets['server_item_capacity'].get('height'))
    assert top >= ring_top + 3 and bottom <= ring_top + 42 - 4
    # (the glyphs draw below their boxes, so the block sits a little above the ring's center)
    assert abs((top + bottom) / 2 - (ring_top + 21)) <= 5


def test_browser_direct_link_keeps_the_legacy_table():
    root, _ = browser_c_widgets()
    prefix = 'xbox/main_menu/multiplayer_type_select/join_game_legacy/'
    rows = root.find(f"widget[@name='{prefix}join_game_items_list']")
    assert rows.get('visible_rows') == '9'
    assert len([c for c in rows.findall('child') if c.get('widget').rsplit('/', 1)[-1].removeprefix('server_item_').isdigit()]) == 9
    patches = ET.parse(MENUS / 'xbox/patches.xml')
    direct = patches.find('.//row[@name="multiplayer_type_join_direct"]')
    assert direct.get('open') == prefix + 'join_game_screen'


def test_browser_retains_map_names_at_the_c_updaters_lookup_paths():
    shared = ET.parse(MENUS / 'xbox/multiplayer.shared.xml')
    original = ET.parse(LOOKS / 'sources/strings.xml')
    for name in ('main_menu/map_list', 'main_menu/mp_map_list'):
        expected = original.find(f'strings[@name="{name}"]')
        actual = shared.find(f'strings[@name="xbox/{name}"]')
        assert actual is not None
        assert [e.get('text') for e in actual] == [e.get('text') for e in expected]


def test_browser_actions_are_a_bar_and_the_list_takes_only_its_own_keys():
    root, widgets = browser_c_widgets()
    actions = widgets['button_refresh_create_inter']
    # the screen-wide actions are the declared bar's (the engine places them and routes their keys), so no widget
    # of the screen handles X or Y itself
    assert not actions.findall('on[@event="x"]') and not actions.findall('on[@event="y"]')
    screen = widgets['join_game_screen']
    assert not screen.findall('on[@event="x"]') and not screen.findall('on[@event="y"]')
    bar = {a.get('slot'): a for a in screen.find('bar').findall('action')}
    assert bar['x'].get('run') == 'port browser ip open'
    assert bar['y'].get('run') == 'join controller to mp game'
    assert bar['b'].get('scope') == 'back'
    assert actions.get('flags') == 'pass_unhandled_to_focused_child'
    assert [c.get('widget').rsplit('/', 1)[-1] for c in actions.findall('child')] == [
        'join_game_items_list', 'browser_chips', 'browser_detail']
    rows = widgets['join_game_items_list']
    assert rows.find('on[@event="up"][@run="port browser rows up"]') is not None
    assert rows.find('on[@event="down"][@run="port browser rows down"]') is not None
    assert widgets['browser_chips'].find('on[@event="down"][@run="port browser chips navigation"]') is not None


def test_multiplayer_list_viewports_stop_before_help_and_footer():
    for filename in MP_FILES:
        root = ET.parse(MENUS / 'xbox' / (filename + '.xml'))
        widgets = {w.get('name'): w for w in root.iter('widget')}
        for w in widgets.values():
            if w.get('type') != 'column_list' or 'up_down_tabs_children' not in w.get('flags', ''):
                continue
            if not any(c.get('y') for c in w.findall('child')):
                continue
            # Long static lists need the runtime viewport scroller. Their
            # declared viewport, rather than a second page, bounds the rows.
            limit = 421 if '/join_game/' in w.get('name') else 414
            assert int(w.get('top', 0)) + int(w.get('height')) <= limit, w.get('name')
            desc = widgets.get(w.get('description'))
            if desc is not None and desc.get('type') == 'text':
                assert int(w.get('top', 0)) + int(w.get('height')) <= int(desc.get('top'))


def test_quit_uses_confirm_button_positions():
    root = ET.parse(MENUS / 'xbox/main_menu.quit_select.xml')
    modal = next(w for w in root.iter('widget') if w.get('name').endswith('/quit_screen'))
    for button, x in (('b', 196), ('a', 314)):
        placed = modal.find(f"child[@widget='xbox/main_menu/quit_select/legend/{button}_butn']")
        assert (int(placed.get('x')), int(placed.get('y'))) == (x, 176 + generator().DIALOG_HIT[1])
        target = root.find(f"widget[@name='{placed.get('widget')}']")
        assert (target.get('width'), target.get('height')) == ('112', '53')
    assert modal.find('on[@event="a"]').get('run') == 'main menu quit game'
    assert modal.find('on[@event="b"]').get('back') == 'true'



def test_selection_rows_end_above_footer():
    paths = ['main_menu.multiplayer_type_select.join_game',
             'main_menu.multiplayer_type_select.lobby', 'main_menu.new_select.port']
    for filename in paths:
        root = ET.parse(MENUS / 'xbox' / (filename + '.xml'))
        # The generated shared file owns the reused profile/campaign row widgets.
        shared = ET.parse(MENUS / 'xbox/multiplayer.shared.xml')
        widgets = {w.get('name'): w for doc in (root, shared) for w in doc.iter('widget')}
        for w in root.iter('widget'):
            for c in w.findall('child'):
                leaf = c.get('widget').rsplit('/', 1)[-1]
                if c.get('y') and (leaf.startswith('list_item_') or
                                   leaf.startswith('server_item_')):
                    row = widgets[c.get('widget')]
                    limit = 421 if '/join_game/' in c.get('widget') else 414
                    first = min(int(part.get('y')) for part in w.findall('child')
                                if part.get('y') and (part.get('widget').rsplit('/', 1)[-1].startswith('list_item_')
                                                    or part.get('widget').rsplit('/', 1)[-1].startswith('server_item_')))
                    if int(c.get('y')) < first + int(w.get('visible_rows', '64')) * generator().COMFORTABLE_PITCH:
                        assert int(c.get('y')) + int(row.get('top', 0)) + int(row.get('height')) <= limit


def test_multiplayer_definitions_are_unique_and_browser_colors_match_xbox():
    names = [(e.tag, e.get('name')) for p in (MENUS / 'xbox').glob('*.xml')
             for e in ET.parse(p).getroot() if e.get('name')]
    assert len(names) == len(set(names))
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml')
    for w in root.iter('widget'):
        if w.get('name').rsplit('/', 1)[-1] in ('server_item_1', 'server_item_server_name'):
            expected = '#FF0080FF' if '/join_game_legacy/' in w.get('name') else '#ffffff'
            assert w.get('color').lower() == expected.lower()


def window_rows(options, widgets):
    """The rows of a scrolling list that show at once: the first rows' slots
    of the column ui_widget.c lays out (visible_rows)."""
    rows = [c for c in options.findall('child') if c.get('y') and 'button_bar' not in c.get('widget')
            and '/scroll_' not in c.get('widget')]
    slots = sorted({int(c.get('y')) for c in rows})
    # (the places the list's rows take, which the leading rows' own y values may skip past)
    pitch = int(options.get('row_pitch', slots[1] - slots[0]))
    slots = list(range(slots[0], slots[-1] + 1, pitch))
    height = max(int(widgets[c.get('widget')].get('height')) for c in rows)
    window = int(options.get('visible_rows', len(slots)))
    return slots, window, slots[0] + (window - 1) * pitch + height


def shown_children(options):
    """The most rows of a list one platform shows: the Mac's VIDEO and every other platform's GRAPHICS share a slot."""
    children = options.findall('child')
    return max(len([c for c in children if c.get('platform') != hidden]) for hidden in ('mac_app', '!mac_app'))


def test_scrolling_lists_fit_above_their_help():
    seen = []
    for path in (MENUS / 'xbox').glob('*.xml'):
        files = list((MENUS / 'xbox').glob('*.xml'))
        widgets = {w.get('name'): w for p in files for w in ET.parse(p).iter('widget')}
        for w in list(ET.parse(path).getroot().iter('widget')):
            if w.get('type') != 'column_list' or 'visible_rows' not in w.attrib:
                continue
            slots, window, bottom = window_rows(w, widgets)
            # (the hub's description is a panel beside the rows, not help text under them)
            help_text = widgets.get(w.get('name').rsplit('/', 1)[0] + '/help')
            if not w.get('description') and w.get('name').startswith('xbox/settings/') and help_text is not None:
                # (ABOUT's help is fixed text, not a row description: its rows scroll above it)
                assert bottom <= int(help_text.get('top')), w.get('name')
                continue
            if not w.get('description') or w.get('name') == 'xbox/settings/settings_list':
                assert shown_children(w) <= window and bottom <= 414, w.get('name')
                continue
            description = widgets[w.get('description')]
            if description.get('type') != 'text':
                assert bottom <= 414
                continue
            help_top = int(description.get('top'))
            # (a settings list keeps its window when its rows fit: rows its code hides close up)
            assert (window < len(slots) or w.get('row_pitch')) and bottom <= help_top, w.get('name')
            seen.append(w.get('name'))
    assert any(name.endswith('network_setup/options_menu') for name in seen)


def test_server_settings_and_network_rows_scroll_above_the_shared_help():
    # Server Setup and Network use the same help top and scroll the remaining rows.
    widgets = {w.get('name'): w for w in ET.parse(
        MENUS / 'xbox/main_menu.multiplayer_type_select.server_settings.xml').getroot().iter('widget')}
    options = next(w for w in widgets.values() if w.get('name').endswith('server_settings/options_menu'))
    slots, window, bottom = window_rows(options, widgets)
    assert int(options.get('visible_rows')) == 6
    assert bottom <= int(widgets[options.get('description')].get('top'))
    widgets = {w.get('name'): w for w in ET.parse(MENUS / 'xbox/settings.network_setup.xml').getroot().iter('widget')}
    options = next(w for w in widgets.values() if w.get('name').endswith('network_setup/options_menu'))
    slots, window, bottom = window_rows(options, widgets)
    assert 'visible_rows' in options.attrib
    assert bottom <= int(widgets[options.get('description')].get('top'))
    assert len(slots) > window


def test_only_column_lists_scroll():
    for path in MENUS.rglob('*.xml'):
        for w in ET.parse(path).getroot().iter('widget'):
            if 'visible_rows' in w.attrib:
                assert w.get('type') == 'column_list' and 1 <= int(w.get('visible_rows')) <= 64, w.get('name')


def test_scrolling_lists_have_the_game_arrows_beside_their_rows():
    for path in (MENUS / 'xbox').glob('*.xml'):
        widgets = {w.get('name'): w for w in ET.parse(path).getroot().iter('widget')}
        for w in widgets.values():
            # The browser updater pages its density's rows and shows the page number.
            if 'visible_rows' not in w.attrib or w.get('name').endswith('/join_game_items_list'):
                continue
            # A short list can use the viewport only to pack platform-filtered rows.
            if shown_children(w) <= int(w.get('visible_rows')):
                continue
            # (nor arrows for a settings list whose rows fit its window: it closes hidden rows up)
            slots = {c.get('y') for c in w.findall('child')
                     if c.get('y') and 'button_bar' not in c.get('widget') and '/scroll_' not in c.get('widget')}
            if w.get('row_pitch') and len(slots) <= int(w.get('visible_rows')):
                continue
            arrows = {c.get('widget').rsplit('/', 1)[-1]: c for c in w.findall('child') if '/scroll_' in c.get('widget')}
            assert set(arrows) == {'scroll_up_arrow', 'scroll_down_arrow'}, w.get('name')
            rows = [int(c.get('y')) for c in w.findall('child')
                    if c.get('y') and 'button_bar' not in c.get('widget') and '/scroll_' not in c.get('widget')]
            up, down = int(arrows['scroll_up_arrow'].get('y')), int(arrows['scroll_down_arrow'].get('y'))
            assert up == min(rows) and up < down
            window = int(w.get('visible_rows'))
            slots = sorted(set(rows))
            pitch = slots[1] - slots[0]
            assert down <= min(rows) + window * pitch
            for leaf, frame in (('scroll_up_arrow', 'arrow_sm_up'), ('scroll_down_arrow', 'arrow_sm_down')):
                art = widgets[arrows[leaf].get('widget')]
                assert art.get('bitmap') == 'xbox/bitmaps/' + frame
                assert not art.findall('on')


def test_browser_stats_keep_runtime_string_aliases():
    root = ET.parse(MENUS / 'xbox/multiplayer.shared.xml')
    name = 'xbox/main_menu/multiplayer_type_select/join_game/join_game_ticker_labels'
    labels = root.find(f'strings[@name="{name}"]')
    assert labels is not None
    assert [s.get('text') for s in labels][6:9] == ['SERVERS:', 'PLAYERS:', 'PAGE']


def test_browser_join_button_draws_map_art_with_a_large_caption_target():
    root, widgets = browser_c_widgets()
    bitmaps = {b.get('name'): b for b in root.iter('bitmap')}
    icon = widgets['browser_join_icon']
    frame = bitmaps[icon.get('bitmap')].find('frame')
    assert frame.get('map') == 'ui\\shell\\bitmaps\\a_butn'
    assert (MENUS / frame.get('png')).is_file()
    assert (frame.get('width'), frame.get('height')) == ('32', '32')
    join = [c for c in widgets['browser_detail'].findall('child') if c.get('widget').endswith('join_game_button_join')][0]
    assert (int(join.get('x')), int(join.get('y'))) == (476, 398)
    assert 476 + int(widgets['join_game_button_join'].get('width')) == 616


def test_browser_highlight_has_one_shape_for_every_row():
    root, widgets = browser_c_widgets()
    rows = [widgets[f'server_item_{i}'] for i in range(1, 7)]
    assert len({r.get('bitmap') for r in rows}) == 1
    bitmap = root.find(f"bitmap[@name='{rows[0].get('bitmap')}']")
    for frame in bitmap.findall('frame'):
        assert (frame.get('width'), frame.get('height')) == ('270', '50')


def test_sort_filter_has_four_large_rows_reset_and_done():
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.filters.xml')
    widgets = {w.get('name').rsplit('/', 1)[-1]: w for w in root.iter('widget')}
    children = widgets['options_menu'].findall('child')
    assert [c.get('widget').rsplit('/', 1)[-1] for c in children] == [
        'op_sort', 'op_full', 'op_locked', 'op_unjoinable']
    assert [int(c.get('y')) for c in children] == [73, 131, 189, 247]
    for c in children:
        assert widgets[c.get('widget').rsplit('/', 1)[-1]].get('height') == '54'
    for key in ('sort', 'full', 'locked', 'unjoinable'):
        label, spinner = widgets[key + '_label'], widgets[key + '_spinner']
        assert label.get('font') == spinner.get('font') == r'ui\large_ui'
        assert label.get('text_y') == '16'
        # Spinner text uses its bounds, not the text-box offset.
        assert (spinner.get('top'), spinner.get('height'), spinner.get('text_y')) == ('16', '24', '0')
        assert spinner.get('align') == 'center'
        assert spinner.get('width') == '198'
        # Arrow centers are -22 and 220, whose midpoint is 99.
        assert int(spinner.get('width')) / 2 == (-22 + 220) / 2
        assert spinner.get('header_bounds') == '20 -28 34 -16'
        assert spinner.get('footer_bounds') == '20 214 34 226'
    assert widgets['filters_screen'].get('type') == 'column_list'
    assert widgets['sort_spinner'].get('strings') == 'MOST PLAYERS|NAME|MAP|GAME'
    assert widgets['filters_screen'].find('data[@input="port browser filters update"]') is not None
    bar = {a.get('slot'): a for a in widgets['filters_screen'].find('bar').findall('action')}
    assert (bar['x'].get('text'), bar['x'].get('run')) == ('RESET', 'port browser filters reset')
    assert (bar['b'].get('text'), bar['b'].get('scope')) == ('DONE', 'back')


def test_no_screen_opens_quit_but_the_dialog_stays():
    for folder in ('xbox',):
        for path in (MENUS / folder).glob('*.xml'):
            root = ET.parse(path)
            assert not [e.attrib for e in root.iter() if 'quit_select' in e.get('open', '')], path
            assert not [e.attrib for e in root.iter() if e.get('platform') == 'quit'], path
            if path.name != 'main_menu.quit_select.xml':
                assert not [e.attrib for e in root.iter('on') if e.get('run') in
                            ('main menu quit game', 'port quit game')], path
        dialog = ET.parse(MENUS / folder / 'main_menu.quit_select.xml')
        assert any(w.get('name', '').endswith('/quit_screen') for w in dialog.iter('widget'))


def test_quit_panel_matches_delete_confirmation_padding():
    root = ET.parse(MENUS / 'xbox/main_menu.quit_select.xml')
    panel = root.find("widget[@name='xbox/main_menu/quit_select/quit_panel']")
    assert (panel.get('left'), panel.get('top'), panel.get('width'), panel.get('height')) == ('192', '176', '239', '159')
    assert (panel.get('text_x'), panel.get('text_y')) == ('9', '36')
    assert panel.get('text') == r'Are you sure you\nwish to exit Halo?'
    modal = root.find("widget[@name='xbox/main_menu/quit_select/quit_screen']")
    title = modal.find("child[@widget='xbox/error/header_are_you_sure']")
    assert (title.get('x'), title.get('y')) == ('201', '180')
    assert panel.find('child') is None, 'absolute legend coordinates must not be nested in the text panel'


def test_redraws_are_rendered_svgs_wired_to_every_row_that_shows_them():
    """Description drawings ship as 4x RGBA PNGs; Xbox rows and hub slots share their sources."""
    manifest = json.loads((MENUS / 'menus.json').read_text())['files']
    for name in ('audio', 'video', 'internet', 'about'):
        svg = (MENUS / f'xbox/art/{name}.svg').read_text()
        assert 'width="512" height="256"' in svg
        png = (MENUS / f'xbox/art/{name}.png').read_bytes()
        assert png[:8] == b'\x89PNG\r\n\x1a\n' and png[24:26] == bytes((8, 6)), '8-bit RGBA'
        assert (int.from_bytes(png[16:20], 'big'), int.from_bytes(png[20:24], 'big')) == (2048, 1024)
        assert f'xbox/art/{name}.png' in manifest
        assert f'xbox/art/{name}.png' in generator().manifest_files(MENUS)
    patches = ET.parse(MENUS / 'xbox/patches.xml')
    wired = {row.get('name'): row.get('picture') for row in patches.iter('row') if row.get('picture')}
    assert wired['profile_item_video'] == 'png:xbox/art/video.png'
    assert wired['profile_item_audio'] == 'png:xbox/art/audio.png'
    assert wired['profile_item_network'] == 'png:xbox/art/internet.png'
    assert wired['multiplayer_type_internet'] == wired['multiplayer_type_join_direct'] == 'png:xbox/art/internet.png'
    hub = ET.parse(MENUS / 'xbox/settings.xml').getroot()
    frames = hub.find("bitmap[@name='xbox/settings/hub_pictures']")
    pngs = [frame.get('png') for frame in frames]
    assert pngs[1:5] == ['xbox/art/video.png', 'xbox/art/none.png', 'xbox/art/audio.png', 'xbox/art/internet.png']
    assert all(frame.get('cache') is None for frame in frames[1:5])
    # Both pictures use the player's map; the importer redraws the recognized keyboard.
    assert pngs[0] == 'xbox/art/none.png' and frames[0].get('cache')
    assert pngs[5] == 'xbox/art/none.png' and frames[5].get('cache') is None
    assert pngs[6] == 'xbox/art/none.png' and frames[6].get('cache') == 'cache/xbox/v1/keyboard.png'
    bitmaps = ET.parse(MENUS / 'xbox/bitmaps.xml').getroot()
    assert not {'xbox/pictures/video', 'xbox/pictures/network'} & {b.get('name') for b in bitmaps}


def hub_panel(tmp_path, redraws=()):
    """The hub generated into a copy of the art folder holding only the named pictures (and none.png)."""
    assets = tmp_path / 'menus'
    (assets / 'xbox/art').mkdir(parents=True)
    shutil.copy(MENUS / 'xbox/art/none.png', assets / 'xbox/art/none.png')
    for name in redraws:
        shutil.copy(MENUS / f'xbox/art/{name}.png', assets / f'xbox/art/{name}.png')
    root = generator().settings_hub(set(), assets)
    return root, root.find("bitmap[@name='xbox/settings/hub_pictures']")


def test_hub_has_the_profile_editors_framed_panel():
    hub = ET.parse(MENUS / 'xbox/settings.xml').getroot()
    options = hub.find("widget[@name='xbox/settings/settings_list']")
    assert options.get('description') == 'xbox/settings/description'
    assert options.find('data').get('input') == 'plyr prof edit select menu upd8'
    panel = hub.find("widget[@name='xbox/settings/description']")
    assert (panel.get('left'), panel.get('top'), panel.get('width'), panel.get('height')) == ('275', '78', '320', '331')
    assert panel.get('bitmap') == 'xbox/panel_a/list_field'
    # the description update takes the first child for the text and the second for the picture
    assert [c.get('widget').rsplit('/', 1)[-1] for c in panel.findall('child')] == [
        'description_text', 'description_picture']
    label = hub.find("widget[@name='xbox/settings/profile_label']")
    assert label.find('data').get('input') == 'get edit plyr profile name'
    # a row's text, picture and place agree wherever a platform hides a row
    rows = options.findall('child')
    strings = hub.find("strings[@name='xbox/settings/hub_descriptions']")
    frames = hub.find("bitmap[@name='xbox/settings/hub_pictures']")
    assert [c.get('platform') for c in rows] == [s.get('platform') for s in strings] == [
        f.get('platform') for f in frames]
    # (GRAPHICS' panel names its mode there, in the profile name's place, while it is chosen)
    assert all(s.get('text').endswith(('Current Profile:', 'Graphics:')) for s in strings)


def test_picture_slot_resolves_redraw_then_cache_then_nothing(tmp_path):
    # no redraw for the others: the player's cache over the transparent PNG (menu_files.c tries cache first)
    _, frames = hub_panel(tmp_path / 'a', redraws=('audio',))
    by_key = dict(zip(('controls', 'video', 'graphics', 'audio', 'network', 'profiles', 'keyboard'), frames))
    assert by_key['audio'].attrib == {'png': 'xbox/art/audio.png', 'width': '512', 'height': '256'}
    # GRAPHICS has no picture of the player's: a transparent one, so its panel is text only
    assert by_key['graphics'].attrib == {'png': 'xbox/art/none.png', 'width': '512', 'height': '256',
                                         'platform': '!mac_app'}
    for key in ('controls', 'video', 'network', 'keyboard'):
        frame = by_key[key]
        assert frame.get('cache') == f'cache/xbox/v1/{key}.png'
        assert frame.get('png') == 'xbox/art/none.png'
        # (ui.map has it: the main menu saves it into the cache, where a game map's hub finds it)
        assert frame.get('map', '').startswith('ui\\shell\\main_menu\\') and frame.get('index') is not None
    # a redraw shipped later takes the slot, with no cache named
    _, frames = hub_panel(tmp_path / 'b', redraws=('audio',))
    shutil.copy(MENUS / 'xbox/art/audio.png', tmp_path / 'b/menus/xbox/art/video.png')
    root = generator().settings_hub(set(), tmp_path / 'b/menus')
    video = root.find("bitmap[@name='xbox/settings/hub_pictures']")[1]
    assert video.get('png') == 'xbox/art/video.png' and video.get('cache') is None
    # the text-only fallback (no redraw, no cache) draws a transparent picture: the placeholder is clear, 512 by 256
    png = (MENUS / 'xbox/art/none.png').read_bytes()
    assert (int.from_bytes(png[16:20], 'big'), int.from_bytes(png[20:24], 'big')) == (512, 256)
    assert png[24:26] == bytes((8, 6))
    data = zlib.decompress(b''.join(body for kind, body in png_chunks(png) if kind == b'IDAT'))
    assert not any(data[1 + row * (1 + 512 * 4):][:512 * 4] != bytes(512 * 4) for row in range(256))


def png_chunks(png):
    offset = 8
    while offset < len(png):
        length = int.from_bytes(png[offset:offset + 4], 'big')
        yield png[offset + 4:offset + 8], png[offset + 8:offset + 8 + length]
        offset += 12 + length


def test_join_warning_takes_a_as_ok_like_the_xbox_confirmations():
    """The legend shows A beside OK and B beside CANCEL, so the modal itself takes A (join) and B (cancel), as the
    delete confirmations do. A focusable CANCEL under the A glyph turned A into cancel and every join into a no-op."""
    widgets = {w.get('name'): w for path in (MENUS / 'xbox').glob('*.xml')
               for w in ET.parse(path).iter('widget')}
    base = 'xbox/error/confirm_join_big'
    modal = widgets[base + '_modal_fullscreen']
    for event in ('a', 'start'):
        assert modal.find(f'on[@event="{event}"]').get('run') == 'port browser join big confirm'
    for event in ('b', 'back'):
        assert modal.find(f'on[@event="{event}"]').get('run') == 'port browser join big back'
    # (nothing inside the modal takes A or Start first: no focusable buttons)
    for element in modal.iter('child'):
        assert 'button' not in element.get('widget')
    assert not any(name.startswith(base + '_button') for name in widgets)
    panel = widgets[base]
    delete = widgets['xbox/error/confirm_delete_profile']
    assert panel.get('bitmap') == delete.get('bitmap')
    placed = lambda w: [(c.get('widget').rsplit('/', 1)[-1], c.get('x'), c.get('y')) for c in w.findall('child')]
    assert placed(panel)[:2] == [(name.replace('confirm_delete_profile', 'confirm_join_big'), x, y)
                                 for name, x, y in placed(delete)[:2]]
    text = widgets[base + '_text_box']
    reference = widgets['xbox/error/confirm_delete_profile_text_box']
    for attr in ('bitmap', 'width', 'height', 'text_x', 'text_y', 'font', 'color'):
        assert text.get(attr) == reference.get(attr)
    strings = {s.get('name'): s for path in (MENUS / 'xbox').glob('*.xml')
               for s in ET.parse(path).iter('strings')}
    assert strings[text.get('string_list')][int(text.get('string_index'))].get('text').replace(r'\n', ' ') == (
        'This game allows more players than this device runs smoothly. Join anyway?')
    for icon, caption, emitted in (('b', 'CANCEL', 'back'), ('a', 'OK', 'accept')):
        target = widgets[f'{base}/legend/{icon}_butn']
        assert (target.get('width'), target.get('height')) == ('112', '53')
        assert widgets[f'{base}/legend/{icon}_butn_glyph'].get('bitmap') == f'xbox/browser/{icon}_butn'
        label = widgets[f'{base}/legend/' + ('accept' if icon == 'a' else 'cancel')]
        assert label.get('text') == caption
        assert target.find('on[@event="left_mouse"]').get('run') == f'mouse emit {emitted} event'


def test_keyboard_picture_is_generated_at_import_without_a_shipped_png():
    assert not list(MENUS.rglob('keyboard.png'))
    assert 'xbox/art/keyboard.png' not in json.loads((MENUS / 'menus.json').read_text())['files']
    assert not (ROOT / 'tools/menu_keyboard_redraw.py').exists()
    redraw = (ROOT / 'port/ios/host/menu_picture_redraw.m').read_text()
    assert '@"keyboard"' not in redraw
    assert 'keyboard.png' not in (ROOT / 'port/ios/CMakeLists.txt').read_text()


def test_filters_title_sits_with_the_other_titles_and_the_count_inside_the_panel():
    base = 'xbox/main_menu/multiplayer_type_select/join_game'
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.filters.xml').getroot()
    screen = root.find(f"widget[@name='{base}/filters/filters_screen']")
    placed = {c.get('widget').rsplit('/', 1)[-1]: (c.get('x'), c.get('y')) for c in screen.findall('child')}
    # Both titles keep the standard position inside the shared content frame.
    assert placed['header_filters'] == (None, None)
    x, y = (int(v) for v in placed['browser_filter_status'])
    assert 44 <= y and y + 22 <= 73  # below the band line, above the first option row
    assert x + 276 <= 586  # right-aligned to the option rows' edge


def test_empty_browser_explains_on_the_left_and_keeps_the_detail_panel():
    base = 'xbox/main_menu/multiplayer_type_select/join_game'
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml').getroot()
    names = {w.get('name') for w in root.iter('widget')}
    screen = root.find(f"widget[@name='{base}/join_game_screen']")
    placed = {c.get('widget').rsplit('/', 1)[-1]: (int(c.get('x', 0)), int(c.get('y', 0))) for c in screen.findall('child')}
    for leaf in ('browser_list_status', 'browser_list_hint'):
        assert f'{base}/{leaf}' in names
        assert placed[leaf][0] == 36 and 82 <= placed[leaf][1] < 82 + 53 * 3  # inset from the list edges, where the first rows sit


def png_header_and_first_pixel(path):
    """An 8-bit RGBA PNG's size and its top-left pixel (row 0's first pixel is stored raw under every filter)."""
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    position, idat, size = 8, b'', None
    while position < len(data):
        length = int.from_bytes(data[position:position + 4], 'big')
        kind = data[position + 4:position + 8]
        body = data[position + 8:position + 8 + length]
        if kind == b'IHDR':
            size = (int.from_bytes(body[:4], 'big'), int.from_bytes(body[4:8], 'big'), body[8], body[9])
        elif kind == b'IDAT':
            idat += body
        position += 12 + length
    return size, tuple(zlib.decompress(idat)[1:5])


def test_title_icon_is_the_outline_helmet_drawn_at_four_times_its_size():
    module = generator()
    bitmaps = ET.parse(MENUS / 'xbox/bitmaps.xml').getroot()
    icon = bitmaps.find(f"bitmap[@name='{module.TITLE_ICON}']")
    frame = icon.find('frame')
    assert (frame.get('width'), frame.get('height')) == (str(module.TITLE_ICON_SIZE),) * 2 == ('36', '36')
    (width, height, depth, color), corner = png_header_and_first_pixel(MENUS / frame.get('png'))
    assert (width, height, depth, color) == (144, 144, 8, 6)
    # (the helmet is drawn from art/visr_title_icon.svg, which is in the checked-in set; the corner is transparent)
    assert (MENUS / 'xbox/art/visr_title_icon.svg').is_file()
    assert corner[3] == 0
    assert frame.get('png') in json.loads((MENUS / 'menus.json').read_text())['files']


def test_title_icon_drawn_by_the_name_the_generator_gives_it():
    """The game finds the icon by its tag name (the menus' tags are named pc\\<name>, with backslashes)."""
    module = generator()
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    tag = 'pc\\' + module.TITLE_ICON.replace('/', '\\')
    assert '"' + tag.replace('\\', '\\\\') + '"' in source
    # the titles' capitals (22 units tall from 33 units down, 30 units in) which the 36-unit picture's helmet matches
    assert 'size = 36 * scale_y;' in source and 'bounds->y0 + 44 * scale_y - size / 2' in source


@pytest.mark.parametrize('page', ('server_settings', 'coop_options'))
def test_hosting_rows_share_value_geometry_and_keep_a_list_action(page):
    module = generator()
    root = ET.parse(MENUS / 'xbox' / f'main_menu.multiplayer_type_select.{page}.xml')
    widgets = {w.get('name'): w for w in root.iter('widget')}
    base = 'xbox/' + module.MP + '/' + page
    options = widgets[base + '/options_menu']
    rows = [c for c in options.findall('child') if '/scroll_' not in c.get('widget')]
    assert not any(c.get('widget').endswith('/button_bar') for c in rows)
    action = widgets[rows[-1].get('widget')]
    if page == 'server_settings':
        assert action.get('name') == base + '/op_start_game'
        assert action.get('text') == 'START GAME'
        assert action.find('on[@event="a"]').get('run') == 'ss start game'
        assert action.get('bitmap') == widgets[base + '/op_coop_options'].get('bitmap')
    else:
        # (co-op rows apply as they change: the list ends with its last option)
        assert action.get('name') == base + '/op_fireteam'
        assert base + '/op_save' not in widgets and base + '/button_ok' not in widgets
    for c in rows:
        row = widgets[c.get('widget')]
        assert int(row.get('height')) == module.COMFORTABLE_PITCH == 39
        for inner in row.findall('child'):
            value = widgets[inner.get('widget')]
            leaf = value.get('name').rsplit('/', 1)[-1]
            if leaf.endswith(('_value', '_spinner')):
                assert value.get('font') == r'ui\large_ui'
                assert value.get('color') == '#FFFFFFFF'
                # (spinners take a standard width; every value is centered on the screen's shared spinner line)
                width = int(value.get('width'))
                if value.get('type') == 'spinner':
                    assert width in {w for _, w, _ in module.SPINNER_WIDTHS}
                else:
                    assert (inner.get('x'), value.get('left'), width) == ('264', '3', 250)
                assert abs(int(inner.get('x')) + int(value.get('left')) + width / 2 - 391.5) <= 1
                label = widgets[value.get('name').rsplit('_', 1)[0] + '_label']
                label_position = row.find(f"child[@widget='{label.get('name')}']")
                assert int(inner.get('y')) + int(value.get('top')) + int(value.get('text_y')) == (
                    int(label_position.get('y', 0)) + int(label.get('text_y')))
                if value.get('type') == 'spinner':
                    left = int(value.get('left'))
                    assert value.get('header_bounds') == f'7 {left - 9} 19 {left - 3}'
                    assert value.get('footer_bounds') == f'7 {left + width + 3} 19 {left + width + 9}'
    if page == 'coop_options':
        # (every co-op row applies as it changes, so the legend has nothing to select)
        assert base + '/' + page + '_screen/legend/accept' not in widgets
    else:
        accept = widgets[base + '/' + page + '_screen/legend/accept']
        assert accept.get('text') == 'SELECT'


def test_map_screen_uses_only_player_counts_and_keeps_campaign_descriptions():
    base = 'xbox/main_menu/multiplayer_type_select/mp_map_select'
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.mp_map_select.xml')
    data = root.find(f'widget[@name="{base}/mp_map_right_data"]')
    counts = root.find(f'strings[@name="{data.get("string_list")}"]')
    assert counts is not None
    source = ET.parse(LOOKS / 'sources/strings.xml').find(
        'strings[@name="main_menu/multiplayer_type_select/mp_map_select/map_data"]')
    assert len(counts) == len(source)
    for actual, original in zip(counts, source):
        lines = original.get('text').split(r'\n')
        expected = [line.strip() for line in lines if line.strip().endswith(' players')]
        assert actual.get('text') == (expected[0] if expected else '')
    assert counts[6].get('text') == '2-8 players'  # Chill Out keeps its capacity.
    panel = root.find(f'widget[@name="{base}/mp_map_right_item"]')
    assert panel.find('child[@widget="xbox/main_menu/solo_level_select/replay_level_right_data"]') is not None
    shared = ET.parse(MENUS / 'xbox/multiplayer.shared.xml')
    campaign = shared.find('strings[@name="xbox/main_menu/solo_level_select/map_data"]')
    pc_campaign = ET.parse(LOOKS / 'sources/strings.xml').find(
        'strings[@name="main_menu/solo_level_select/map_data"]')
    assert [s.get('text') for s in campaign] == [s.get('text') for s in pc_campaign]


@pytest.mark.parametrize('look', ('sources', 'xbox'))
def test_server_setup_rows_invite_and_gametype_editor(look):
    base = ('xbox/' if look == 'xbox' else '') + 'main_menu/multiplayer_type_select/server_settings'
    root = ET.parse(LOOKS / look / 'main_menu.multiplayer_type_select.server_settings.xml')
    widgets = {w.get('name'): w for w in root.iter('widget')}
    options = widgets[base + '/options_menu']
    screen = widgets[base + '/server_settings_screen']
    rows = [c.get('widget').rsplit('/', 1)[-1] for c in options.findall('child')
            if '/scroll_' not in c.get('widget')]
    assert [r for r in rows if r != 'op_coop_options'] == [
        'op_server_name', 'op_max_players', 'op_game_type', 'op_gametype_options',
        'op_listing', 'op_password', 'op_start_game']
    assert [r for r in rows if r not in ('op_game_type', 'op_gametype_options')] == [
        'op_server_name', 'op_max_players', 'op_coop_options', 'op_listing', 'op_password', 'op_start_game']
    assert not any(base + '/' + r in widgets for r in (
        'op_invite', 'op_map_share', 'op_player_options', 'op_item_options', 'op_vehicle_options',
        'op_indicator_options', 'op_team_options', 'op_friendly_fire', 'op_extra_enemies',
        'op_enemies_per_player', 'op_enemies_multiplier', 'op_player_collisions', 'op_ally_friendly_fire', 'op_fireteam'))
    invite = widgets[base + '/invite_value']
    assert invite.get('color') == '#FFFFFFFF'
    assert int(invite.get('height')) >= 53
    assert invite.find('on[@event="left_mouse"]').get('run') == 'ss copy invite'
    assert 'invite_value' not in rows
    position = screen.find(f'child[@widget="{base}/x_butn_invite"]')
    assert position is not None
    hit = widgets[position.get('widget')]
    assert hit.find(f'child[@widget="{base}/invite_value"]') is not None
    assert hit.get('height') == invite.get('height')
    assert hit.get('bitmap') is None
    assert hit.find('on[@event="left_mouse"]').get('run') == 'ss copy invite'
    help_panel = widgets[options.get('description')]
    assert int(help_panel.get('top')) + int(help_panel.get('height')) <= int(position.get('y'))
    assert screen.find('on[@event="x"]').get('run') == 'ss copy invite'
    editor = widgets[base + '/op_gametype_options']
    for event in ('a', 'start'):
        handler = editor.find(f'on[@event="{event}"]')
        assert handler.get('run') == 'port setup edit'
        target = 'main_menu/settings_select/multiplayer_setup/playlist_edit/gametype_edit_screen'
        assert handler.get('open') == ('ui\\shell\\' + target.replace('/', '\\') if look == 'xbox' else target)
    assert widgets[base + '/op_game_type'].find(f'child[@widget="{base}/game_type_value"]') is not None
    if look == 'xbox':
        legend = base + '/server_settings_screen/legend'
        assert widgets[legend + '/copy'].get('text') == 'INVITE'
        keys = [screen.find(f'child[@widget="{legend}/{key}_butn"]') for key in ('x', 'b', 'a')]
        assert int(position.get('x')) + int(invite.get('width')) <= int(keys[0].get('x'))
        for i, key in enumerate(keys):
            target = widgets[key.get('widget')]
            assert int(target.get('height')) >= 53
            assert int(key.get('x')) + int(target.get('width')) <= (int(keys[i + 1].get('x')) if i < 2 else 640)
        assert widgets[legend + '/x_butn'].find('on[@event="left_mouse"]').get('run') == 'ss copy invite'
    # Hard-coded help and label indices remain available to the runtime.
    import port_settings
    generated_text = '\n'.join(port_settings.multiplayer_files()['main_menu.multiplayer_type_select.server_settings.xml'])
    generated = ET.fromstring(generated_text)
    for name in ('labels', 'help_strings'):
        expected = generated.find(f'strings[@name="main_menu/multiplayer_type_select/server_settings/{name}"]')
        without_ids = lambda e: {k: v for k, v in e.attrib.items() if k != 'id'}
        assert [without_ids(e) for e in root.find(f'strings[@name="{base}/{name}"]')] == [without_ids(e) for e in expected]


def test_merged_rows_and_bindings():
    import port_settings
    BASE = 'main_menu/multiplayer_type_select/coop_options/'
    NAME = 'main_menu.multiplayer_type_select.coop_options.xml'
    for look in ('sources', 'xbox'):
        prefix = 'xbox/' if look == 'xbox' else ''
        base = prefix + BASE
        root = ET.parse(LOOKS / look / NAME).getroot()
        widgets = {w.get('name'): w for w in root.findall('widget')}
        rows = widgets[base + 'options_menu'].findall('child')
        assert [c.get('widget').removeprefix(base) for c in rows] == [
            'op_friendly_fire', 'op_extra_enemies', 'op_player_collisions', 'op_fireteam',
            ] + ([] if look == 'xbox' else ['button_bar'])
        assert [int(c.get('y')) for c in rows[:4]] == ([73, 112, 151, 190] if look == 'xbox' else [73, 103, 133, 163])
        assert all(widgets[base + name].get('height') == ('39' if look == 'xbox' else '28') for name in (
            'op_friendly_fire', 'op_extra_enemies', 'op_player_collisions', 'op_fireteam'))
        for key, expected in (
            ('friendly_fire', ['OFF', 'PLAYERS ONLY', 'AI ALLIES ONLY', 'ON']),
            ('extra_enemies', ['NONE'] + [f'+{x}% PER PLAYER' for x in port_settings.COOP_ENEMIES_PERCENTAGES]
             + [f'{x}X' for x in port_settings.COOP_ENEMIES_MULTIPLIERS])):
            spinner = widgets[base + key + '_spinner']
            assert spinner.get('strings').split('|') == expected
            assert spinner.get('width') == '206'
            assert spinner.get('setting') is None and spinner.get('values') is None
            assert spinner.find("on[@event='created']") is None
            assert spinner.find("on[@event='left_mouse']").get('run') == 'mouse spinner 1wide click'
            if look == 'xbox':
                for event in ('left', 'right', 'stick_left', 'stick_right'):
                    assert spinner.find(f"on[@event='{event}']").get('run') == 'coop options changed'
        assert widgets[base + 'coop_options_screen'].find("on[@event='created']").get('run') == 'coop options init'
        if look == 'sources':
            action = widgets[base + 'button_ok']
            for event in ('a', 'start'):
                assert action.find(f"on[@event='{event}']").get('run') == 'coop options save'
        assert all(not any(w.get('name').endswith('/' + name) for w in widgets.values())
                   for name in ('op_ally_friendly_fire', 'op_enemies_per_player', 'op_enemies_multiplier'))


def test_settings_bars_offer_defaults_on_x_and_no_focusable_buttons():
    """Bug class: a screen-wide action built as a focusable list item (DEFAULTS was a row of the options list, so the
    d-pad stopped on it and touch drew no pill). Every settings page declares X DEFAULTS as a global bar action, and
    no button row is left in its options list."""
    for path in (MENUS / 'xbox').glob('settings.*.xml'):
        if path.name == 'settings.about.xml':
            continue
        root = ET.parse(path).getroot()
        widgets = {w.get('name'): w for w in root.iter('widget')}
        screen = root.find('widget')
        # (each change applies and saves as it is made: no SAVE, no Y)
        assert not any(name.endswith(('/button_ok', '/button_defaults', '/button_bar')) for name in widgets), path
        assert screen.find('on[@event="y"]') is None, path
        assert bar_actions(screen) == {'x': 'global', 'b': 'back', 'a': 'select'}, path
        run = screen.find("bar/action[@slot='x']").get('run')
        assert run in ('port settings defaults apply', 'port controls defaults'), path
        assert not [c for w in widgets.values() for c in w.findall('child')
                    if c.get('widget').rsplit('/', 1)[-1] in ('button_bar', 'button_defaults', 'button_ok')], path

def test_generated_menu_text_and_names_do_not_contain_equals():
    for path in (MENUS / 'xbox').glob('*.xml'):
        for element in ET.parse(path).iter():
            for attr in ('text', 'name', 'widget'):
                assert '=' not in element.get(attr, ''), (path, element.attrib)


def test_legend_touch_rectangles_fit_without_overlapping():
    widgets = {w.get('name'): w for path in (MENUS / 'xbox').glob('*.xml')
               for w in ET.parse(path).iter('widget')}
    def origin(name, seen=()):
        assert name not in seen
        for widget in widgets.values():
            for c in widget.findall('child'):
                if c.get('widget') == name:
                    px, py = origin(widget.get('name'), (*seen, name))
                    return px + int(c.get('x', 0)), py + int(c.get('y', 0))
        return 0, 0

    parents = {}
    for name, widget in widgets.items():
        for c in widget.findall('child'):
            parents.setdefault(c.get('widget'), []).append(name)
    def screens(name, seen=()):
        if name in seen:
            return set()
        if name.endswith('screen'):
            return {name}
        return {screen for parent in parents.get(name, []) for screen in screens(parent, (*seen, name))}
    screen_rectangles = {}
    count = 0
    for parent in widgets.values():
        children = parent.findall('child')
        rectangles = []
        for icon in children:
            name = icon.get('widget')
            if name.rsplit('/', 1)[-1] not in ('a_butn', 'b_butn', 'x_butn', 'y_butn', 'left_thumb_butn'):
                continue
            art = widgets[name]
            if (parent.find('on[@event="left_mouse"]') is not None
                    and int(parent.get('height', 0)) >= 53):
                continue  # The explicit container target owns the glyph and caption.
            if art.findall('child') or parent.get('name').rsplit('/', 1)[-1] in (
                    'a_butn', 'b_butn', 'x_butn', 'y_butn'):
                continue  # a tall target that holds its own caption and mouse handler
            x, y = int(icon.get('x', 0)), int(icon.get('y', 0))
            right = x + int(art.get('width', 0))
            labels = [c for c in children if widgets[c.get('widget')].get('type') == 'text'
                      and int(c.get('y', 0)) == y and 0 <= int(c.get('x', 0)) - right < 17]
            assert len(labels) == 1, name
            label = labels[0]
            end = int(label.get('x')) + int(widgets[label.get('widget')].get('width'))
            # The C helper pads by eight horizontally and centers a 60-unit height.
            width = max(60, end - x + 16)
            parent_x, parent_y = origin(parent.get('name'))
            x += parent_x
            end += parent_x
            y += parent_y
            rect = (x - (width - (end - x)) // 2, y - 17, x - (width - (end - x)) // 2 + width, y + 43)
            assert 0 <= rect[0] < rect[2] <= 640, (name, rect)
            assert 0 <= rect[1] < rect[3] <= 480, (name, rect)
            for previous in rectangles:
                assert rect[0] >= previous[2] or previous[0] >= rect[2] or rect[1] >= previous[3] or previous[1] >= rect[3], (name, rect, previous)
            rectangles.append(rect)
            for screen in screens(parent.get('name')):
                for previous in screen_rectangles.get(screen, []):
                    assert rect[0] >= previous[2] or previous[0] >= rect[2] or rect[1] >= previous[3] or previous[1] >= rect[3], (screen, name, rect, previous)
                screen_rectangles.setdefault(screen, []).append(rect)
            count += 1
    # (the placed legends left; declared bars are placed by the engine and checked by
    # test_declared_bar_captions_fit_their_slots and the touch legend probe)
    assert count > 7


def test_declared_bar_captions_fit_their_slots():
    """Bug class: a caption that does not fit its button (CREATE GAME), and two actions in one place. Every declared
    bar action's caption fits its slot in the bar's caption font, both as a touch pill (the caption and the minimum
    padding) and beside its controller glyph, and a screen declares each slot once."""
    import menu_geometry
    import menu_text_metrics

    count = 0
    for path in sorted((MENUS / 'xbox').glob('*.xml')):
        for screen in ET.parse(path).iter('widget'):
            for bar in screen.findall('bar'):
                slots = [action.get('slot') for action in bar.findall('action')]
                assert len(slots) == len(set(slots)), (path.name, screen.get('name'))
                for action in bar.findall('action'):
                    x0, x1 = menu_geometry.BAR_SLOT_BOUNDS[action.get('slot')]
                    room = x1 - x0 - 2 * menu_geometry.BAR_INSET - menu_geometry.BAR_CAPTION_OFFSET
                    # (every caption a run-time one can show)
                    for caption in (action.get('text') or action.get('strings')).split('|'):
                        width = menu_text_metrics.text_width(menu_geometry.BAR_FONT, caption)
                        assert width <= room, (screen.get('name'), caption, width, room)
                        assert width + 2 * 12 <= x1 - x0, (screen.get('name'), caption)
                    count += 1
    assert count >= 25
    # the slots run left to right X, Y, B, A, apart, inside the screen
    bounds = [menu_geometry.BAR_SLOT_BOUNDS[slot] for slot in 'xyba']
    assert all(left[1] + menu_geometry.BAR_GAP <= right[0] for left, right in zip(bounds, bounds[1:]))
    assert bounds[0][0] >= 0 and bounds[-1][1] <= 640


def test_every_legend_button_has_a_key_and_action():
    widgets = {w.get('name'): w for path in (MENUS / 'xbox').glob('*.xml')
               for w in ET.parse(path).iter('widget')}
    parents = {}
    for name, widget in widgets.items():
        for c in widget.findall('child'):
            parents.setdefault(c.get('widget'), []).append(name)

    def ancestors(name, seen):
        if name in seen:
            return
        seen.add(name)
        for parent in parents.get(name, []):
            ancestors(parent, seen)

    def descendants(name, seen):
        if name in seen:
            return
        seen.add(name)
        for c in widgets[name].findall('child'):
            if c.get('widget') in widgets:
                descendants(c.get('widget'), seen)

    header = (ROOT / 'port/linux/include/halo_controller_glyphs.h').read_text()
    import re
    keys = dict((int(button), label) for button, label in
                re.findall(r'X\((\d+), SDL_SCANCODE_\w+, "([^"]+)"\)', header))
    buttons = dict(a=0, b=1, x=2, y=3, left_thumb=14)
    keys[14] = 'R'
    count = 0
    for name in widgets:
        leaf = name.rsplit('/', 1)[-1]
        if leaf not in tuple(button + '_butn' for button in buttons):
            continue
        button = leaf.removesuffix('_butn')
        assert keys[buttons[button]]
        family = set()
        ancestors(name, family)
        graph = set()
        for root in family:
            if not parents.get(root):
                descendants(root, graph)
        events = {event.get('event') for n in graph for event in widgets[n].findall('on')}
        # B without a screen-specific handler uses the engine's built-in back action.
        assert button in events or button == 'b', name
        count += 1
    assert count > 40


def test_lobby_actions_have_distinct_live_shortcuts_and_glyphs():
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.lobby.xml')
    widgets = {w.get('name').rsplit('/', 1)[-1]: w for w in root.iter('widget')}
    expected = {'start': 'a', 'team': 'x', 'add': 'y', 'leave': 'b', 'share': 'left_thumb'}
    screen = widgets['lobby_screen']
    for action, button in expected.items():
        target = widgets['lobby_button_' + action]
        assert target.get('type') != 'text', action
        assert target.find(f'on[@event="{button}"]') is not None, action
        assert any(c.get('widget').endswith('/' + button + '_butn') for c in target.findall('child'))
        if action != 'start':  # A acts on the focused button only
            assert screen.find(f'on[@event="{button}"]') is not None
    assert len({screen.find(f'on[@event="{b}"]').get('run') for a, b in expected.items() if a != 'start'}) == 4
    assert not any('/legend/' in c.get('widget') for c in screen.findall('child'))
    assert screen.find('on[@event="start"][@run="port lobby join"]') is not None


def test_visible_action_bars_use_glyph_caption_pairs():
    widgets = {w.get('name'): w for path in (MENUS / 'xbox').glob('*.xml')
               for w in ET.parse(path).iter('widget')}
    parents = {}
    for name, w in widgets.items():
        for c in w.findall('child'):
            parents.setdefault(c.get('widget'), []).append(name)
    def screens(name, seen):
        if name in seen:
            return set()
        seen.add(name)
        result = {name} if name.endswith('_screen') else set()
        for parent in parents.get(name, []):
            result |= screens(parent, seen)
        return result
    for name, w in widgets.items():
        if 'button_bar' not in name.rsplit('/', 1)[-1] or not screens(name, set()):
            continue
        children = [widgets[c.get('widget')] for c in w.findall('child')]
        if any(c.get('name').endswith('_butn') for c in children):
            continue  # Confirmation bars already pair icons and captions.
        for action in children:
            assert any(c.get('widget').endswith('_butn') for c in action.findall('child')), action.attrib


# ---------- the graphics modes (display.graphics): the hub's row, the GRAPHICS screen, VIDEO's and the PC look's rows

def settings_widgets(path):
    return {w.get('name'): w for w in ET.parse(path).getroot().iter('widget')}


def test_hub_has_a_graphics_row_between_controls_and_audio_with_its_mode_in_the_status_line():
    hub = ET.parse(MENUS / 'xbox/settings.xml').getroot()
    options = hub.find("widget[@name='xbox/settings/settings_list']")
    rows = [(c.get('widget').rsplit('/', 1)[-1], c.get('platform')) for c in options.findall('child')]
    assert rows == [('controls', None), ('video', 'mac_app'), ('graphics', '!mac_app'), ('audio', None),
                    ('network', None), ('profiles', None), ('keyboard', 'keyboard')]
    # the Mac keeps VIDEO, which has the mode as its first row: GRAPHICS shares its slot elsewhere
    by_name = {c.get('widget').rsplit('/', 1)[-1]: c for c in options.findall('child')}
    assert by_name['graphics'].get('y') == by_name['video'].get('y')
    strings = hub.find("strings[@name='xbox/settings/hub_descriptions']")
    # (a blank line more than the text needs, so the status line sits where the other rows' do)
    assert [s.get('text') for s in strings if s.get('platform') == '!mac_app'] == [
        "The game's look.\\n\\n\\nGraphics:"]
    row = hub.find("widget[@name='xbox/settings/graphics']")
    # the row shows only its name, like the others: no mode word, no children
    assert row.get('text') == 'GRAPHICS' and row.find('child') is None
    assert row.find('on[@event="a"]').get('open') == 'xbox/settings/graphics/graphics_screen'
    assert hub.find("widget[@name='xbox/settings/graphics_state']") is None
    # the panel's status line: "Graphics:" ends the description, and the mode's word is a text the game fills in
    # (menu_functions.c's "port graphics state") in the profile name's place, which "port hub status" swaps while
    # GRAPHICS is the row chosen
    status = hub.find("widget[@name='xbox/settings/graphics_label']")
    assert status.find('data').get('input') == 'port graphics state'
    profile = hub.find("widget[@name='xbox/settings/profile_label']")
    placed = {c.get('widget'): c for c in hub.find("widget[@name='xbox/settings/settings_screen']").findall('child')}
    assert placed['xbox/settings/graphics_label'].get('y') == placed['xbox/settings/profile_label'].get('y')
    assert placed['xbox/settings/graphics_label'].get('platform') == '!mac_app'
    assert (status.get('font'), status.get('color')) == (profile.get('font'), profile.get('color'))
    assert [d.get('input') for d in options.findall('data')] == ['plyr prof edit select menu upd8', 'port hub status']


def test_graphics_screen_applies_each_change_with_the_modes_help():
    widgets = settings_widgets(MENUS / 'xbox/settings.graphics.xml')
    base = 'xbox/settings/graphics'
    options = widgets[f'{base}/options_menu']
    assert [c.get('widget').rsplit('/', 1)[-1] for c in options.findall('child')] == [
        'op_graphics', 'op_crt_effect']
    # the CRT effect is an Apple setting, shown by the game only while the mode row shows Hardcore
    assert options.findall('child')[1].get('platform') == 'apple'
    mode = widgets[f'{base}/graphics_spinner']
    assert mode.get('setting') == 'display.graphics'
    assert mode.get('strings') == 'HARDCORE|CLASSIC|ENHANCED' and mode.get('values') == 'hardcore|classic|enhanced'
    crt = widgets[f'{base}/crt_effect_spinner']
    assert (crt.get('setting'), crt.get('strings'), crt.get('values')) == ('display.crt_effect', 'ON|OFF', 'true|false')
    labels = ET.parse(MENUS / 'xbox/settings.graphics.xml').getroot().find(f"strings[@name='{base}/labels']")
    assert [s.get('text') for s in labels] == ['MODE:', 'CRT EFFECT:']
    # the help of the mode shown: the three texts of the design, and the CRT effect's under its row
    root = ET.parse(MENUS / 'xbox/settings.graphics.xml').getroot()
    helps = [s.get('text') for s in root.find(f"strings[@name='{base}/graphics_help_strings']")]
    flat = [' '.join(text.split('\\n')) for text in helps]
    assert flat[3] == flat[0] + ' In VR, Hardcore draws as Classic.'
    assert flat[:3] == [
        'The original Xbox exactly: 640x480 at 30 frames a second, nothing added.',
        "The original Xbox look at this device's resolution and frame rate, with sharper textures and HUD.",
        'Tastefully enhanced: the original game with soft shading where things meet the ground and shadows under '
        'props.']
    assert len(flat) == 4
    crt_help = [s.get('text') for s in root.find(f"strings[@name='{base}/help_strings']")][-1]
    assert ' '.join(crt_help.split('\\n')) == (
        'The picture as a CRT television showed it: scanlines, phosphor glow, and soft color.')
    # each change applies as it is made, as on the other settings screens: B just leaves, X restores the defaults
    screen = widgets[f'{base}/graphics_screen']
    assert screen.find('on[@event="b"]').get('back') == 'true'
    assert f'{base}/button_ok' not in widgets and f'{base}/button_defaults' not in widgets
    assert screen.find("bar/action[@slot='x']").get('run') == 'port settings defaults apply'
    assert screen.find("bar/action[@slot='b']").get('text') == 'BACK'
    for spinner in (mode, crt):
        assert spinner.find('on[@event="left"][@run="port setting changed"]') is not None
        assert spinner.find('on[@event="deleted"]') is None
    assert not any(on.get('run') == 'port settings save' for on in root.iter('on'))


def test_video_and_the_pc_looks_video_settings_have_the_mode_first_and_the_crt_row_beneath_it():
    for path, base in ((MENUS / 'xbox/settings.video_settings.xml', 'xbox/settings/video_settings'),
                       (LOOKS / 'sources/main_menu.settings_select.player_setup.player_profile_edit.video_settings.xml',
                        'main_menu/settings_select/player_setup/player_profile_edit/video_settings')):
        widgets = settings_widgets(path)
        rows = [c.get('widget').rsplit('/', 1)[-1] for c in widgets[f'{base}/options_menu'].findall('child')]
        assert rows[:3] == ['op_graphics', 'op_crt_effect', 'op_mode'], path
        assert widgets[f'{base}/graphics_spinner'].get('values') == 'hardcore|classic|enhanced'
        # a page's row is packed from the top, so the CRT effect's closing up when it is hidden leaves no gap
        assert widgets[f'{base}/options_menu'].get('row_pitch'), path


@pytest.mark.parametrize('look', ['sources', 'xbox'])
@pytest.mark.parametrize('page', ['video_settings', 'graphics'])
def test_hires_rows_are_absent_and_remaining_rows_keep_their_layout_and_strings(look, page):
    import port_settings

    base = f'xbox/settings/{page}' if look == 'xbox' else f'{port_settings.PE}/{page}'
    filename = f'settings.{page}.xml' if look == 'xbox' else base.replace('/', '.') + '.xml'
    root = ET.parse(LOOKS / look / filename).getroot()
    widgets = {w.get('name'): w for w in root.iter('widget')}
    assert not any('high_res_' in name for name in widgets)
    assert not any(w.get('setting') in ('display.high_res_hud', 'display.high_res_text')
                   for w in widgets.values())
    labels = [s.get('text') for s in root.find(f"strings[@name='{base}/labels']")]
    helps = [s.get('text') for s in root.find(f"strings[@name='{base}/help_strings']")]
    rows = port_settings.SCREENS[page]['rows']
    assert labels == [row[0] for row in rows]
    assert helps == [''] + [row[3].replace('\n', r'\n') for row in rows]
    options = widgets[f'{base}/options_menu']
    children = [c for c in options.findall('child') if c.get('widget').startswith(f'{base}/op_')]
    import xbox_menus
    dropped = 0
    if look == 'xbox':
        dropped = sum(1 for n in xbox_menus.XBOX_DROPPED_ROWS.get(page, ()) if n.startswith('op_'))
    assert len(children) == len(rows) - dropped
    slots = sorted({int(c.get('y')) for c in children})
    pitch = int(options.get('row_pitch'))
    assert slots == list(range(int(options.get('row_top')), slots[-1] + 1, pitch))
    visible = min(len(slots), int(options.get('visible_rows')))
    assert slots[visible - 1] + 28 <= int(widgets[f'{base}/help'].get('top'))
    assert int(widgets[f'{base}/help'].get('top')) + int(widgets[f'{base}/help'].get('height')) <= 414
    for index, (_, setting, *_rest) in enumerate(rows):
        key = setting.split('.', 1)[1]
        if look == 'xbox' and f'{key}_label' in xbox_menus.XBOX_DROPPED_ROWS.get(page, ()):
            continue
        assert widgets[f'{base}/{key}_label'].get('string_index') == str(index)


def test_rows_the_hardcore_mode_replaces_show_their_value_in_force_beside_their_spinner():
    """Every Video Settings row whose setting the mode table replaces has a text in its spinner's place that shows one
    of the spinner's own strings (menu_functions.c's graphics_rows_show chooses it and hides the spinner), and the
    screen has the line of help the game writes for a fixed or profile-set row"""
    import port_settings
    replaced = port_settings.graphics_replaced()
    video_settings = {row[1] for row in port_settings.SCREENS['video_settings']['rows']}
    assert {'display.frame_rate', 'display.interpolation'} <= replaced & video_settings
    for path, base in ((MENUS / 'xbox/settings.video_settings.xml', 'xbox/settings/video_settings'),
                       (LOOKS / 'sources/main_menu.settings_select.player_setup.player_profile_edit.video_settings.xml',
                        'main_menu/settings_select/player_setup/player_profile_edit/video_settings')):
        root = ET.parse(path).getroot()
        widgets = {w.get('name'): w for w in root.iter('widget')}
        assert widgets[f'{base}/row_help'].get('type') == 'text'
        assert widgets[f'{base}/row_help'].get('string_list') is None
        spinners = [w for w in widgets.values() if w.get('type') == 'spinner']
        for spinner in spinners:
            key = spinner.get('name').rsplit('/', 1)[-1].removesuffix('_spinner')
            overlay = widgets.get(f'{base}/{key}_replaced')
            assert (overlay is not None) == (spinner.get('setting') in replaced), spinner.get('setting')
            if overlay is not None:
                assert overlay.get('type') == 'text' and overlay.get('text') is None
                assert overlay.get('string_list') == f'{base}/{key}_shown'
                shown = [s.get('text') for s in root.find(f"strings[@name='{base}/{key}_shown']")]
                assert shown == spinner.get('strings').split('|')
                row = widgets[f'{base}/op_{key}']
                assert [c.get('widget').rsplit('/', 1)[-1] for c in row.findall('child')] == [
                    f'{key}_label', f'{key}_spinner', f'{key}_replaced']
    # the settings the mode does not replace keep their spinner alone
    assert 'display.vsync' not in replaced and 'display.graphics' not in replaced


def test_the_graphics_screen_is_shipped():
    manifest = json.loads((MENUS / 'menus.json').read_text())['files']
    for name in ('xbox/settings.graphics.xml',
                 'pictures/port/main_menu/settings_select/player_setup/player_profile_edit/graphics/header_graphics.png'):
        assert name in manifest, name


def test_profile_edit_has_a_graphics_row_where_there_is_no_video_row():
    patches = ET.parse(MENUS / 'xbox/patches.xml').getroot()
    row = next(r for r in patches.iter('row') if r.get('name') == 'profile_item_graphics')
    assert (row.get('platform'), row.get('text')) == ('!mac_app', 'GRAPHICS')
    assert row.get('open') == 'xbox/settings/graphics/graphics_screen'

def test_no_screen_draws_both_a_legend_and_its_own_button_bar():
    """A screen with a generated footer legend must not also draw the original
    button row (JOIN GAME / CANCEL, OK / BACK), or the actions show twice."""
    widgets = {w.get('name'): w for path in (MENUS / 'xbox').glob('*.xml')
               for w in ET.parse(path).iter('widget')}

    def reachable(name, seen):
        if name in seen or name not in widgets:
            return
        seen.add(name)
        for c in widgets[name].findall('child'):
            reachable(c.get('widget'), seen)

    duplicated = []
    for name in widgets:
        if not name.endswith('_screen'):
            continue
        seen = set()
        reachable(name, seen)
        has_legend = any('/legend/' in n for n in seen)
        has_bar = any(n.rsplit('/', 1)[-1] in ('button_bar', 'player_2_button_bar') for n in seen)
        if has_legend and has_bar:
            duplicated.append(name)
    assert duplicated == []


ERROR_SCREENS = tuple(f'error_{mode}_{size}screen' for mode in ('modal', 'nonmodal')
                      for size in ('full', 'half', 'qtr')) + ('error_modal', 'error_non_modal')
ABORT_SCREENS = ('error_abort_to_dashboard', 'error_abort_to_dashboard_you_have_no_choice')
WAIT_SCREENS = ('warning_creating_player_profile_screen', 'warning_saving_file_screen')


@pytest.mark.parametrize('name', ERROR_SCREENS + ABORT_SCREENS + WAIT_SCREENS +
                         ('confirm_delete_profile_modal_fullscreen', 'confirm_delete_gametype_modal_fullscreen'))
def test_original_dialogs_route_on_every_platform(name):
    assert (f'ui/shell/error/{name}', f'pc/xbox/error/{name}') in generator().SCREEN_ALWAYS


def test_main_menu_settings_opens_the_hub_not_the_profile_picker():
    always = dict(generator().SCREEN_ALWAYS)
    select = 'ui/shell/main_menu/settings_select/player_setup/player_profile_select_screen'
    assert always[select] == 'pc/xbox/settings/settings_screen'


def test_hub_profiles_row_reaches_the_picker_without_looping():
    module = generator()
    profiles = [r for r in module.HUB_ROWS if r[0] == 'profiles']
    assert len(profiles) == 1
    # A row open is a root open (`ui_widget_launch_widget` passes no parent), so the always-redirect applies to it:
    # the row must name the picker, and the game must skip the redirect while the hub is up.
    assert module.PROFILES_OPEN.endswith('player_profile_select_screen')
    redirects = {src.replace('/', '\\'): dst for src, dst in module.SCREEN_ALWAYS}
    assert redirects[module.PROFILES_OPEN] == 'pc/xbox/settings/settings_screen'
    source = (Path(__file__).parent.parent / 'port/linux/game/menu_tags.c').read_text()
    body = source[source.index('long pc_menu_screen_open('):]
    body = body[:body.index('\n}\n')]
    skip = body.index('if (from_hub && strcmp(name, target))')
    assert body.index('from_hub = menu_hub_open;') < body.index('menu_screen_equivalent(name)') < skip
    assert 'return tag_index;' in body[skip:skip + 120]
    # the flow stays set across the picker and the edit screen, so a chosen profile opens the editor and back returns
    assert 'menu_hub_open = TRUE;' in body[skip:skip + 120]
    edit = 'ui/shell/' + module.PE + '/player_profile_edit_screen'
    assert redirects[edit.replace('/', '\\')] == 'pc/xbox/settings/settings_screen'


@pytest.mark.parametrize('name', ERROR_SCREENS + ABORT_SCREENS + WAIT_SCREENS)
def test_generated_errors_preserve_runtime_text_and_pane_bounds(name):
    module = generator()
    root = module.dialog_document()
    widgets = {w.get('name'): w for w in root.findall('widget')}
    screen = widgets[f'xbox/error/{name}']
    frame = widgets[screen.find('child').get('widget')]
    panel_children = [widgets[c.get('widget')] for c in frame.findall('child')
                      if c.get('widget') in widgets]
    text = panel_children[0]
    assert text.get('type') == 'text'
    assert frame.get('width') == '239' and text.get('width') == '239'
    assert frame.get('bitmap') == 'xbox/bitmaps/alert_bkd'
    assert text.get('string_list') == 'xbox/strings/displayed_error_messages'
    assert text.find('replace[@search="%controller"][@function="controller"]') is not None
    width = 320 if 'qtrscreen' in name else 640
    height = 240 if any(s in name for s in ('halfscreen', 'qtrscreen')) else 480
    assert (int(screen.get('width')), int(screen.get('height'))) == (width, height)
    if name in WAIT_SCREENS:
        assert screen.get('auto_close') == '1000'
        assert screen.get('auto_close_fade') == '500'
        assert screen.find('on[@event="b"]').get('run') == 'NULL'
        assert not any('butn' in c.get('widget') for c in screen.findall('child'))
    else:
        for event in ('a', 'start'):
            assert screen.find(f'on[@event="{event}"]').get('close') == 'current'
        assert screen.find('on[@event="b"]') is None
        icon = widgets[f'xbox/error/{name}/a_butn']
        assert int(icon.get('height')) >= 53
        assert int(icon.get('width')) >= 53
        assert widgets[f'xbox/error/{name}/accept'].get('text') == 'OK'
    for w in widgets.values():
        assert 'dashboard' not in w.get('text', '').lower()


def test_browser_private_error_panel_keeps_its_direct_text_child():
    widgets = {w.get('name'): w for path in (MENUS / 'xbox').glob('*.xml')
               for w in ET.parse(path).iter('widget')}
    screen = widgets['xbox/' + generator().MP + '/join_game_legacy/gamespy_error_fullscreen']
    panel = widgets[screen.find('child').get('widget')]
    assert panel.get('name') == 'xbox/error/browser_error_panel'
    assert panel.find('child').get('widget') == 'xbox/error/error_text_box'


def test_dialog_ok_has_a_rounded_touch_button_and_adaptive_legend():
    root = generator().dialog_document()
    widgets = {w.get('name'): w for w in root.findall('widget')}
    for name in ERROR_SCREENS + ABORT_SCREENS:
        base = f'xbox/error/{name}'
        target = widgets[base + '/a_butn']
        assert target.find('data[@input="port dialog legend update"]') is not None
        assert widgets[base + '/a_butn/touch_background'].get('bitmap') == 'xbox/dialog_touch_button'
        caption = widgets[base + '/a_butn/touch_caption']
        assert caption.get('text') == 'OK' and caption.get('align') == 'center'


def test_delete_confirmation_has_adaptive_a_and_b_touch_targets():
    widgets = {w.get('name'): w for w in ET.parse(MENUS / 'xbox/multiplayer.shared.xml').iter('widget')}
    base = 'xbox/error/confirm_delete_profile/legend'
    for button, label in (('a', 'OK'), ('b', 'CANCEL')):
        target = widgets[f'{base}/{button}_butn']
        assert target.find('data[@input="port dialog legend update"]') is not None
        assert widgets[f'{base}/{button}_butn/touch_caption'].get('text') == label
        assert (target.get('width'), target.get('height')) == ('112', '53')


@pytest.mark.parametrize('platform', [None, 'apple', 'mac_app'])
def test_video_setup_rows_keep_the_leading_places_on_every_platform(platform):
    """The game lays a platform's surviving rows out by rank from the list's row_top and row_pitch, so those must
    match the first row and the comfortable pitch, and every row kept on iOS must survive (FRAME RATE on the Mac
    app)."""
    root = ET.parse(MENUS / 'xbox/settings.video_settings.xml').getroot()
    options = next(w for w in root.iter('widget') if w.get('name', '').endswith('video_settings/options_menu'))
    rows = [c for c in options.findall('child') if c.get('y') and '/op_' in c.get('widget')]
    kept = [c for c in rows if c.get('platform') in (None, platform)]
    names = [c.get('widget').rsplit('/', 1)[1] for c in kept]
    assert options.get('row_pitch') == '39'
    assert options.get('row_top') == rows[0].get('y')
    if platform is None:
        assert names == ['op_graphics', 'op_vsync', 'op_interpolation']
    if platform == 'mac_app':
        assert names == ['op_graphics', 'op_vsync', 'op_frame_rate', 'op_interpolation']


def test_frame_rate_spinner_fits_the_longest_value_inside_the_row():
    """The Mac app's DISPLAY choice reads "DISPLAY (240 HZ)" (menu_tags.c); the spinner needs about 10.5 units a
    character as the clipped 147-unit one showed, and both arrows must stay inside the 512-unit row."""
    root = ET.parse(MENUS / 'xbox/settings.video_settings.xml').getroot()
    spinner = next(w for w in root.iter('widget') if w.get('name', '').endswith('/frame_rate_spinner'))
    row = next(c for c in root.iter('child') if c.get('widget', '').endswith('/frame_rate_spinner'))
    width = int(spinner.get('width'))
    assert width >= len('DISPLAY (240 HZ)') * 10.5
    footer_end = int(row.get('x')) + int(spinner.get('left')) + int(spinner.get('footer_bounds').split()[3])
    assert footer_end <= 512


def spinner_rows(document):
    widgets = {w.get('name'): w for w in document.iter('widget')}
    for row in document.iter('widget'):
        for part in row.findall('child'):
            spinner = widgets.get(part.get('widget'))
            if spinner is not None and spinner.get('type') == 'spinner':
                yield row, part, spinner


def test_every_value_control_uses_a_standard_width_that_fits_its_longest_value():
    module = generator()
    standard = {width for _, width, _ in module.SPINNER_WIDTHS}
    documents = {p.name: ET.parse(p).getroot() for p in sorted((MENUS / 'xbox').glob('*.xml'))}
    longest = {id(s): n for s, n in module.spinner_longest_values(documents)}
    seen = 0
    for name, document in documents.items():
        if not name.endswith('.xml') or name in ('bitmaps.xml', 'patches.xml'):
            continue
        for row, part, spinner in spinner_rows(document):
            seen += 1
            width = int(spinner.get('width'))
            if '/join_game/filters/' in spinner.get('name'):
                continue  # the Sort & Filter screen draws its own 198-wide value slots
            assert width in standard, (name, spinner.get('name'), width)
            assert width / 2 >= module.SPINNER_MINIMUM_HALF_WIDTH, (name, spinner.get('name'), width)
            left = int(spinner.get('left', '0'))
            # (the arrows bracket the value box: bounds are relative to the widget origin, not the box's left edge)
            assert spinner.get('header_bounds') == f'7 {left - 9} 19 {left - 3}', (name, spinner.get('name'))
            assert spinner.get('footer_bounds') == f'7 {left + width + 3} 19 {left + width + 9}', (name, spinner.get('name'))
            if id(spinner) in longest:
                assert longest[id(spinner)] * module.SPINNER_UNITS_PER_CHARACTER <= width, (name, spinner.get('name'))
            reach = int(part.get('x', '0')) + int(spinner.get('left', '0')) + width + 9
            assert reach <= int(row.get('width')), (name, spinner.get('name'), reach)
    assert seen > 50


def test_in_game_help_toggle_lines_up_with_the_other_short_controls():
    root = ET.parse(MENUS / 'xbox/settings.gamepad_setup.xml').getroot()
    shorts = {(int(s.get('width')), p.get('x')) for _, p, s in spinner_rows(root)
              if s.get('strings') in ('ON|OFF', 'YES|NO')}
    assert len(shorts) == 1, shorts


def test_value_controls_on_a_screen_share_one_center_line():
    """The short, medium and wide controls of a screen are centered on one vertical line (within the half unit an odd
    width leaves), so values align and both arrow edges stagger symmetrically."""
    documents = {p.name: ET.parse(p).getroot() for p in sorted((MENUS / 'xbox').glob('*.xml'))}
    checked = 0
    for name, document in documents.items():
        centers = [int(part.get('x', '0')) + int(spinner.get('left', '0')) + int(spinner.get('width')) / 2
                   for _, part, spinner in spinner_rows(document)]
        if len(centers) > 1:
            checked += 1
            assert max(centers) - min(centers) <= 1, (name, min(centers), max(centers))
    assert checked >= 8


def test_profile_editor_removes_save_after_inserting_settings():
    module = generator()
    root = ET.parse(MENUS / 'xbox/patches.xml').getroot()
    edit = next(p for p in root.findall('patch') if p.get('list') == module.PEP + 'profile_edit_select_list')
    operations = list(edit)
    removes = [r for r in operations if r.tag == 'remove']
    assert operations[-2:] == removes
    assert [r.get('child') for r in removes] == [module.PEP + 'save_changes_profile_item', module.PEP + 'profile_edit_button_bar']
    assert {r.get('text') for r in operations if r.tag == 'row'} >= {'AUDIO', 'VIDEO', 'NETWORK'}
    assert not any(r.get('text') == 'SAVE CHANGES' for r in operations)
    # (other lists' patches may remove their own rows, e.g. the gametype editor's INDICATORS and the item screen's
    # STARTING EQUIPMENT, but none removes the profile editor's SAVE CHANGES)
    assert not any(r.get('child') == module.PEP + 'save_changes_profile_item'
                   for p in root.findall('patch') if p is not edit for r in p.findall('remove'))

@pytest.mark.parametrize('kind', ('password', 'code'))
def test_password_and_code_bar_captions_follow_the_row(kind):
    """A opens the keyboard until text is held, so the bar's A action has two captions (EDIT, JOIN GAME) that
    menu_functions.c's legend_accept_caption picks by slot through ui_widget_port_bar_caption; the focused row takes
    A. The password's X hides or shows the text; its key is a letter while typing, which the action declares
    (while_typing="pointer"), so no screen branches on how X arrived."""
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.port.xml').getroot()
    name = f'xbox/main_menu/multiplayer_type_select/join_game/{kind}/{kind}_screen'
    screen = next(w for w in root.iter('widget') if w.get('name') == name)
    actions = {a.get('slot'): a for a in screen.find('bar').findall('action')}
    assert actions['a'].get('strings') == 'EDIT|JOIN GAME' and actions['a'].get('index') == '1'
    assert actions['a'].get('focus') == 'true' and actions['a'].get('scope') == 'select'
    assert actions['b'].get('text') == 'CANCEL' and actions['b'].get('focus') == 'true'
    assert screen.find('on[@event="b"]').get('run') == f'port {kind} back'
    assert screen.find('on[@event="y"]') is None and screen.find('on[@event="x"]') is None
    # no placed legend: nothing is found by a leaf name any more
    assert not [w for w in root.iter('widget') if w.get('name', '').startswith(name + '/legend')]
    # JOIN GAME is its own row under the field; A on the field edits, A on the row joins
    options = next(w for w in root.iter('widget') if w.get('name') == name.rsplit('/', 1)[0] + '/options_menu')
    leaves = [c.get('widget').rsplit('/', 1)[1] for c in options.findall('child')]
    assert leaves == [f'op_{kind}', 'button_ok']
    ok = next(w for w in root.iter('widget') if w.get('name', '').endswith(f'/{kind}/button_ok'))
    assert ok.find('on[@event="a"]').get('run') == f'port {kind} join'
    field = next(w for w in root.iter('widget') if w.get('name', '').endswith(f'/op_{kind}'))
    assert field.find('on[@event="a"]').get('run') == f'port {kind} edit'
    source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    assert 'legend_accept_caption(list, join_row_focused(list))' in source
    assert 'ui_widget_port_bar_caption(screen, _gamepad_analog_button_a' in source
    assert 'named(screen, "accept"' not in source and 'named(screen, "x_hide"' not in source
    assert 'config_string("debug.menu_text_seed")' in source
    if kind == 'code':
        assert 'x' not in actions
        return
    x = actions['x']
    assert x.get('strings') == 'HIDE|SHOW' and x.get('scope') == 'global'
    assert x.get('run') == 'port password togglehide' and x.get('while_typing') == 'pointer'
    assert 'password_screen.hidden = FALSE' in source
    assert 'stars[index] = password_screen.hidden' in source
    assert 'togglehide click' not in source + (ROOT / 'port/linux/game/menu_tags.c').read_text()
    # the hosting Server Settings password stays masked
    assert 'text_field_begin_masked(row, multiplayer.game_password' in source


def test_a_bar_key_that_is_a_letter_waits_only_for_the_key():
    """while_typing="pointer" is data the engine reads (ui_bar_key_is_letter), for every screen: the key's press
    waits while a field is typed in, a tap or a click (event_manager_post_pointer_button) still runs."""
    engine = (ROOT / 'source/interface/ui_widget.c').read_text()
    check = engine[engine.index('static boolean ui_bar_key_is_letter(\n\tstruct widget_instance *screen,\n\tstruct event_record const *event)\n{'):]
    check = check[:check.index('\n}\n')]
    assert 'event->data.button.pointer' in check and 'HALO_MENU_TYPING_POINTER' in check
    assert 'ui_widget_port_text_editing()' in check
    dispatch = engine[engine.index('if (ui_widget_port_disabled_row_activation(widget, handler->event_type))'):]
    assert dispatch.index('ui_bar_key_is_letter(widget, event)') < dispatch.index('_event_handler_run_function_bit')
    # (a tap's or a click's press is marked; the port's own posted presses, ui_widget_port_post_button, are not)
    assert 'event_manager_post_pointer_button(claim_controller != NONE ? claim_controller : controller_index,\n\t\t\t\tui_mouse_presses[0]);' in engine


def layout_widgets(density):
    files = {p.name: p for p in (MENUS / 'xbox').glob('*.xml')}
    return {w.get('name'): w for p in files.values() for w in ET.parse(p).iter('widget')}


def screen_parts(screen, widgets, ancestors=(), x=0, y=0, screen_names=None):
    """Child placements translate descendants; a widget's own bounds do not."""
    if screen_names is None:
        screen_names = generator().SCREEN_LAYOUTS
    name = screen.get('name')
    assert name not in ancestors, name
    yield screen, x + int(screen.get('left', 0)), y + int(screen.get('top', 0))
    dependencies = list(screen.findall('child'))
    if screen.get('description') in widgets:
        dependencies.append(ET.Element('child', widget=screen.get('description')))
    for c in dependencies:
        target = widgets[c.get('widget')]
        # Nested screens such as the browser's error overlay have their own contract.
        if target.get('name') in screen_names and not name.endswith('_wrapper'):
            continue
        yield from screen_parts(target, widgets, (*ancestors, name),
                                x + int(c.get('x', 0)), y + int(c.get('y', 0)), screen_names)


def test_every_generated_screen_declares_a_layout_kind():
    module = generator()
    widgets = layout_widgets('comfortable')
    screens = {name for name in widgets if name.endswith(('_screen', 'fullscreen', 'halfscreen', 'qtrscreen',
                                                        '_wrapper', '/send_log_confirm'))}
    screens |= {'xbox/error/' + name for name in module.ABORT_DIALOGS + ('error_modal', 'error_non_modal')}
    for name in screens:
        assert name in module.SCREEN_LAYOUTS, f'{name}: missing layout kind'
        assert module.SCREEN_LAYOUTS[name]['kind'] in module.LAYOUT_KINDS, name
    assert set(module.SCREEN_LAYOUTS) == screens, 'layout declarations must name actual screens'
    assert {s['kind'] for s in module.NATIVE_SCREEN_LAYOUTS.values()} >= {'K3', 'K4', 'K9', 'K10', 'K11'}


@pytest.mark.parametrize('density', ('comfortable',))
def test_layout_kinds_control_help_rows_and_panels(density):
    module = generator()
    widgets = layout_widgets(density)
    for name, spec in module.SCREEN_LAYOUTS.items():
        if spec.get('legacy'):
            continue
        kind = module.LAYOUT_KINDS[spec['kind']]
        screen = widgets[name]
        for c in screen.findall('child'):
            w = widgets[c.get('widget')]
            if w.get('type') is None and w.get('name').rsplit('/', 1)[-1].startswith('header'):
                assert tuple(int(w.get(k)) for k in ('left', 'top', 'width', 'height')) == module.TITLE, name
                assert int(c.get('x', 0)) == int(c.get('y', 0)) == 0, name
        options = widgets.get(spec.get('options'))
        if options is not None:
            placed = module.density_rows(options, widgets)
            if spec['kind'] == 'K2-A':
                placed = [c for c in options.findall('child') if '/scroll_' not in c.get('widget')
                          and c.get('widget') in widgets and (module.is_picture_row(widgets[c.get('widget')])
                                                              or 'list_item_' in c.get('widget'))]
            ys = sorted({int(c.get('y')) for c in placed})
            pitch = spec.get('row_pitch', kind.get('row_pitch'))
            expected = [ys[0] + i * pitch for i in range(len(ys))]
            assert ys == expected, f'{name}: {density} row pitch {ys}, expected {expected}'
            if spec['kind'] == 'K1':
                base = spec['options'].rsplit('/', 1)[0]
                help_widgets = [w for n, w in widgets.items() if n.rsplit('/', 1)[0] == base
                                and n.rsplit('/', 1)[-1] in ('help', 'row_help', 'graphics_help',
                                                           'vehicle_options_help', 'teamplay_options_help')]
                for help_widget in help_widgets:
                    top = spec.get('help', (module.HELP_TOP,))[0]
                    assert int(help_widget.get('top')) == top, f'{name}: help top'
        if spec['kind'] in ('K2-A', 'K2-B'):
            expected = tuple(kind['panel']['panel'])
            panels = [w for w, _, _ in screen_parts(screen, widgets)
                      if w.get('bitmap', '').endswith('/list_field')]
            assert panels, f'{name}: missing panel'
            for w in panels:
                assert tuple(int(w.get(k)) for k in ('left', 'top', 'width', 'height')) == expected, name


@pytest.mark.parametrize('density', ('comfortable',))
def test_k8_panels_headers_and_adaptive_targets(density):
    module = generator()
    widgets = layout_widgets(density)
    for name, spec in module.SCREEN_LAYOUTS.items():
        if spec['kind'] != 'K8':
            continue
        screen = widgets[name]
        parts = list(screen_parts(screen, widgets))
        panels = [(w, x, y) for w, x, y in parts if w.get('bitmap') == 'xbox/bitmaps/alert_bkd']
        assert len(panels) == 1, f'{name}: one alert panel'
        frame, left, top = panels[0]
        width, height = int(screen.get('width')), int(screen.get('height'))
        expected = (192, 176) if (width, height) == (640, 480) else ((width - 239) // 2, (height - 212) // 2)
        assert (left, top, int(frame.get('width')), int(frame.get('height'))) == (*expected, 239, 159), name
        headers = [(w, x, y) for w, x, y in parts if w.get('name').rsplit('/', 1)[-1].startswith('header')
                   or w.get('name').endswith('/title')]
        assert len(headers) == 1 and headers[0][1:] == (left + 9, top + 4), f'{name}: header offset'
        targets = [(w, x, y) for w, x, y in parts
                   if w.find('data[@input="port dialog legend update"]') is not None]
        assert bool(targets) != spec.get('buttonless', False), f'{name}: buttonless wait'
        for target, x, y in targets:
            button = target.get('name').rsplit('/', 1)[-1][0]
            assert (x, y) == (left + (122 if button == 'a' else 4), top + generator().DIALOG_HIT[1]), name
            assert (target.get('width'), target.get('height')) == ('112', '53'), name
            # Targets sit within the panel's width; glyphs and captions must fit too.
            assert left <= x and x + 112 <= left + 239, f'{name}: target outside panel width'
            for c in target.findall('child'):
                w = widgets[c.get('widget')]
                if w.get('bitmap') == 'xbox/dialog_touch_button':
                    continue
                drawn_left = x + int(c.get('x', 0))
                assert left <= drawn_left and drawn_left + int(w.get('width')) <= left + 239, (
                    f'{name}: glyph or caption outside panel: {w.get("name")}')


def legend_items(screen, widgets):
    """Return one anchor per button, including explicit container targets."""
    parts = list(screen_parts(screen, widgets))
    result = []
    for w, x, y in parts:
        leaf = w.get('name').rsplit('/', 1)[-1]
        if not leaf.endswith('_butn') or leaf.removesuffix('_butn') not in ('a', 'b', 'x', 'y', 'left_thumb'):
            continue
        # Hosting's glyph inside its named target is counted by the target itself.
        if w.get('name').rsplit('/', 2)[-2] == leaf:
            continue
        result.append((leaf.removesuffix('_butn'), x, y))
    return result


@pytest.mark.parametrize('density', ('comfortable',))
def test_legends_share_y_and_spacing(density):
    module = generator()
    widgets = layout_widgets(density)
    for name, spec in module.SCREEN_LAYOUTS.items():
        if spec['kind'] == 'K8' or spec.get('legacy') or spec.get('entry_legend'):
            continue
        items = legend_items(widgets[name], widgets)
        if not items:
            continue
        assert len(items) == len({b for b, _, _ in items}), f'{name}: duplicate button actions'
        ordered = [b for b, _, _ in sorted(items, key=lambda item: item[1])]
        captions = legend_caption_widths(widgets[name], widgets, items)
        expected = spec.get('legend_targets', module.legend_positions(ordered, captions))
        # DEFAULTS stays in its separate global-action slot; B/A use the standard pair.
        if name.startswith('xbox/settings/') and 'defaults_x' in spec:
            expected = {'x': spec['defaults_x'], 'b': 326, 'a': 471}
        for button, x, y in items:
            assert (x, y) == (expected[button], 414), f'{name}: {button} legend at {(x, y)}'
    lobby = widgets[f'xbox/{module.MP}/lobby/lobby_list']
    placed = sorted(module.density_rows(lobby, widgets), key=lambda c: int(c.get('y')))
    # The roster scrolls; only its visible window must clear the legend.
    shown = placed[:int(lobby.get('visible_rows', len(placed)))]
    assert max(int(c.get('y')) + int(widgets[c.get('widget')].get('height')) for c in shown) <= 414, 'lobby: roster overlaps bar'


def test_send_log_keeps_send_action_and_adds_cancel():
    widgets = layout_widgets('comfortable')
    screen = widgets['xbox/settings/send_log_confirm']
    assert screen.find('on[@event="a"][@run="port send log"][@back="true"]') is not None
    assert screen.find('on[@event="b"][@back="true"]') is not None
    targets = [widgets[c.get('widget')] for c in screen.findall('child') if c.get('widget').endswith('_butn')]
    assert len(targets) == 2
    assert widgets['xbox/settings/send_log_confirm/legend/accept'].get('text') == 'SEND'


def legend_caption_widths(screen, widgets, items):
    """Return each legend button's caption width: the text widget drawn 22 units right of its glyph.

    Hosting's named targets draw the caption 4 units down inside the target.
    """
    texts = {(x, y): int(w.get('width')) for w, x, y in screen_parts(screen, widgets) if w.get('type') == 'text'}
    found = {}
    for b, x, y in items:
        for dy in (0, 4):
            if (x + 22, y + dy) in texts:
                found[b] = texts[(x + 22, y + dy)]
                break
    return found


def pill_spans(positions, widths):
    """Native touch pills span x-8 to x+22+caption+8 (ui_mouse_legend_bounds)."""
    return sorted((x - 8, x + 22 + widths.get(b, 90) + 8) for b, x in positions.items())


def assert_pills_fit(name, positions, widths):
    spans = pill_spans(positions, widths)
    assert spans[0][0] >= 0 and spans[-1][1] <= 640, f'{name}: legend pill off screen {spans}'
    for (_, right), (left, _) in zip(spans, spans[1:]):
        assert right <= left, f'{name}: legend pills overlap {spans}'


@pytest.mark.parametrize('buttons, widths, expected', (
    (('b', 'a'), {}, (326, 471)),
    (('y', 'b', 'a'), {'y': 100, 'b': 40, 'a': 90}, (272, 414, 496)),
    (('x', 'y', 'b', 'a'), {'x': 70, 'y': 110, 'b': 60, 'a': 90}, (130, 242, 394, 496)),
    (('x', 'y', 'left_thumb', 'b', 'a'), {'x': 100, 'y': 100, 'left_thumb': 55, 'b': 50, 'a': 90},
     (23, 165, 307, 404, 496)),
))
def test_shared_legend_slots_pack_by_caption_width(buttons, widths, expected):
    positions = generator().legend_positions(buttons, widths)
    assert tuple(positions[b] for b in buttons) == expected
    assert_pills_fit(buttons, positions, widths)


@pytest.mark.parametrize('density', ('comfortable',))
def test_generated_legend_pills_do_not_overlap(density):
    module = generator()
    widgets = layout_widgets(density)
    for name, spec in module.SCREEN_LAYOUTS.items():
        # The DEFAULTS slot sits in its own global-action place, checked by its own test.
        if spec['kind'] == 'K8' or spec.get('legacy') or spec.get('entry_legend') or 'defaults_x' in spec:
            continue
        items = legend_items(widgets[name], widgets)
        if len(items) < 2:
            continue
        captions = legend_caption_widths(widgets[name], widgets, items)
        assert set(captions) == {b for b, _, _ in items}, f'{name}: legend caption not found'
        positions = {b: x for b, x, _ in items}
        captions = legend_caption_widths(widgets[name], widgets, items)
        assert_pills_fit(name, positions, captions)


@pytest.mark.parametrize('density', ('comfortable',))
def test_legend_pills_clear_info_panels(density):
    """A list-field box ends at the bottom rule, not below it, where the touch pill sits (info panel, stats box)."""
    module = generator()
    widgets = layout_widgets(density)
    checked = 0
    for name, spec in module.SCREEN_LAYOUTS.items():
        if spec['kind'] == 'K8' or spec.get('legacy') or spec.get('entry_legend'):
            continue
        screen = widgets[name]
        items = legend_items(screen, widgets)
        if not items:
            continue
        widths = legend_caption_widths(screen, widgets, items)
        pill_top = module.LEGEND_Y - 17
        spans = pill_spans({b: x for b, x, _ in items}, widths)
        for w, x, y in screen_parts(screen, widgets):
            if 'list_field' not in w.get('bitmap', '') or not w.get('height'):
                continue
            right, bottom = x + int(w.get('width', 0)), y + int(w.get('height'))
            if bottom <= module.RULE_Y:
                checked += 1
            for left_edge, right_edge in spans:
                if right_edge <= x or left_edge >= right:
                    continue
                # the panel runs to the bottom rule, as the disc's do; the pill straddles the rule
                assert bottom <= module.RULE_Y, (
                    f'{name}: {w.get("name")} ends at y {bottom}, below the rule and over the legend pill {left_edge}..{right_edge}')
    assert checked, 'no info panels found above any legend'


@pytest.mark.parametrize('filename', ('direct_ip', 'join_game.filters'))
def test_other_branches_keep_their_layout_and_handlers(filename):
    name = f'main_menu.multiplayer_type_select.{filename}.xml'
    original = ET.parse(ROOT / 'port/assets/menus/xbox' / name).getroot()
    generated = ET.parse(MENUS / 'xbox' / name).getroot()

    def signature(element):
        attrs = tuple(sorted((k, v) for k, v in element.attrib.items() if k != 'id' and not k.endswith('_ids')))
        return element.tag, attrs, tuple(sorted(signature(c) for c in element))

    assert signature(generated) == signature(original), f'{filename}: changed another branch’s layout'


def test_pause_dialog_touch_pills_fit_inside_its_box():
    """The pause dialog's rows and legend come from the stock tags, which
    `pause_patch` (menu_tags.c) moves. Measured on the unraised layout, the
    touch legend's pills hung 12 units below the box and sat 10 units under
    the last row, with 50 empty units at the box's top. The raises must put
    the pills inside the box, clear the last row by at least as much as
    before, and stay within the top margin."""
    import re
    source = (ROOT / 'port/linux/game/menu_tags.c').read_text()
    def define(name):
        return int(re.search(rf'#define {name} (\d+)', source).group(1))
    list_raise, legend_raise = define('PAUSE_LIST_RAISE'), define('PAUSE_LEGEND_RAISE')
    overhang, row_gap, top_room = 12, 10, 50
    assert legend_raise > overhang
    assert row_gap + list_raise - legend_raise >= row_gap
    assert list_raise <= top_room - 20
    assert 'children[child].vertical_offset -= PAUSE_LIST_RAISE;' in source
    assert 'children[child].vertical_offset -= PAUSE_LEGEND_RAISE;' in source


def test_dialog_touch_buttons_sit_inside_their_dialog_box():
    """The OK (and CANCEL) pills hung below the 239x159 box, and
    the join-game error drew its box-less text on a black screen."""
    gen = generator()
    px, py, pw, ph = gen.DIALOG_PANEL
    bx, by, bw, bh = gen.DIALOG_BUTTON
    hx, hy, hw, hh = gen.DIALOG_HIT
    rule = 130  # the footer rule, relative to the panel; the panel ends at ph
    assert bw == 108 and by >= rule and by + bh <= ph  # drawn pill sits in the footer band, below the rule
    assert abs((by + bh / 2) - (rule + ph) / 2) <= 2  # and is centered in it
    assert hh >= 53  # tap area is at least 44 pt
    assert hx + hw <= pw
    assert hy <= by and by + bh <= hy + hh and hx <= bx and bx + bw <= hx + hw
    assert hx - gen.DIALOG_BUTTON_STEP >= 0
    assert hw <= gen.DIALOG_BUTTON_STEP  # B and A tap areas do not overlap
    bw, bh = hw, hh
    shared = ET.parse(MENUS / 'xbox/multiplayer.shared.xml').getroot()
    for name in ('confirm_delete_profile_modal_fullscreen', 'confirm_delete_gametype_modal_fullscreen'):
        modal = shared.find(f"widget[@name='xbox/error/{name}']")
        for c in modal.findall('child'):
            if c.get('widget').endswith(('/a_butn', '/b_butn')):
                assert px <= int(c.get('x')) and int(c.get('x')) + bw <= px + pw
    widgets = {w.get('name'): w for path in (MENUS / 'xbox').glob('*.xml') for w in ET.parse(path).iter('widget')}
    screen = widgets['xbox/' + gen.MP + '/join_game_legacy/gamespy_error_fullscreen']
    assert screen.get('bitmap') != 'xbox/bitmaps/black'
    assert widgets['xbox/error/error_text_box'].get('bitmap') == 'xbox/bitmaps/alert_bkd'


def test_panel_frames_fit_their_panels():
    """A frame is drawn at its own size, so the panel art's 1260x1288 outline (of 2048) must fill the panel's bounds."""
    module = generator()
    for key, spec in (('a', module.PANEL_A), ('b', module.PANEL_B)):
        width, height = module.panel_frame_size(spec['panel'])
        assert abs(width * module.FIELD_ART[0] / module.FIELD_PNG - spec['panel'][2]) < 0.5
        assert abs(height * module.FIELD_ART[1] / module.FIELD_PNG - spec['panel'][3]) < 0.5
        assert module.PANEL_BITMAPS[key].endswith('/list_field')


def gametype_editor():
    """(module, the generated gametype editor's widgets by name, its list)"""
    module = generator()
    document = ET.parse(MENUS / 'xbox/main_menu.settings_select.multiplayer_setup.playlist_edit.xml').getroot()
    widgets = {w.get('name'): w for w in document.iter('widget')}
    screen = widgets[module.GAMETYPE_EDIT_SCREEN]
    (options,) = [widgets[c.get('widget')] for c in screen.findall('child')
                  if widgets.get(c.get('widget'), ET.Element('x')).get('type') == 'column_list']
    return module, widgets, options


def test_the_game_s_gametype_editor_and_create_new_open_the_generated_one():
    """The game's editor and CREATE NEW's screen (which holds only that editor) both open ours, on every
    platform, so the game's rows (GAME TYPE, INDICATOR OPTIONS) are never shown."""
    module = generator()
    always = {source: target for source, target in module.SCREEN_ALWAYS}
    disc = 'ui/shell/main_menu/settings_select/multiplayer_setup/'
    for source in (disc + 'playlist_edit/gametype_edit_screen', disc + 'create_and_edit_new_gametype_screen'):
        assert always[source] == 'pc/' + module.GAMETYPE_EDIT_SCREEN


def test_gametype_editor_has_one_game_options_row_that_reaches_every_rules_page():
    """GAME OPTIONS (the game's GAME TYPE row, its picture and description kept) opens the shared game settings
    page and there is no GAME row; that page's TYPE row for each gametype opens its rules page, so no gametype's
    rules are unreachable."""
    module, widgets, options = gametype_editor()
    rows = [widgets[c.get('widget')] for c in options.findall('child')]
    texts = [r.get('text') for r in rows]
    assert 'GAME' not in texts and texts.count('GAME OPTIONS') == 1
    row = rows[texts.index('GAME OPTIONS')]
    page = {on.get('open') for on in row.findall('on') if on.get('event') in ('a', 'start')}
    assert page == {'xbox/main_menu/multiplayer_type_select/game_options_edit/game_options_screen'}
    # (the game's GAME TYPE row's picture and description: frame and string 1 of its lists)
    (entry,) = [e for e in module.GAMETYPE_EDIT_ROWS if e[1] == 'GAME OPTIONS']
    assert entry[5] == (module.GAMETYPE_OPTIONS_PICTURES, 1) and entry[6].startswith('Specify the type of game')
    opened = set()
    for widget in ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.game_options_edit.xml').getroot().iter(
            'widget'):
        if widget.get('name', '').rsplit('/', 1)[-1].startswith('op_type_'):
            opened.update(h.get('open') for h in widget.iter('on') if h.get('event') == 'a')
    for engine in ('ctf', 'koth', 'slayer', 'oddball', 'race'):
        assert any(o and o.endswith(f'playlist_edit/{engine}_edit/{engine}_options_screen') for o in opened), engine


def test_gametype_editor_panel_follows_the_rows_on_every_platform():
    """The description update shows string and frame N for the Nth row shown, so the descriptions and pictures are
    filtered by the same platforms as the rows, in the same order, and each row runs the game's handler it had
    (rename, save) or the port's (delete)."""
    module, widgets, options = gametype_editor()
    document = ET.parse(MENUS / 'xbox/main_menu.settings_select.multiplayer_setup.playlist_edit.xml').getroot()
    strings = document.find(f"strings[@name='{module.GAMETYPE_EDIT}/descriptions']")
    pictures = document.find(f"bitmap[@name='{module.GAMETYPE_EDIT}/pictures']")
    children = options.findall('child')
    assert [c.get('platform') for c in children] == [s.get('platform') for s in strings] == [
        f.get('platform') for f in pictures]
    assert len(children) == len(module.GAMETYPE_EDIT_ROWS) == 7
    assert [c.get('platform') for c in children].count('tvos') == 1  # (DELETE GAMETYPE)
    runs = {widgets[c.get('widget')].get('text'): {on.get('run') for on in widgets[c.get('widget')].findall('on')}
            for c in children}
    assert runs['CHANGE NAME'] == {'mp profile change name'}
    assert runs['SAVE CHANGES'] == {'mp profile save changes'}
    assert runs['DELETE GAMETYPE'] == {'port delete request'}
    screen = widgets[module.GAMETYPE_EDIT_SCREEN]
    assert screen.find("on[@event='created']").get('run') == 'close if not editing profile'
    # (every description ends on the line the gametype's name sits beside)
    assert all(s.get('text').endswith('This gametype:') and s.get('text').count('\\n') == 4 for s in strings)


def test_gametype_editor_rows_are_tall_picture_rows():
    """The rows are the standard tall picture row (the row_36 art, 39 units apart), as the settings hub's are."""
    module, widgets, options = gametype_editor()
    children = options.findall('child')
    ys = [int(c.get('y')) for c in children]
    assert ys == [ys[0] + i * 39 for i in range(len(ys))]
    # (each device's rows end above the panel's bottom rule: six, and the Apple TV's seven with DELETE GAMETYPE)
    for active in (set(), {'tvos'}, {'mac_app', 'keyboard'}):
        shown = [c for c in children if not c.get('platform') or (c.get('platform').lstrip('!') in active) != c.get(
            'platform').startswith('!')]
        assert max(int(c.get('y')) for c in shown) + 39 <= module.RULE_Y, active
    for c in children:
        row = widgets[c.get('widget')]
        assert row.get('height') == '39' and row.get('row') == 'tall', row.get('name')
        assert row.get('bitmap').startswith('xbox/bitmaps/row_36/list_item_bkd'), row.get('bitmap')


def test_every_title_picture_sets_its_title_at_the_same_height_and_size_inside_its_frame():
    """Bug class: a title picture whose text sits higher or lower in its 512x64 frame than the others draws above or
    below the title band (the gametype editor's, made from the game's one-line 512x32 header, drew half off the top
    of the screen). Every shipped header picture of the standard title size centers what it draws at one height."""
    # (stdlib only: CI has no Pillow or NumPy)
    from mac_run import read_png

    over = bytes(255 if value > 200 else 0 for value in range(256))
    seen = bytes(0 if value == 0 else 255 for value in range(256))

    def span(mask):
        """first and last index of a nonzero byte, or None"""
        first = len(mask) - len(mask.lstrip(b'\0'))
        return None if first == len(mask) else (first, len(mask.rstrip(b'\0')) - 1)

    centers, letters, edges = {}, {}, {}
    for frame in ET.parse(MENUS / 'xbox' / 'bitmaps.xml').getroot().iter('frame'):
        png = frame.get('png') or ''
        if png.rsplit('/', 1)[-1].startswith('header') and (frame.get('width'), frame.get('height')) == ('512', '64'):
            if (MENUS / png).is_file():
                width, height, bgra = read_png((MENUS / png).read_bytes())
                stride = width * 4
                rows = [bgra[y * stride:(y + 1) * stride] for y in range(height)]
                alpha = [row[3::4] for row in rows]
                drawn = [y for y in range(height) if span(alpha[y].translate(seen))]
                centers[png] = (drawn[0] + drawn[-1] + 1) / 2 / height
                # (and its letters: the bright blue of the text, at 4x)
                left, top, bottom = width, None, None
                for y, row in enumerate(rows):
                    mask = (int.from_bytes(row[0::4].translate(over), 'big') &
                            int.from_bytes(alpha[y].translate(over), 'big')).to_bytes(width, 'big')
                    found = span(mask)
                    if found:
                        left, top, bottom = min(left, found[0]), y if top is None else top, y
                letters[png] = (left, top, bottom)
                edges[png] = max(max(alpha[0]), max(alpha[-1]), max(a[0] for a in alpha), max(a[-1] for a in alpha))
    assert len(centers) >= 30
    reference = sorted(centers.values())[len(centers) // 2]
    assert {png: center for png, center in centers.items() if abs(center - reference) > 0.01} == {}
    # the same left edge and cap top, and the standard cap height unless the title is set smaller to fit (the game's
    # one-line header for the gametype editor was set at half the height)
    assert {png: box for png, box in letters.items()
            if not (110 <= box[0] <= 135 and 128 <= box[1] <= 138 and 70 <= box[2] - box[1] <= 92)} == {}
    # and its plate fades out inside the picture, on every side
    assert {png: edge for png, edge in edges.items() if edge > 4} == {}
