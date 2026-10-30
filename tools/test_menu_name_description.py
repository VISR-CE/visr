"""CHANGE NAME keeps the shared gametype label line and panel bounds."""
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
FAMILY = 'main_menu/settings_select/multiplayer_setup/playlist_edit'


def test_gametype_name_description_layout():
    widgets = {w.get('name'): w for w in ET.parse(ROOT / f'port/assets/menus/ce/{FAMILY.replace("/", ".")}.xml').getroot()}
    panel = widgets[f'{FAMILY}/playlist_edit_ext_desc_text']
    name = widgets[f'{FAMILY}/gametype_edit_screen'].find(f"child[@widget='{FAMILY}/gametype_label']")
    assert (panel.get('left'), panel.get('top'), panel.get('width'), panel.get('height')) == ('294', '276', '321', '124')
    assert (name.get('x'), name.get('y')) == ('294', '360')
    strings = ET.parse(ROOT / 'port/assets/menus/ce/strings.xml').find(f"strings[@name='{panel.get('string_list')}']")
    label_line = strings[1].get('text').split(r'\n').index('This gametype:')
    assert label_line == 3
    for entry in strings:
        lines = entry.get('text').split(r'\n')
        assert lines[label_line] == 'This gametype:'
        assert len(lines) == label_line + 1
