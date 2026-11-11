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
        # (a page laid out as CONTROLLER SETUP has no DEFAULTS: A accepts)
        skipped = {'port settings defaults'} if folder in module.ACCEPT_PAGES else set()
        assert {e.get('run') for e in source.iter('on') if e.get('run')} - skipped <= {
            e.get('run') for e in output.iter('on') if e.get('run')}
        names = {w.get('name').rsplit('/', 1)[-1] for w in output.iter('widget')}
        # (the About screen's A does nothing: its legend is the way back alone)
        assert ({'b_butn', 'cancel'} if folder == 'about' else
                {'a_butn', 'b_butn', 'accept', 'cancel'}) <= names
        assert all('\\' not in w.get('name') for w in output.iter('widget'))
    hub = ET.parse(MENUS / 'xbox/settings.xml')
    assert {w.get('text') for w in hub.iter('widget') if w.find('on[@open]') is not None} == {
        'CONTROLS', 'VIDEO', 'GRAPHICS', 'AUDIO', 'NETWORK', 'KEYBOARD & MOUSE'}
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
    module = generator()
    for path in (MENUS / 'xbox').glob('settings.*.xml'):
        if path.name == 'settings.about.xml':  # (nothing to save)
            continue
        widgets = {w.get('name'): w for w in ET.parse(path).iter('widget')}
        if path.stem.split('.', 1)[-1] in module.ACCEPT_PAGES:
            # (CONTROLLER SETUP's layout: A on any row accepts, so the legend's A, posted to the focus, does too)
            assert not any(n.endswith('/button_ok') for n in widgets)
            assert next(w for n, w in widgets.items() if n.endswith('/accept')).get('text') == 'ACCEPT'
            rows = [w for n, w in widgets.items() if n.rsplit('/', 1)[-1].startswith('op_')]
            assert rows and all(w.find('on[@event="a"][@run="port settings save"][@back="true"]') is not None
                                for w in rows)
            continue
        save = next(w for n, w in widgets.items() if n.endswith('/button_ok'))
        # ui_mouse_key_button posts A to the existing focus; a SAVE item must
        # get the normal item target, which focuses it before posting A.
        assert save.get('type') is None
        caption = widgets[next(c.get('widget') for c in save.findall('child') if c.get('widget').endswith('_caption'))]
        assert caption.get('text') == 'SAVE'
        assert len(save.findall('child')) == 2
        assert save.find('on[@event="a"][@back="true"]') is not None
        accept = next(w for n, w in widgets.items() if n.endswith('/accept'))
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
                    assert copied.get('bitmap') == 'xbox/' + original.get('bitmap')
        module.validate(output.getroot(), MENUS)


def test_browser_uses_density_row_list_and_description_panel():
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml')
    widgets = {w.get('name').rsplit('/', 1)[-1]: w for w in root.iter('widget')}
    rows = [c for c in widgets['join_game_items_list'].findall('child')
            if c.get('widget').rsplit('/', 1)[-1].removeprefix('server_item_').isdigit()]
    pitch = generator().COMFORTABLE_PITCH
    count = (338 - 108) // pitch
    assert len(rows) == count
    assert int(widgets['join_game_items_list'].get('visible_rows')) == count
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
        (48, 108 + pitch * i) for i in range(count)]
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
    assert int(panel.get('top')) >= 108 + count * pitch
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
            assert frame.get('height') == '36'
            assert int(row.get('height')) == pitch
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
    assert [(int(c.get('x')), int(c.get('y'))) for c in bar.findall('child')] == [(244, 305), (260, 305), (356, 305), (372, 305)]
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
                    first = min(int(part.get('y')) for part in w.findall('child')
                                if part.get('y') and (part.get('widget').rsplit('/', 1)[-1].startswith('list_item_')
                                                    or part.get('widget').rsplit('/', 1)[-1].startswith('server_item_')))
                    if int(c.get('y')) < first + int(w.get('visible_rows', '64')) * generator().COMFORTABLE_PITCH:
                        assert int(c.get('y')) + int(row.get('top', 0)) + int(row.get('height')) <= 414


