"""Xbox settings generation preserves wiring and needs no extracted assets."""
import importlib.util
import json
from pathlib import Path
import shutil
import sys
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
            name = 'xbox/' + original.get('name')
            # Quit's original text panel sits inside a new confirmation modal.
            if name == 'xbox/main_menu/quit_select/quit_screen':
                name = 'xbox/main_menu/quit_select/quit_panel'
            copied = widgets[name]
            for tag, key in (('on', 'run'), ('data', 'input')):
                assert [(e.get('event'), e.get(key)) for e in original.findall(tag)] == [
                    (e.get('event'), e.get(key)) for e in copied.findall(tag)]
            if '/header_' in original.get('name'):
                # (their size is their picture's: tools/test_xbox_titles.py)
                for key in ('left', 'top'):
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
    first_row = widgets[rows[0].get('widget').rsplit('/', 1)[-1]]
    parts = {c.get('widget').rsplit('/', 1)[-1]: c for c in first_row.findall('child')}
    left_margin = int(parts['server_item_server_name'].get('x'))
    right_edge = (int(parts['server_item_ping'].get('x')) +
                  int(widgets['server_item_ping'].get('width')))
    assert int(first_row.get('width')) - right_edge == left_margin
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
    assert [(int(c.get('x')), int(c.get('y'))) for c in bar.findall('child')] == [(268, 310), (284, 310), (356, 310), (372, 310)]
    children = bar.findall('child')
    assert children[0].get('widget') == 'xbox/error/confirm_delete_profile/b_butn'
    assert children[2].get('widget') == 'xbox/error/confirm_delete_profile/a_butn'
    modal = next(w for w in root.iter('widget') if w.get('name').endswith('/quit_screen'))
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
            # (the hub's description is a panel beside the rows, not help text under them)
            if not w.get('description') or w.get('name') == 'xbox/settings/settings_list':
                assert len(w.findall('child')) <= window and bottom <= 414, w.get('name')
                continue
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
            # A short list can use the viewport only to pack platform-filtered rows.
            if len(w.findall('child')) <= int(w.get('visible_rows')):
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
        assert (frame.get('width'), frame.get('height')) == ('32', '32')
    screen = widgets['join_game_screen']
    positions = {c.get('widget').rsplit('/', 1)[-1]: int(c.get('x', 0))
                 for c in screen.findall('child')}
    items = [('x_butn', 'join_game_button_refresh'), ('y_butn', '=create'),
             ('b_butn', '=cancel'), ('a_butn', '=accept')]
    gaps, previous_end = [], None
    for icon, caption in items:
        assert positions[caption] - positions[icon] == 21
        if previous_end is not None:
            gaps.append(positions[icon] - previous_end)
        previous_end = positions[caption] + int(widgets[caption].get('width'))
    assert min(gaps) > 0 and len(set(gaps)) == 1


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


def test_main_menus_never_open_quit_but_keep_the_dialog():
    for folder in ('ce', 'xbox'):
        for path in (MENUS / folder).glob('*.xml'):
            root = ET.parse(path)
            assert not [e.attrib for e in root.iter() if 'quit_select' in e.get('open', '')], path
            assert not [e.attrib for e in root.iter() if e.get('platform') == 'quit'], path
            if path.name != 'main_menu.quit_select.xml':
                assert not [e.attrib for e in root.iter('on') if e.get('run') in
                            ('main menu quit game', 'port quit game')], path
        dialog = ET.parse(MENUS / folder / 'main_menu.quit_select.xml')
        assert any(w.get('name', '').endswith('/quit_screen') for w in dialog.iter('widget'))
    main = ET.parse(MENUS / 'ce/main_menu.xml')
    rows = main.find("widget[@name='main_menu/main_menu_select_list']").findall('child')
    assert len(rows) == 4
    assert [int(c.get('y')) for c in rows] == [247, 283, 319, 355]
    assert not any('quit' in w.get('name', '') for w in main.iter('widget'))


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
    """Only AUDIO, VIDEO and INTERNET ship as our own art: 4x PNGs of the 512 by 256 drawings, which the profile
    editor's rows, the multiplayer type screen's INTERNET and DIRECT LINK rows and the hub's slots name."""
    manifest = json.loads((MENUS / 'menus.json').read_text())['files']
    for name in ('audio', 'video', 'internet'):
        svg = (MENUS / f'xbox/art/{name}.svg').read_text()
        assert 'width="512" height="256"' in svg
        png = (MENUS / f'xbox/art/{name}.png').read_bytes()
        assert png[:8] == b'\x89PNG\r\n\x1a\n' and png[24:26] == bytes((8, 6)), '8-bit RGBA'
        assert (int.from_bytes(png[16:20], 'big'), int.from_bytes(png[20:24], 'big')) == (2048, 1024)
        assert f'xbox/art/{name}.png' in manifest
        assert f'xbox/art/{name}.png' in generator().retain_xbox_files(MENUS, [])
    patches = ET.parse(MENUS / 'xbox/patches.xml')
    wired = {row.get('name'): row.get('picture') for row in patches.iter('row') if row.get('picture')}
    assert wired['profile_item_video'] == 'png:xbox/art/video.png'
    assert wired['profile_item_audio'] == 'png:xbox/art/audio.png'
    assert wired['profile_item_network'] == 'png:xbox/art/internet.png'
    assert wired['multiplayer_type_internet'] == wired['multiplayer_type_join_direct'] == 'png:xbox/art/internet.png'
    hub = ET.parse(MENUS / 'xbox/settings.xml').getroot()
    frames = hub.find("bitmap[@name='xbox/settings/hub_pictures']")
    pngs = [frame.get('png') for frame in frames]
    assert pngs[1:4] == ['xbox/art/video.png', 'xbox/art/audio.png', 'xbox/art/internet.png']
    assert all(frame.get('cache') is None for frame in frames[1:4])
    # the controllers and keyboard stay the player's own, and nothing else borrows the Xbox's video or network pictures
    assert pngs[0] == 'xbox/art/none.png' and frames[0].get('cache') and pngs[4] == 'xbox/art/none.png'
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
    assert (panel.get('left'), panel.get('top'), panel.get('width'), panel.get('height')) == ('275', '78', '320', '362')
    assert panel.get('bitmap') == 'xbox/bitmaps/list_field'
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
    assert all(s.get('text').endswith('Current Profile:') for s in strings)


def test_picture_slot_resolves_redraw_then_cache_then_nothing(tmp_path):
    # no redraw for the others: the player's cache over the transparent PNG (menu_files.c tries cache first)
    _, frames = hub_panel(tmp_path / 'a', redraws=('audio',))
    by_key = dict(zip(('controls', 'video', 'audio', 'network', 'keyboard'), frames))
    assert by_key['audio'].attrib == {'png': 'xbox/art/audio.png', 'width': '512', 'height': '256'}
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
