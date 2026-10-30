"""Shipped PC description panels never display the old PLACE HOLDER cards."""
from pathlib import Path
import sys
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'
sys.path.insert(0, str(ROOT / 'tools'))
import port_settings


FAMILIES = {
    'main_menu/multiplayer_type_select/mp_options': (0, 1, 2),
    'main_menu/settings_select/multiplayer_setup/playlist_edit/gametype_options': (0, 2, 3, 4, 5, 6, 7),
    'main_menu/settings_select/player_setup/player_profile_edit/profile_options': (1, 3, 4, 5, 6, 8),
}


def test_pc_picture_gaps_resolve_to_real_art():
    bitmaps = {b.get('name'): b for b in ET.parse(MENUS / 'ce/bitmaps.xml').getroot()}
    for name, indices in FAMILIES.items():
        for index in indices:
            frame = bitmaps[name][index]
            assert frame.get('map') or frame.get('png', '').startswith('xbox/art/'), (name, index)
            if frame.get('map'):
                assert frame.get('cache') and frame.get('png') == 'xbox/art/none.png'
            else:
                assert (MENUS / frame.get('png')).is_file()


def test_generation_preserves_pc_picture_choices():
    bitmaps = {b.get('name'): b for b in ET.parse(MENUS / 'ce/bitmaps.xml').getroot()}
    for name, indices in FAMILIES.items():
        for index in indices:
            assert port_settings.picture_frame('shell/' + name, index) == bitmaps[name][index].attrib


def test_no_shipped_frame_references_placeholder_cards():
    placeholders = {f'ce/shell/{name}__{index}.png' for name, indices in FAMILIES.items() for index in indices}
    for path in MENUS.rglob('*.xml'):
        for frame in ET.parse(path).iter('frame'):
            assert frame.get('png') not in placeholders, (path, frame.attrib)


def test_description_art_is_embedded():
    import json
    files = set(json.loads((MENUS / 'menus.json').read_text())['files'])
    for bitmap in ET.parse(MENUS / 'ce/bitmaps.xml').getroot():
        for frame in bitmap:
            if frame.get('png'):
                assert frame.get('png') in files


def test_pc_direct_link_uses_internet_drawing():
    name = 'main_menu/multiplayer_type_select/mp_options'
    frame = port_settings.picture_frame('shell/' + name, 2)
    assert frame == {'width': '512', 'height': '256', 'png': 'xbox/art/internet.png'}


def test_pc_mouse_setup_uses_mouse_drawing():
    name = 'main_menu/settings_select/player_setup/player_profile_edit/profile_options'
    frame = port_settings.picture_frame('shell/' + name, 3)
    assert frame == {'width': '512', 'height': '256', 'png': 'xbox/art/mouse.png'}


def test_pc_gametype_frames_follow_focused_rows():
    family = 'main_menu/settings_select/multiplayer_setup/playlist_edit'
    names = ('name', None, 'player', 'item', 'vehicle', 'indicator', 'teamplay', 'save')
    frames = ET.parse(MENUS / 'ce/bitmaps.xml').find(f"bitmap[@name='{family}/gametype_options']")
    for index, name in enumerate(names):
        assert 'map' not in frames[index].attrib and 'cache' not in frames[index].attrib
        assert (frames[index].get('width'), frames[index].get('height')) == ('256', '128')
        assert frames[index].attrib == port_settings.picture_frame('shell/' + family + '/gametype_options', index)
        if name:
            assert frames[index].get('png') == f'xbox/art/gametype-{name}.png'
        else:
            assert frames[index].get('png') == f'ce/shell/{family}/gametype_options__1.png'
    rows = ET.parse(MENUS / ('ce/' + family.replace('/', '.') + '.xml'))
    picture = rows.find(f"widget[@name='{family}/playlist_edit_ext_desc_pic']")
    assert (picture.get('left'), picture.get('top'), picture.get('width'), picture.get('height')) == ('304', '114', '256', '128')
    assert port_settings.WIDGET_PATCHES[family + '/playlist_edit_ext_desc_pic']['set'] == {'left': 304, 'top': 114, 'width': 256, 'height': 128}
    order = rows.find(f"widget[@name='{family}/edit_playlist_select_list']")
    assert [child.get('widget').removeprefix(family + '/playlist_edit_') for child in order.findall('child')] == [
        'name_list_item', 'game_type_item', 'player_opts_item', 'items_list_item',
        'vehicles_list_item', 'indicators_item', 'team_item', 'button_bar',
    ]


def test_generation_rebuilds_existing_game_options_redraw(tmp_path):
    # Load the actual generator method without its optional raster libraries.
    import ast
    from types import SimpleNamespace
    tree = ast.parse((ROOT / 'tools/ce_menus.py').read_text())
    art_class = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == 'Art')
    method = next(node for node in art_class.body if isinstance(node, ast.FunctionDef) and node.name == 'bitmap')
    namespace = {'port_settings': port_settings, 'MENUS': tmp_path,
                 'our_name': lambda tag: tag.replace('\\', '/').removeprefix('ui/shell/'),
                 'attributes': lambda items: ''.join(f' {key}="{value}"' for key, value in items)}
    exec(compile(ast.Module(body=[method], type_ignores=[]), 'ce_menus.bitmap', 'exec'), namespace)
    calls = []
    art = SimpleNamespace(tags=SimpleNamespace(load=lambda *args: {
        'bitmap data': [{'width': 512, 'height': 256}] * 8, 'bitmap group sequence': []}),
        pngs=[], shown={}, draw=lambda *args: calls.append(args))
    family = 'main_menu/settings_select/multiplayer_setup/playlist_edit/gametype_options'
    generated = ET.fromstring(namespace['bitmap'](art, 'ui\\shell\\' + family.replace('/', '\\')))
    assert [call[1] for call in calls] == [1]
    assert calls[0][5] == tmp_path / f'ce/shell/{family}__1.png'
    assert [frame.attrib for frame in generated] == [
        port_settings.picture_frame('shell/' + family, index) for index in range(8)]
