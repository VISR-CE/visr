"""Shipped PC description panels never display the old PLACE HOLDER cards."""
from pathlib import Path
import sys
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'
sys.path.insert(0, str(ROOT / 'tools'))
import port_settings


FAMILIES = {
    'main_menu/multiplayer_type_select/mp_options': (0, 1),
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
