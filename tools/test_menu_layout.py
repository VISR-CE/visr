"""Layout rules that hold for every generated Xbox-look screen.

Each test walks all of `port/assets/menus/xbox/*.xml` and checks one rule of the standard elements for every
widget it applies to. A screen is covered because it is generated, not because a test names it; the few
exceptions are listed here with their reasons, and each list should only shrink.
"""
from collections import defaultdict
from pathlib import Path
import re
import sys
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
XBOX = ROOT / 'port/assets/menus/xbox'
sys.path.insert(0, str(ROOT / 'tools'))

import menu_geometry  # noqa: E402
import xbox_menus  # noqa: E402

MP = 'xbox/main_menu/multiplayer_type_select'
# Lists whose rows keep the disc's 28-unit height inside the standard pitch, with the reason.
SHORT_ROW_LISTS = {
    f'{MP}/join_game/password/options_menu': 'entry form: its help box sits right under the rows and must move first',
    f'{MP}/join_game/code/options_menu': 'entry form: its help box sits right under the rows and must move first',
    f'{MP}/direct_ip/direct_ip_menu': 'entry form: its help box sits right under the rows and must move first',
}
# The server browser's lists keep the approved mockup's own geometry (a list beside a detail panel, rows of the
# mockup's pitch), and the legacy Direct Link table keeps the disc's rows; each is exempt from the standard pitch.
BROWSER_LISTS = {
    f'{MP}/join_game/join_game_items_list': 'server browser: the approved mockup C list, 46-unit pitch (rows 43 tall)',
    f'{MP}/join_game_legacy/join_game_items_list': 'legacy Direct Link table: the disc table at its own pitch',
}
# Screens still laid out by their own code and skipped by `layout_geometry`.
LEGACY_SCREENS = {
    f'{MP}/direct_ip/direct_ip_screen', f'{MP}/join_game/filters/filters_screen',
    f'{MP}/join_game_legacy/join_game_screen',
    *(f'xbox/main_menu/settings_select/multiplayer_setup/playlist_edit/{kind}_edit/{kind}_options_screen'
      for kind in ('ctf', 'koth', 'oddball', 'race', 'slayer')),
}
ROW_ART = re.compile(r'(option_bkds(_sm)?|list_item_bkd(_top)?|row)$')
BUTTON = re.compile(r'(a|b|x|y|left_thumb)_butn$')


def load():
    widgets, owners = {}, {}
    for path in sorted(XBOX.glob('*.xml')):
        for widget in ET.parse(path).getroot().iter('widget'):
            widgets[widget.get('name')] = widget
            owners[widget.get('name')] = path.name
    return widgets, owners


WIDGETS, OWNERS = load()


def leaf(name):
    return name.rsplit('/', 1)[-1]


def is_row(placement):
    """A list child drawn with row art: an option row, a list item or a server row."""
    widget = WIDGETS.get(placement.get('widget'))
    return (widget is not None and placement.get('y') is not None
            and ROW_ART.search(widget.get('bitmap', '')) is not None
            and 'button_bar' not in placement.get('widget') and '/scroll_' not in placement.get('widget'))


def row_lists():
    for name, widget in WIDGETS.items():
        if widget.get('type') != 'column_list' or 'left_right_tabs_items' in widget.get('flags', ''):
            continue
        rows = [c for c in widget.findall('child') if is_row(c)]
        if rows:
            yield name, widget, rows


def screens():
    return {name: widget for name, widget in WIDGETS.items() if name in xbox_menus.SCREEN_LAYOUTS}


def test_the_xml_has_the_lists_and_screens_these_rules_are_about():
    assert len(list(row_lists())) >= 25
    assert len(screens()) >= 40


def test_every_row_list_is_on_the_standard_pitch():
    off = {}
    for name, _, rows in row_lists():
        if name in BROWSER_LISTS:
            continue
        ys = sorted({int(c.get('y')) for c in rows})
        pitches = {b - a for a, b in zip(ys, ys[1:])}
        if pitches - {menu_geometry.COMFORTABLE_PITCH}:
            off[name] = sorted(pitches)
    assert off == {}


def test_every_row_is_the_standard_tap_band_tall():
    short = defaultdict(set)
    for name, _, rows in row_lists():
        if name in BROWSER_LISTS:
            continue
        for c in rows:
            height = int(WIDGETS[c.get('widget')].get('height', 480))
            if height != menu_geometry.COMFORTABLE_PITCH:
                short[name].add(height)
    assert set(short) == set(SHORT_ROW_LISTS), {name: sorted(heights) for name, heights in short.items()}
    assert all(heights == {menu_geometry.ROW_HEIGHT} for heights in short.values())


