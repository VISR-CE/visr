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
    'main_menu/settings_select/multiplayer_setup/playlist_edit/gametype_options': (0, 3, 4, 6, 7),
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


def test_pc_vehicle_options_uses_warthog_without_changing_items():
    family = 'main_menu/settings_select/multiplayer_setup/playlist_edit'
    widgets = ET.parse(MENUS / ('ce/' + family.replace('/', '.') + '.xml'))
    row = family + '/playlist_edit_vehicles_list_item'
    assert widgets.find(f"widget[@name='{row}']").get('string_index') == '6'
    children = widgets.find(f"widget[@name='{family}/edit_playlist_select_list']").findall('child')
    index = [child.get('widget') for child in children].index(row)
    assert index == 4
    assert port_settings.picture_frame('shell/' + family + '/gametype_options', index) == {
        'width': '512', 'height': '256', 'png': 'xbox/art/warthog.png'}
    items = port_settings.picture_frame('shell/' + family + '/gametype_options', 3)
    assert items['map'] == 'ui\\shell\\' + (family + '/gametype_options').replace('/', '\\')
    assert items['index'] == '3'
    assert items['png'] == 'xbox/art/none.png'
