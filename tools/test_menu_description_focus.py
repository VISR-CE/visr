"""The MULTIPLAYER screen's description panel never shows a missing string."""
from pathlib import Path
from xml.etree import ElementTree as ET
import sys

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus/ce'
sys.path.insert(0, str(ROOT / 'tools'))
import port_settings

MT = 'main_menu/multiplayer_type_select'


def test_every_selectable_row_has_a_description():
    widgets = {}
    for name in ('main_menu.multiplayer_type_select.xml', 'main_menu.multiplayer_type_select.coop.xml'):
        for w in ET.parse(MENUS / name).getroot().iter('widget'):
            widgets[w.get('name')] = w
    rows = [widgets[c.get('widget')] for c in
            widgets[f'{MT}/multiplayer_type_select_list'].findall('child')
            if c.get('widget') in widgets]
    selectable = [w for w in rows if w.get('type') != 'column_list' and w.find('on') is not None]
    strings = port_settings.STRING_OVERRIDES[f'{MT}/multiplayer_option_descriptions']
    assert len(selectable) == len(strings)


def test_nested_button_bar_focus_has_no_description_index():
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    start = source.index('short ui_widget_port_list_index(')
    body = source[start:source.index('\n}\n', start)]
    assert 'focused_child->type == _ui_widget_type_column_list' in body
    assert body.index('return NONE') < body.index('for (child = list_widget->child')