def test_rows_stay_on_screen_and_above_the_bottom_rule():
    """A list's shown rows (all of them, or its `visible_rows` window from the first) end by the bottom rule.
    (Rows that share a place are shown one at a time by platform or by code, so overlap is checked where the
    screens run: the sweep's lint.)"""
    problems = []
    for name, options, rows in row_lists():
        for c in rows:
            widget = WIDGETS[c.get('widget')]
            x = int(c.get('x', 0))
            if x < 0 or x + int(widget.get('width', 640)) > 640:
                problems.append((name, c.get('widget'), 'off screen sideways'))
        ys = sorted({int(c.get('y')) for c in rows})
        shown = min(len(ys), int(options.get('visible_rows', len(ys))))
        bottom = ys[0] + shown * menu_geometry.COMFORTABLE_PITCH
        if name in BROWSER_LISTS:
            bottom = ys[0] + shown * (ys[1] - ys[0]) if len(ys) > 1 else bottom
        if ys[0] < 0 or bottom > xbox_menus.RULE_Y:
            problems.append((name, 'rows end at', bottom))
    assert problems == []


def test_every_screen_names_a_layout_kind_and_the_legacy_ones_are_known():
    generated = {name for name in WIDGETS if leaf(name).endswith(('_screen', 'fullscreen'))}
    assert generated - set(xbox_menus.SCREEN_LAYOUTS) == set()
    legacy = {name for name, spec in xbox_menus.SCREEN_LAYOUTS.items() if spec.get('legacy')}
    assert legacy == LEGACY_SCREENS
    assert {spec['kind'] for spec in xbox_menus.SCREEN_LAYOUTS.values()} <= set(xbox_menus.LAYOUT_KINDS)


def test_side_panels_run_down_to_the_bottom_rule():
    panels = {name: widget for name, widget in WIDGETS.items() if leaf(widget.get('bitmap', '')) == 'list_field'
              and widget.get('top') is not None}
    assert len(panels) >= 4
    short = {name: int(widget.get('top')) + int(widget.get('height')) for name, widget in panels.items()
             if int(widget.get('top')) + int(widget.get('height')) != xbox_menus.RULE_Y}
    assert short == {}


def test_bottom_bar_buttons_sit_on_the_legend_line():
    off = []
    for name, screen in screens().items():
        spec = xbox_menus.SCREEN_LAYOUTS[name]
        if spec['kind'] == 'K8' or spec.get('legacy'):
            continue  # dialogs place their two buttons inside the panel
        for c in screen.findall('child'):
            if BUTTON.search(leaf(c.get('widget'))) and int(c.get('y', 0)) != menu_geometry.LEGEND_Y:
                off.append((name, c.get('widget'), c.get('y')))
    assert off == []


def test_no_screen_draws_a_button_bar_under_its_legend():
    both = []
    for name, screen in screens().items():
        if xbox_menus.SCREEN_LAYOUTS[name].get('legacy'):
            continue
        children = [c.get('widget') for c in screen.findall('child')]
        legend = any('/legend/' in child for child in children)
        bars = [child for child in children if 'button_bar' in leaf(child) and WIDGETS[child].findall('child')]
        if legend and bars:
            both.append((name, bars))
    assert both == []


def test_a_box_is_as_large_as_the_drawing_of_ours_it_shows():
    """A widget cuts its picture to its own size. A box that shows one of our own drawings whole (a single frame
    from xbox/art with no placement of its own) must be at least the drawing's size, or it loses an edge: the
    browser's status box was 3 units shorter than its drawing and drew no bottom border."""
    bitmaps = {}
    for path in sorted(XBOX.glob('*.xml')):
        for bitmap in ET.parse(path).getroot().iter('bitmap'):
            bitmaps[bitmap.get('name')] = bitmap
    checked, cut = 0, []
    for name, widget in WIDGETS.items():
        bitmap = bitmaps.get(widget.get('bitmap'))
        frames = bitmap.findall('frame') if bitmap is not None else []
        # (xbox/art/none.png is the transparent picture: nothing of it can be cut)
        if len(frames) != 1 or not frames[0].get('png', '').startswith('xbox/art/') or frames[0].get('map') \
                or frames[0].get('png') == xbox_menus.NONE_PNG \
                or frames[0].get('x') or frames[0].get('y') or widget.get('height') is None:
            continue
        checked += 1
        if int(widget.get('height')) < int(frames[0].get('height')) or \
                int(widget.get('width', 640)) < int(frames[0].get('width')):
            cut.append((name, widget.get('width'), widget.get('height'), frames[0].get('width'), frames[0].get('height')))
    assert checked >= 2 and cut == [], cut
