"""Screen redirects end, and no screen opens its way back to itself through one.

`menu_equivalence.h` (written by tools/xbox_menus.py) names screens that open in place of others. A redirect
whose target can open the redirected screen again would loop: the settings hub did, through its PROFILES row,
until the game learned to skip the redirect while the hub is up. These checks read the generator's tables and the
generated XML's `open=` edges.
"""
from pathlib import Path
import sys
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

import ui_map_tags  # noqa: E402
import xbox_menus  # noqa: E402

XBOX = ROOT / 'port/assets/menus/xbox'
# Redirected screens that their own target opens, which the game must not redirect while the target is up
# (`menu_hub_open` in port/linux/game/menu_tags.c). Each entry is a loop the game breaks by hand.
OPENED_BY_THEIR_TARGET = {
    ('ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_select_screen',
     'pc\\xbox\\settings\\settings_screen'),
}


# Screens the engine asks for by name that this ui.map does not hold (`display_error` in ui_widget.c picks a
# half- or quarter-screen error for split screen): the redirect answers the name.
ASKED_FOR_BY_NAME = {f'ui\\shell\\error\\error_{mode}_{size}screen'
                     for mode in ('modal', 'nonmodal') for size in ('half', 'qtr')}


def tag(name):
    return name.replace('/', '\\')


def redirects():
    table = {tag(source): tag(target) for source, target in xbox_menus.SCREEN_ALWAYS}
    table.update({tag(source): tag(target) for source, target in xbox_menus.SCREEN_REPLACED})
    return table


def opens():
    """screen tag -> the screens its widgets open (generated screens, by their `open=` and `replace=`)."""
    widgets = {}
    for path in sorted(XBOX.glob('*.xml')):
        for widget in ET.parse(path).getroot().iter('widget'):
            widgets[widget.get('name')] = widget

    def reach(name, seen):
        if name in seen or name not in widgets:
            return set()
        seen.add(name)
        found = set()
        for element in widgets[name].iter():
            for key in ('open', 'replace', 'otherwise'):
                target = element.get(key)
                if target:
                    found.add(target if '\\' in target else 'pc\\' + tag(target))
            if element.tag == 'child':
                found |= reach(element.get('widget'), seen)
        return found

    return {'pc\\' + tag(name): reach(name, set()) for name in xbox_menus.SCREEN_LAYOUTS if name in widgets}


def test_a_redirect_never_leads_to_another_redirect():
    table = redirects()
    chained = {source: target for source, target in table.items() if target in table}
    assert chained == {}


def test_every_redirect_names_a_disc_screen_and_a_generated_one():
    layout = ui_map_tags.load_layout()
    generated = set(opens())
    for source, target in redirects().items():
        assert source in layout or source in ASKED_FOR_BY_NAME, source
        assert target in generated, target


def test_no_target_opens_the_screen_it_replaces_except_where_the_game_breaks_the_loop():
    edges = opens()
    loops = {(source, target) for source, target in redirects().items() if source in edges.get(target, ())}
    assert loops == OPENED_BY_THEIR_TARGET


