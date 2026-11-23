"""Every bar caption fits its touch slot.

Bug class: a caption wider than its slot. On touch the engine draws each bar action as a pill in its button's
standard slot (`ui_touch_legend_slot_for_button`; a fifth button takes B's empty slot), and a caption wider than the
slot widens its pill toward the neighbor slots. Declared bars are checked by
`test_declared_bar_captions_fit_their_slots`; this test covers the captions of the placed legend glyphs too (dialogs,
the lobby, the browser, FILTERS and the action-bar screens): each glyph's caption is the text drawn just right of it,
and every caption it can show, in its own font, with the pill's minimum padding, fits the slot.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

import menu_geometry
import menu_text_metrics
import pytest
from test_xbox_menus import layout_widgets, screen_parts

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'
# ui_widget.c: UI_TOUCH_LEGEND_MINIMUM_PADDING, each side of the caption
MINIMUM_PADDING = 12
SLOT = {'a': 'a', 'b': 'b', 'x': 'x', 'y': 'y'}


def string_lists():
    lists = {}
    for path in (MENUS / 'xbox').glob('*.xml'):
        for strings in ET.parse(path).getroot().iter('strings'):
            lists[strings.get('name')] = [s.get('text') for s in strings.findall('string')]
    return lists


def captions(text, lists):
    if text.get('text'):
        return [text.get('text')]
    if text.get('string_list') in lists:
        shown = lists[text.get('string_list')]
        index = text.get('string_index')
        return [shown[int(index)]] if index is not None else shown
    return []


def placed_captions():
    """(glyph name, button, font, caption) for every caption beside a placed legend glyph"""
    widgets = layout_widgets('comfortable')
    lists = string_lists()
    children = {c.get('widget') for w in widgets.values() for c in w.findall('child')}
    found, seen = [], set()
    for root in (w for name, w in widgets.items() if name not in children):
        parts = list(screen_parts(root, widgets, screen_names=set()))
        texts = [(w, x, y) for w, x, y in parts if w.get('type') == 'text']
        for glyph, x, y in parts:
            leaf = glyph.get('name').rsplit('/', 1)[-1]
            if not leaf.endswith('_butn') or glyph.get('name') in seen:
                continue
            seen.add(glyph.get('name'))
            near = [(abs(tx - (x + menu_geometry.BAR_CAPTION_OFFSET)) + abs(ty - y), t) for t, tx, ty in texts]
            near = [item for item in near if item[0] < 17]
            if not near:
                continue
            text = min(near, key=lambda item: item[0])[1]
            for caption in captions(text, lists):
                found.append((glyph.get('name'), leaf.removesuffix('_butn'), text.get('font'), caption))
    return found


@pytest.mark.skipif(not menu_text_metrics.available(), reason='the disc font metrics are not available')
def test_every_placed_caption_fits_its_touch_slot():
    found = placed_captions()
    assert len({name for name, *_ in found}) >= 60
    for name, button, font, caption in found:
        x0, x1 = menu_geometry.BAR_SLOT_BOUNDS[SLOT.get(button, 'b')]
        width = menu_text_metrics.text_width(font, caption)
        assert width + 2 * MINIMUM_PADDING <= x1 - x0, (name, caption, width, x1 - x0)


def test_the_padding_is_the_engines():
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    assert f'#define UI_TOUCH_LEGEND_MINIMUM_PADDING {MINIMUM_PADDING}' in source.replace('\t', ' ')
