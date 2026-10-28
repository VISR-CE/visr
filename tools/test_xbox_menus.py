"""Xbox settings generation preserves wiring and needs no extracted assets."""
import importlib.util
import json
from pathlib import Path
import shutil
import sys
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


def snapshot(path):
    return {p.relative_to(path).as_posix(): p.read_bytes() for p in path.rglob('*') if p.is_file()}


def test_generation_is_deterministic_and_checked_in(tmp_path):
    assets = tmp_path / 'menus'
    shutil.copytree(MENUS / 'ce', assets / 'ce')
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
    # ce_menus passes its newly drawn PNG list through the same retention step.
    retained = module.retain_xbox_files(assets, ['ce/bitmaps.xml'])
    assert set(manifest) & {p for p in manifest if p.startswith('xbox/')} <= set(retained)
    assert 'ce/port/pause/pausebox_center__1.png' in retained


def test_settings_keep_pc_values_and_handlers():
    module = generator()
    for folder in module.PAGES:
        source = ET.parse(MENUS / 'ce' / f'{module.PE.replace("/", ".")}.{folder}.xml')
        output = ET.parse(MENUS / 'xbox' / f'settings.{folder}.xml')
        expected = {w.get('setting'): (w.get('values'), w.get('strings'))
                    for w in source.iter('widget') if w.get('setting')}
        actual = {w.get('setting'): (w.get('values'), w.get('strings'))
                  for w in output.iter('widget') if w.get('setting')}
        assert actual == expected
        assert {e.get('run') for e in source.iter('on') if e.get('run')} <= {
            e.get('run') for e in output.iter('on') if e.get('run')}
        names = {w.get('name').rsplit('/', 1)[-1] for w in output.iter('widget')}
        assert {'a_butn', 'b_butn', '=accept', '=cancel'} <= names
        assert all('\\' not in w.get('name') for w in output.iter('widget'))
    hub = ET.parse(MENUS / 'xbox/settings.xml')
    assert {w.get('text') for w in hub.iter('widget') if w.find('on[@open]') is not None} == {
        'CONTROLS', 'VIDEO', 'AUDIO', 'NETWORK', 'KEYBOARD & MOUSE'}
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


def test_save_is_a_focusable_action_separate_from_a_legend():
    for path in (MENUS / 'xbox').glob('settings.*.xml'):
        widgets = {w.get('name'): w for w in ET.parse(path).iter('widget')}
        save = next(w for n, w in widgets.items() if n.endswith('/button_ok'))
        # ui_mouse_key_button posts A to the existing focus; a SAVE item must
        # get the normal item target, which focuses it before posting A.
        assert save.get('type') == 'text'
        assert save.get('text') == 'SAVE'
        assert not save.findall('child')
        assert save.find('on[@event="a"][@back="true"]') is not None
        accept = next(w for n, w in widgets.items() if n.endswith('/=accept'))
        assert accept.get('text') == 'SELECT'


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
    'join_game', 'join_game.filters', 'join_game.port', 'direct_ip', 'server_settings', 'lobby')) + (
    'main_menu.quit_select', 'main_menu.new_select.port')


def test_multiplayer_files_preserve_pc_wiring_and_widget_names():
    module = generator()
    for filename in MP_FILES:
        source = ET.parse(MENUS / 'ce' / (filename + '.xml'))
        path = MENUS / 'xbox' / (filename + '.xml')
        assert path.is_file(), filename
        output = ET.parse(path)
        widgets = {w.get('name'): w for w in output.iter('widget')}
        for original in source.iter('widget'):
            copied = widgets['xbox/' + original.get('name')]
            for tag, key in (('on', 'run'), ('data', 'input')):
                assert [(e.get('event'), e.get(key)) for e in original.findall(tag)] == [
                    (e.get('event'), e.get(key)) for e in copied.findall(tag)]
            if '/header_' in original.get('name'):
                for key in ('left', 'top', 'width', 'height'):
                    assert copied.get(key) == original.get(key)
                if original.get('bitmap'):
                    assert copied.get('bitmap') == 'xbox/' + original.get('bitmap')
        module.validate(output.getroot(), MENUS)