def test_multiplayer_definitions_are_unique_and_browser_colors_match_xbox():
    names = [(e.tag, e.get('name')) for p in (MENUS / 'xbox').glob('*.xml') if '.compact.' not in p.name
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
    # (the places the list's rows take, which the leading rows' own y values may skip past)
    slots = list(range(slots[0], slots[-1] + 1, slots[1] - slots[0]))
    height = max(int(widgets[c.get('widget')].get('height')) for c in rows)
    window = int(options.get('visible_rows', len(slots)))
    pitch = slots[1] - slots[0]
    return slots, window, slots[0] + (window - 1) * pitch + height


def shown_children(options):
    """The most rows of a list one platform shows: the Mac's VIDEO and every other platform's GRAPHICS share a slot."""
    children = options.findall('child')
    return max(len([c for c in children if c.get('platform') != hidden]) for hidden in ('mac_app', '!mac_app'))


def test_scrolling_lists_fit_above_their_help():
    seen = []
    for path in (MENUS / 'xbox').glob('*.xml'):
        compact = '.compact.' in path.name
        files = [p for p in (MENUS / 'xbox').glob('*.xml')
                 if ('.compact.' in p.name if compact else '.compact.' not in p.name)
                 or (compact and '.compact.' not in p.name and not p.with_suffix('.compact.xml').exists())]
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


def test_server_settings_rows_fit_without_scrolling_and_network_rows_scroll():
    # (Server Setup's seven rows fit above its help)
    widgets = {w.get('name'): w for w in ET.parse(
        MENUS / 'xbox/main_menu.multiplayer_type_select.server_settings.xml').getroot().iter('widget')}
    options = next(w for w in widgets.values() if w.get('name').endswith('server_settings/options_menu'))
    slots, window, bottom = window_rows(options, widgets)
    assert 'visible_rows' not in options.attrib
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
    items = [('x_butn', 'join_game_button_refresh'), ('y_butn', 'create'),
             ('b_butn', 'cancel'), ('a_butn', 'accept')]
    gaps, previous_end = [], None
    for icon, caption in items:
        assert positions[caption] - positions[icon] == 21
        if previous_end is not None:
            gaps.append(positions[icon] - previous_end)
        previous_end = positions[caption] + int(widgets[caption].get('width'))
    assert min(gaps) > 0 and len(set(gaps)) == 1


def test_browser_highlight_has_one_shape_for_every_row():
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.join_game.xml')
    names = {c.get('widget') for w in root.iter('widget')
             if w.get('name').endswith('/join_game_items_list') for c in w.findall('child')}
    rows = [w for w in root.iter('widget') if w.get('name') in names
            and w.get('name').rsplit('/', 1)[-1].removeprefix('server_item_').isdigit()]
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
    """Description drawings ship as 4x RGBA PNGs; Xbox rows and hub slots share their sources."""
    manifest = json.loads((MENUS / 'menus.json').read_text())['files']
    for name in ('audio', 'video', 'internet', 'about', 'mouse'):
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
    assert pngs[1:5] == ['xbox/art/video.png', 'xbox/art/none.png', 'xbox/art/audio.png', 'xbox/art/internet.png']
    assert all(frame.get('cache') is None for frame in frames[1:5])
    # Both pictures use the player's map; the importer redraws the recognized keyboard.
    assert pngs[0] == 'xbox/art/none.png' and frames[0].get('cache')
    assert pngs[5] == 'xbox/art/none.png' and frames[5].get('cache') == 'cache/xbox/v1/keyboard.png'
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
    # (GRAPHICS' panel names its mode there, in the profile name's place, while it is chosen)
    assert all(s.get('text').endswith(('Current Profile:', 'Graphics:')) for s in strings)


def test_picture_slot_resolves_redraw_then_cache_then_nothing(tmp_path):
    # no redraw for the others: the player's cache over the transparent PNG (menu_files.c tries cache first)
    _, frames = hub_panel(tmp_path / 'a', redraws=('audio',))
    by_key = dict(zip(('controls', 'video', 'graphics', 'audio', 'network', 'keyboard'), frames))
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
        assert widgets[f'{base}/{icon}_butn'].get('bitmap') == f'xbox/browser/{icon}_butn_sm'
        label = widgets[f'{base}/{icon}_caption']
        assert label.get('text') == caption
        assert label.find('on[@event="left_mouse"]').get('run') == f'mouse emit {emitted} event'
    assert (r'pc\xbox\error\confirm_join_big_modal_fullscreen',
            r'pc\error\confirm_join_big_modal_fullscreen') in generator().screen_pairs()


def test_keyboard_picture_is_generated_at_import_without_a_shipped_png():
    assert not list(MENUS.rglob('keyboard.png'))
    assert 'xbox/art/keyboard.png' not in json.loads((MENUS / 'menus.json').read_text())['files']
    assert not (ROOT / 'tools/menu_keyboard_redraw.py').exists()
    redraw = (ROOT / 'port/ios/host/menu_picture_redraw.m').read_text()
    assert '@"keyboard"' not in redraw
    assert 'keyboard.png' not in (ROOT / 'port/ios/CMakeLists.txt').read_text()


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
    assert action.get('name') == base + ('/op_start_game' if page == 'server_settings' else '/op_save')
    assert action.get('text') == ('START GAME' if page == 'server_settings' else 'SAVE')
    if page == 'coop_options':
        old = widgets[base + '/button_ok']
        assert [e.attrib for e in action.findall('on')] == [e.attrib for e in old.findall('on')]
    else:
        assert action.find('on[@event="a"]').get('run') == 'ss start game'
    assert action.get('bitmap') == widgets[base + ('/op_coop_options' if page == 'server_settings'
                                                  else '/op_fireteam')].get('bitmap')
    for c in rows:
        row = widgets[c.get('widget')]
        assert int(row.get('height')) == module.ROW_HEIGHT == 28
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
                assert int(inner.get('y')) + int(value.get('top')) + int(value.get('text_y')) == int(label.get('text_y'))
                if value.get('type') == 'spinner':
                    assert value.get('header_bounds') == '7 -6 19 0'
                    assert value.get('footer_bounds') == f'7 {width + 3} 19 {width + 9}'
    accept = widgets[base + '/' + page + '_screen/legend/accept']
    assert accept.get('text') == 'SELECT'


def test_map_screen_uses_only_player_counts_and_keeps_campaign_descriptions():
    base = 'xbox/main_menu/multiplayer_type_select/mp_map_select'
    root = ET.parse(MENUS / 'xbox/main_menu.multiplayer_type_select.mp_map_select.xml')
    data = root.find(f'widget[@name="{base}/mp_map_right_data"]')
    counts = root.find(f'strings[@name="{data.get("string_list")}"]')
    assert counts is not None
    source = ET.parse(MENUS / 'ce/strings.xml').find(
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
    pc_campaign = ET.parse(MENUS / 'ce/strings.xml').find(
        'strings[@name="main_menu/solo_level_select/map_data"]')
    assert [s.get('text') for s in campaign] == [s.get('text') for s in pc_campaign]


@pytest.mark.parametrize('look', ('ce', 'xbox'))
def test_server_setup_rows_invite_and_gametype_editor(look):
    base = ('xbox/' if look == 'xbox' else '') + 'main_menu/multiplayer_type_select/server_settings'
    root = ET.parse(MENUS / look / 'main_menu.multiplayer_type_select.server_settings.xml')
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
    if look == 'ce':
        assert (MENUS / look / 'main_menu.multiplayer_type_select.server_settings.xml').read_text() == generated_text
    generated = ET.fromstring(generated_text)
    for name in ('labels', 'help_strings'):
        expected = generated.find(f'strings[@name="main_menu/multiplayer_type_select/server_settings/{name}"]')
        assert [e.attrib for e in root.find(f'strings[@name="{base}/{name}"]')] == [e.attrib for e in expected]


def test_merged_rows_and_bindings():
    import port_settings
    BASE = 'main_menu/multiplayer_type_select/coop_options/'
    NAME = 'main_menu.multiplayer_type_select.coop_options.xml'
    for look in ('ce', 'xbox'):
        prefix = 'xbox/' if look == 'xbox' else ''
        base = prefix + BASE
        root = ET.parse(ROOT / 'port/assets/menus' / look / NAME).getroot()
        widgets = {w.get('name'): w for w in root.findall('widget')}
        rows = widgets[base + 'options_menu'].findall('child')
        assert [c.get('widget').removeprefix(base) for c in rows] == [
            'op_friendly_fire', 'op_extra_enemies', 'op_player_collisions', 'op_fireteam',
            'op_save' if look == 'xbox' else 'button_bar']
        assert [int(c.get('y')) for c in rows[:4]] == [73, 103, 133, 163]
        assert all(widgets[base + name].get('height') == '28' for name in (
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
        assert widgets[base + 'coop_options_screen'].find("on[@event='created']").get('run') == 'coop options init'
        action = widgets[base + ('op_save' if look == 'xbox' else 'button_ok')]
        for event in ('a', 'start'):
            assert action.find(f"on[@event='{event}']").get('run') == 'coop options save'
        if look == 'xbox':
            assert action.get('text') == 'SAVE'
            assert int(rows[-1].get('y')) == 193
        assert all(not any(w.get('name').endswith('/' + name) for w in widgets.values())
                   for name in ('op_ally_friendly_fire', 'op_enemies_per_player', 'op_enemies_multiplier'))


def test_settings_legends_have_global_button_bindings():
    module = generator()
    for path in (MENUS / 'xbox').glob('settings.*.xml'):
        if path.name == 'settings.about.xml':
            continue
        if path.stem.split('.', 1)[-1] in module.ACCEPT_PAGES:  # (A accepts on any row: no DEFAULTS or SAVE)
            continue
        root = ET.parse(path).getroot()
        widgets = {w.get('name'): w for w in root.iter('widget')}
        screen = next(w for name, w in widgets.items() if name.endswith('_screen'))
        for button, target in (('x', 'button_defaults'), ('y', 'button_ok')):
            action = next(w for name, w in widgets.items() if name.endswith('/' + target))
            handler = action.find(f'on[@event="{button}"]')
            assert handler is not None, (path, button)
            global_handler = screen.find(f'on[@event="{button}"]')
            assert global_handler is not None
            assert global_handler.get('run') == handler.get('run')
            assert global_handler.get('back') == handler.get('back')
            assert any(name.endswith('/' + button + '_butn') for name in widgets)
            for event in ('a', 'start'):
                assert action.find(f'on[@event="{event}"]').get('run') == handler.get('run')


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
    assert count > 40


def test_settings_footer_focus_only_visits_actions():
    module = generator()
    for path in (MENUS / 'xbox').glob('settings.*.xml'):
        if path.name == 'settings.about.xml':
            continue
        if path.stem.split('.', 1)[-1] in module.ACCEPT_PAGES:  # (A accepts on any row: no DEFAULTS or SAVE)
            continue
        widgets = {w.get('name'): w for w in ET.parse(path).iter('widget')}
        bar = next(w for name, w in widgets.items() if name.endswith('/button_bar'))
        assert [c.get('widget').rsplit('/', 1)[-1] for c in bar.findall('child')] == ['button_defaults', 'button_ok']
        for placement in bar.findall('child'):
            action = widgets[placement.get('widget')]
            assert action.find('on[@event="a"]') is not None
            assert len(action.findall('child')) == 2


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
                    ('network', None), ('keyboard', 'keyboard')]
    # the Mac keeps VIDEO, which has the mode as its first row: GRAPHICS shares its slot elsewhere
    by_name = {c.get('widget').rsplit('/', 1)[-1]: c for c in options.findall('child')}
    assert by_name['graphics'].get('y') == by_name['video'].get('y') == by_name['audio'].get('y')
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


def test_graphics_screen_is_controller_setups_layout_with_the_modes_help():
    widgets = settings_widgets(MENUS / 'xbox/settings.graphics.xml')
    base = 'xbox/settings/graphics'
    options = widgets[f'{base}/options_menu']
    assert [c.get('widget').rsplit('/', 1)[-1] for c in options.findall('child')] == ['op_graphics', 'op_crt_effect']
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
        'The picture as a CRT television showed it: scanlines, phosphor glow and soft color.')
    # A accepts on any row, B goes back (ACCEPT_PAGES): no SAVE or DEFAULTS
    assert widgets[f'{base}/graphics_screen'].find('on[@event="b"]').get('back') == 'true'
    assert f'{base}/button_ok' not in widgets and f'{base}/button_defaults' not in widgets


def test_video_and_the_pc_looks_video_settings_have_the_mode_first_and_the_crt_row_beneath_it():
    for path, base in ((MENUS / 'xbox/settings.video_settings.xml', 'xbox/settings/video_settings'),
                       (MENUS / 'ce/main_menu.settings_select.player_setup.player_profile_edit.video_settings.xml',
                        'main_menu/settings_select/player_setup/player_profile_edit/video_settings')):
        widgets = settings_widgets(path)
        rows = [c.get('widget').rsplit('/', 1)[-1] for c in widgets[f'{base}/options_menu'].findall('child')]
        assert rows[:3] == ['op_graphics', 'op_crt_effect', 'op_mode'], path
        assert widgets[f'{base}/graphics_spinner'].get('values') == 'hardcore|classic|enhanced'
        # a page's row is packed from the top, so the CRT effect's closing up when it is hidden leaves no gap
        assert widgets[f'{base}/options_menu'].get('row_pitch'), path


@pytest.mark.parametrize('look', ['ce', 'xbox'])
@pytest.mark.parametrize('page', ['video_settings', 'graphics'])
def test_hires_rows_are_absent_and_remaining_rows_keep_their_layout_and_strings(look, page):
    import port_settings

    base = f'xbox/settings/{page}' if look == 'xbox' else f'{port_settings.PE}/{page}'
    filename = f'settings.{page}.xml' if look == 'xbox' else base.replace('/', '.') + '.xml'
    root = ET.parse(MENUS / look / filename).getroot()
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
    assert len(children) == len(rows)
    slots = sorted({int(c.get('y')) for c in children})
    pitch = int(options.get('row_pitch'))
    assert slots == list(range(int(options.get('row_top')), slots[-1] + 1, pitch))
    visible = min(len(slots), int(options.get('visible_rows')))
    assert slots[visible - 1] + 28 <= int(widgets[f'{base}/help'].get('top'))
    assert int(widgets[f'{base}/help'].get('top')) + int(widgets[f'{base}/help'].get('height')) <= 414
    for index, (_, setting, *_rest) in enumerate(rows):
        key = setting.split('.', 1)[1]
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
                       (MENUS / 'ce/main_menu.settings_select.player_setup.player_profile_edit.video_settings.xml',
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


def test_the_pc_looks_graphics_screen_is_the_xbox_looks_source_and_pairs_with_it():
    pairs = (ROOT / 'port/linux/include/menu_equivalence.h').read_text()
    assert r'{"pc\\xbox\\settings\\graphics\\graphics_screen", ' in pairs
    manifest = json.loads((MENUS / 'menus.json').read_text())['files']
    for name in ('xbox/settings.graphics.xml', 'ce/main_menu.settings_select.player_setup.player_profile_edit.graphics.xml',
                 'ce/port/main_menu/settings_select/player_setup/player_profile_edit/graphics/header_graphics.png'):
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
                         ('confirm_delete_profile_modal_fullscreen',))
def test_original_dialogs_route_on_every_platform(name):
    assert (f'ui/shell/error/{name}', f'pc/xbox/error/{name}') in generator().SCREEN_ALWAYS


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
    screen = widgets['xbox/' + generator().MP + '/join_game/gamespy_error_fullscreen']
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
    root = ET.parse(Path(__file__).parents[1] / 'port/assets/menus/xbox/settings.video_settings.xml').getroot()
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
    root = ET.parse(Path(__file__).parents[1] / 'port/assets/menus/xbox/settings.video_settings.xml').getroot()
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
            assert width in standard, (name, spinner.get('name'), width)
            assert width / 2 >= module.SPINNER_MINIMUM_HALF_WIDTH, (name, spinner.get('name'), width)
            assert spinner.get('footer_bounds') == f'7 {width + 3} 19 {width + 9}'
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
