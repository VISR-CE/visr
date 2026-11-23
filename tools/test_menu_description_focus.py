"""The MULTIPLAYER screen's description panel never shows a missing string."""
from pathlib import Path
from xml.etree import ElementTree as ET
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

MT = 'main_menu/multiplayer_type_select'


def test_nested_button_bar_focus_has_no_description_index():
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    start = source.index('short ui_widget_port_list_index(')
    body = source[start:source.index('\n}\n', start)]
    assert 'focused_child->type == _ui_widget_type_column_list' in body
    assert body.index('return NONE') < body.index('pc_menu_patch_list_index')