def test_browser_uses_xbox_nine_row_list_and_description_panel():
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml')
    widgets = {w.get('name').rsplit('/', 1)[-1]: w for w in root.iter('widget')}
    rows = [c for c in widgets['join_game_items_list'].findall('child')
            if c.get('widget').rsplit('/', 1)[-1].removeprefix('server_item_').isdigit()]
    assert len(rows) == 9
    assert widgets['join_game_items_list'].get('visible_rows') == '9'
    # Match the viewport's row eligibility: descriptive containers must
    # not consume a server slot or change the first row's origin/pitch.
    eligible = []
    for c in widgets['join_game_items_list'].findall('child'):
        w = widgets[c.get('widget').rsplit('/', 1)[-1]]
        if w.get('type') != 'column_list' and (w.findall('on') or w.findall('child')):
            eligible.append(c)
    assert eligible == rows
    assert {'item_ticker_player_info', 'item_ticker_rules_info'} <= {
        c.get('widget').rsplit('/', 1)[-1]
        for c in widgets['join_game_items_list'].findall('child')}
    assert [(int(c.get('x')), int(c.get('y'))) for c in rows] == [
        (48, 108 + 24 * i) for i in range(9)]
    fields = ('server_name', 'map', 'type', 'players', 'ping')
    headers = widgets['join_game_header']
    header_x = {c.get('widget').rsplit('_', 1)[-1]: int(c.get('x', 0))
                for c in headers.findall('child')}
    for c in rows:
        row = widgets[c.get('widget').rsplit('/', 1)[-1]]
        parts = {c.get('widget').rsplit('/', 1)[-1]: c for c in row.findall('child')}
        assert {f'server_item_{field}' for field in fields} <= parts.keys()
        for field in fields:
            part = parts[f'server_item_{field}']
            # Header and value share a column, with room for every column.
            x = int(part.get('x', 0))
            assert x == header_x[field.rsplit('_', 1)[-1]]
            assert x + int(widgets[f'server_item_{field}'].get('width')) <= int(row.get('width'))
    panel = widgets['screen_stats_container']
    assert int(panel.get('top')) >= 108 + 9 * 24
    assert int(panel.get('top')) + int(panel.get('height')) <= 414
    bitmaps = {b.get('name'): b for b in root.iter('bitmap')}
    frame = bitmaps[panel.get('bitmap')].find('frame')
    # The source textures include transparent right/bottom padding. Their
    # visible panel must cover both description lines and the list width.
    assert int(frame.get('width')) * 315 // 512 >= int(panel.get('width')) - 1
    assert int(frame.get('height')) * 322 // 512 >= int(panel.get('height'))
    for c in rows:
        row = widgets[c.get('widget').rsplit('/', 1)[-1]]
        for frame in bitmaps[row.get('bitmap')].findall('frame'):
            assert int(frame.get('width')) >= int(row.get('width'))
            assert frame.get('height') == row.get('height')
    assert {'a_butn', 'b_butn', 'y_butn', 'x_butn'} <= widgets.keys()
    assert not any(c.get('widget').endswith('/join_game_button_bar')
                   for w in root.iter('widget') for c in w.findall('child'))


def test_browser_retains_map_names_at_the_c_updaters_lookup_paths():
    shared = ET.parse(MENUS / 'xbox/multiplayer.shared.xml')
    original = ET.parse(MENUS / 'ce/strings.xml')
    for name in ('main_menu/map_list', 'main_menu/mp_map_list'):
        expected = original.find(f'strings[@name="{name}"]')
        actual = shared.find(f'strings[@name="{name}"]')
        assert actual is not None
        assert [e.get('text') for e in actual] == [e.get('text') for e in expected]


def test_browser_legend_routes_shortcuts_without_broadcasting_row_actions():
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml')
    widgets = {w.get('name'): w for w in root.iter('widget')}
    screen = next(w for n, w in widgets.items() if n.endswith('/join_game_screen'))
    actions = next((widgets[c.get('widget')] for c in screen.findall('child')
                    if 'button_refresh' in c.get('widget').rsplit('/', 1)[-1]), None)
    assert actions is not None
    assert actions.find('on[@event="x"][@run="gamespy select button"]') is not None
    assert 'create_inter' in actions.get('name').rsplit('/', 1)[-1][:31]
    assert [e.get('run') for e in actions.findall('on[@event="y"]')] == [
        'join controller to mp game', 'mp type set mode']
    assert actions.find('on[@event="y"][@run="mp type set mode"][@open]') is not None
    assert actions.get('flags') == 'pass_unhandled_to_focused_child'
    assert [c.get('widget').rsplit('/', 1)[-1] for c in actions.findall('child')] == ['join_game_items_list']


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
            assert int(w.get('top', 0)) + int(w.get('height')) <= 414, w.get('name')
            desc = widgets.get(w.get('description'))
            if desc is not None and desc.get('type') == 'text':
                assert int(w.get('top', 0)) + int(w.get('height')) <= int(desc.get('top'))


def test_quit_uses_confirm_button_positions():
    root = ET.parse(MENUS / 'xbox/main_menu.quit_select.xml')
    bar = next(w for w in root.iter('widget') if w.get('name').endswith('/quit_button_bar'))
    assert [(int(c.get('x')), int(c.get('y'))) for c in bar.findall('child')] == [(268, 294), (356, 294)]


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
                    assert int(c.get('y')) + int(row.get('top', 0)) + int(row.get('height')) <= 414


def test_multiplayer_definitions_are_unique_and_browser_colors_match_xbox():
    names = [(e.tag, e.get('name')) for p in (MENUS / 'xbox').glob('*.xml')
             for e in ET.parse(p).getroot() if e.get('name')]
    assert len(names) == len(set(names))
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml')
    for w in root.iter('widget'):
        if w.get('name').rsplit('/', 1)[-1] in ('server_item_1', 'server_item_server_name'):
            assert w.get('color') == '#FF0080FF'


