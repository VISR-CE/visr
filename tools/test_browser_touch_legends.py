"""The server browser's touch buttons follow the global-vs-contextual rule: its actions are a declared bar (the
engine draws global ones on touch and leaves BACK to the chevron), and JOIN GAME in the detail panel, the main
contextual action, is not drawn on touch because tapping the item does it."""
import re
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'port/linux/game/menu_functions.c'
MENUS = ROOT / 'port/assets/menus/xbox'
BROWSER_XML = 'main_menu.multiplayer_type_select.join_game.xml'
FILTERS_XML = 'main_menu.multiplayer_type_select.join_game.filters.xml'


def function_body(name):
    text = SOURCE.read_text()
    match = re.search(r'\n(?:static )?[\w ]+[ *]' + name + r'\([^)]*\)\n\{', text)
    assert match, name
    depth, index = 0, match.end() - 1
    for index in range(match.end() - 1, len(text)):
        if text[index] == '{':
            depth += 1
        elif text[index] == '}':
            depth -= 1
            if depth == 0:
                break
    return text[match.end():index]


def bar_actions(xml):
    root = ET.parse(MENUS / xml).getroot()
    bars = list(root.iter('bar'))
    assert len(bars) == 1, xml
    return {a.get('slot'): (a.get('scope'), a.get('text'), a.get('run')) for a in bars[0].findall('action')}


def test_browser_actions_are_a_declared_bar():
    assert bar_actions(BROWSER_XML) == {
        'x': ('global', 'IP ADDRESS', 'port browser ip open'),
        'y': ('global', 'CREATE', 'port browser create'),
        'b': ('back', 'BACK', 'gamespy back handler'),
        'a': ('select', 'JOIN GAME', None)}


def test_filters_actions_are_a_declared_bar():
    actions = bar_actions(FILTERS_XML)
    assert actions['x'] == ('global', 'RESET', 'port browser filters reset')
    assert actions['b'][:2] == ('back', 'BACK')


def test_browser_has_no_placed_legend_widgets():
    for xml in (BROWSER_XML, FILTERS_XML):
        text = (MENUS / xml).read_text()
        for name in ('browser_ip_address', 'browser_create_inter', 'browser_back', 'browser_reset', 'browser_done'):
            assert f'/{name}' not in text, (xml, name)


def test_join_game_is_the_bars_select_action_and_never_changes_its_caption():
    root = ET.parse(MENUS / BROWSER_XML).getroot()
    action = next(a for a in root.iter('action') if a.get('slot') == 'a')
    assert action.get('scope') == 'select' and action.get('focus') == 'true'
    # (connect progress shows on the joining game's row and one panel line, never on the legend)
    assert action.get('text') == 'JOIN GAME' and action.get('strings') is None
    # no placed copy of it, and no touch code of its own: the bar leaves a select action undrawn on touch
    text = (MENUS / BROWSER_XML).read_text()
    assert 'xbox/main_menu/multiplayer_type_select/join_game/join_game_button_join' not in text and 'browser_join_' not in text.replace('browser_join_reason', '')
    assert 'browser_touch_join_update' not in SOURCE.read_text()


def test_a_global_x_action_is_never_the_items_secondary_action():
    """Bug class: right-click is the secondary action only (menu-global-contextual-actions), but the browser's global X
    (IP ADDRESS) was treated as the focused game's: its legend lit the right mouse button and a right-click or hold
    offered it as an item action. The engine names a screen-wide X in one place and every X consumer asks it."""
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    assert 'static boolean ui_widget_is_global_x_action(' in source
    scan = source[source.index('static void ui_touch_screen_x_scan('):]
    scan = scan[:scan.index('\n}\n')]
    assert 'ui_screen_has_global_x_action(' in scan
    assert source.count('!ui_widget_is_global_x_action(widget)') == 2, 'both draw-time X notes skip a global X'
    assert 'halo_controller_glyph_x_global(ui_widget_is_global_x_action(widget))' in source