def window_rows(options, widgets):
    """The rows of a scrolling list that show at once: the first rows' slots
    of the column ui_widget.c lays out (visible_rows)."""
    rows = [c for c in options.findall('child') if c.get('y') and 'button_bar' not in c.get('widget')
            and '/scroll_' not in c.get('widget')]
    slots = sorted({int(c.get('y')) for c in rows})
    height = max(int(widgets[c.get('widget')].get('height')) for c in rows)
    window = int(options.get('visible_rows', len(slots)))
    pitch = slots[1] - slots[0]
    return slots, window, slots[0] + (window - 1) * pitch + height


def test_scrolling_lists_fit_above_their_help():
    seen = []
    for path in (MENUS / 'xbox').glob('*.xml'):
        widgets = {w.get('name'): w for w in ET.parse(path).getroot().iter('widget')}
        for w in widgets.values():
            if w.get('type') != 'column_list' or 'visible_rows' not in w.attrib:
                continue
            slots, window, bottom = window_rows(w, widgets)
            help_top = int(widgets[w.get('description')].get('top'))
            assert window < len(slots) and bottom <= help_top, w.get('name')
            seen.append(w.get('name'))
    assert any(name.endswith('server_settings/options_menu') for name in seen)
    assert any(name.endswith('network_setup/options_menu') for name in seen)


def test_server_settings_and_network_rows_fit_with_scrolling():
    for filename, name in (('main_menu.multiplayer_type_select.server_settings', 'server_settings'),
                           ('settings.network_setup', 'network_setup')):
        widgets = {w.get('name'): w for w in ET.parse(MENUS / 'xbox' / (filename + '.xml')).getroot().iter('widget')}
        options = next(w for w in widgets.values() if w.get('name').endswith(name + '/options_menu'))
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
            # (the server browser pages its own nine rows, lobby_browser_update, and shows the page number)
            if 'visible_rows' not in w.attrib or w.get('name').endswith('/join_game_items_list'):
                continue
            arrows = {c.get('widget').rsplit('/', 1)[-1]: c for c in w.findall('child') if '/scroll_' in c.get('widget')}
            assert set(arrows) == {'scroll_up_arrow', 'scroll_down_arrow'}, w.get('name')
            rows = [int(c.get('y')) for c in w.findall('child')
                    if c.get('y') and 'button_bar' not in c.get('widget') and '/scroll_' not in c.get('widget')]
            up, down = int(arrows['scroll_up_arrow'].get('y')), int(arrows['scroll_down_arrow'].get('y'))
            assert up == min(rows) and up < down
            window = int(w.get('visible_rows'))
            assert down <= min(rows) + window * 30
            for leaf, frame in (('scroll_up_arrow', 'arrow_sm_up'), ('scroll_down_arrow', 'arrow_sm_down')):
                art = widgets[arrows[leaf].get('widget')]
                assert art.get('bitmap') == 'xbox/bitmaps/' + frame
                assert not art.findall('on')


def test_browser_stats_keep_runtime_string_aliases():
    root = ET.parse(MENUS / 'xbox/multiplayer.shared.xml')
    name = 'main_menu/multiplayer_type_select/join_game/join_game_ticker_labels'
    labels = root.find(f'strings[@name="{name}"]')
    assert labels is not None
    assert [s.get('text') for s in labels][6:9] == ['SERVERS:', 'PLAYERS:', 'PAGE']


def test_browser_buttons_draw_map_art_with_shipped_fallbacks():
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml')
    widgets = {w.get('name').rsplit('/', 1)[-1]: w for w in root.iter('widget')}
    bitmaps = {b.get('name'): b for b in root.iter('bitmap')}
    for button in ('a', 'b', 'x', 'y'):
        icon = widgets[button + '_butn']
        assert icon.get('text') is None
        frame = bitmaps[icon.get('bitmap')].find('frame')
        assert frame.get('map') == 'ui\\shell\\bitmaps\\' + button + '_butn'
        assert (MENUS / frame.get('png')).is_file()


def test_browser_highlight_has_one_shape_for_every_row():
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml')
    rows = [w for w in root.iter('widget')
            if w.get('name').rsplit('/', 1)[-1].removeprefix('server_item_').isdigit()]
    assert len({r.get('bitmap') for r in rows}) == 1
    bitmap = root.find(f"bitmap[@name='{rows[0].get('bitmap')}']")
    frame = bitmap.findall('frame')[1]
    shape = ET.parse((MENUS / frame.get('png')).with_suffix('.svg')).getroot()[0]
    left, width = float(shape.get('x')), float(shape.get('width'))
    stroke = float(shape.get('stroke-width')) / 2
    assert left - stroke > 0
    assert left + width + stroke < int(rows[0].get('width'))
