#!/usr/bin/env python3
"""The Xbox-look menus' shared geometry, in 640x480 menu units: the one place these numbers live.

    python3 tools/menu_geometry.py           # rewrite port/linux/include/menu_geometry.h
    python3 tools/menu_geometry.py --check   # fail when the header is stale

`tools/xbox_menus.py` builds the screens from these values; the engine reads the same values from
the generated `menu_geometry.h` (`menu_tags.c` packs and fits rows with them at load time, `ui_widget.c`
draws a row's dropdown chevron with them).
"""
from __future__ import annotations

import argparse
from pathlib import Path
import sys

import menu_text_metrics

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'port/linux/include/menu_geometry.h'

# Option rows: the first row's y, the disc's own row height, and the standard row: a 39-unit tap band with 36
# units of row art centered in it.
FIRST_ROW = 73
ROW_HEIGHT = 28
COMFORTABLE_PITCH = 39
COMFORTABLE_ROW_ART = 36
# How far a comfortable row's label and value move down inside the taller row: the game's own option_bkds art
# drawn as the 36-unit band (`xbox/bitmaps/row_36/option_bkds`) has its text line 5 units lower than the 28-unit row.
COMFORTABLE_ROW_SHIFT = 5
# Picture-list rows (the disc's list_item_bkd art, 32 units, beside a picture panel): the standard row draws the art
# 36 units tall in the same band, so the row's text moves down by half the difference.
PICTURE_ROW_HEIGHT = 32
PICTURE_ROW_SHIFT = (COMFORTABLE_ROW_ART - PICTURE_ROW_HEIGHT) // 2
# The bottom bar: legend y (glyphs and captions) and the y below which nothing of a list is drawn.
LEGEND_Y = 414
# the bottom bar's top rule (its last row); the legend's glyphs and captions hang below it
RULE_Y = 409
# A value column: the measured widest value plus this margin on each side, at least the minimum width. The margin
# keeps a centered value clear of the touch dropdown's chevron, drawn CHEVRON_INSET in from the column's right
# edge and CHEVRON_WIDTH wide.
CHEVRON_INSET = 8
CHEVRON_WIDTH = 12
VALUE_MARGIN = CHEVRON_INSET + CHEVRON_WIDTH + 4
VALUE_MINIMUM_WIDTH = 80
# Pointer dropdown option rows (at least 44 pt on the iPhone 17 Pro in landscape at the minimum menu scale).
DROPDOWN_PITCH = 52

# The button bar: four fixed slots, left to right X, Y, B and A, right-aligned. Each slot is as wide as the widest
# caption it ever holds, in the bar's one caption font, plus padding on each side; a touch pill fills its slot, and
# a controller glyph and its caption sit BAR_INSET inside it. The row first slides right into the screen's margin
# and then trims padding, so the four slots fit the screen.
BAR_FONT = 'ui\\large_ui'
BAR_SMALL_FONT = 'ui\\small_ui'   # a caption too wide for its slot in BAR_FONT is drawn in this (bar_caption_font)
BAR_SLOTS = 'xyba'
BAR_WIDEST = {'a': 'START GAME', 'b': 'CANCEL', 'y': 'CREATE GAME', 'x': 'IP ADDRESS'}
BAR_RIGHT = 592
BAR_SCREEN_RIGHT = 628
BAR_LEFT_MARGIN = 12
BAR_GAP = 12
BAR_PADDING = 24
BAR_INSET = 4
BAR_GLYPH_WIDTH = 21
BAR_GLYPH_HEIGHT = 26
BAR_CAPTION_OFFSET = 22
BAR_CAPTION_HEIGHT = 22
# A glyph's box is taller than its caption's, so centered in its box the glyph sits below the capitals of
# BAR_FONT's text; measured once against that font's cap height in the caption box. It raises the glyph's box
# and the clip it draws under together (controller_glyph_legend_raise).
BAR_GLYPH_RAISE = 3


def bar_slots():
    """Each slot's (x0, x1), from the A slot's right edge leftward."""
    widths = {slot: menu_text_metrics.text_width(BAR_FONT, text) for slot, text in BAR_WIDEST.items()}
    right, padding = BAR_RIGHT, BAR_PADDING
    overflow = sum(widths.values()) + 2 * padding * 4 + BAR_GAP * 3 - BAR_RIGHT + BAR_LEFT_MARGIN
    if overflow > 0:
        shift = min(overflow, BAR_SCREEN_RIGHT - BAR_RIGHT)
        right += shift
        overflow -= shift
        if overflow > 0:
            padding = max(8, padding - (overflow + 2 * 4 - 1) // (2 * 4))
    slots = {}
    for slot in reversed(BAR_SLOTS):
        slots[slot] = (right - widths[slot] - 2 * padding, right)
        right = slots[slot][0] - BAR_GAP
    return slots


BAR_SLOT_BOUNDS = bar_slots()

# Row lists with a geometry of their own, each exempt from the standard pitch and row height by exact name, with its
# (pitch, height) and the reason: the tests check these lists have exactly this geometry, so an exemption cannot
# hide a drift. The server browser's rows are the approved mockup's (a list beside a detail panel); the legacy Direct
# Link table keeps the disc's rows.
_MP = 'xbox/main_menu/multiplayer_type_select'
BROWSER_ROW_PITCH, BROWSER_ROW_HEIGHT = 46, 43
# The server browser's rows are tabs, as the gametype editor's: the tab (BROWSER_TAB_WIDTH wide) is outlined all around
# and the chosen game's reaches BROWSER_TAB_REACH, over the detail panel's left border, in the panel's own fill, so
# tab and panel are one shape. The panel is drawn first so the tab lies over it.
BROWSER_LIST_X, BROWSER_PANEL_LEFT = 24, 304
BROWSER_TAB_WIDTH, BROWSER_TAB_REACH = 270, 284
BROWSER_PANEL_FILL = '#0d2c57'
assert BROWSER_LIST_X + BROWSER_TAB_WIDTH < BROWSER_PANEL_LEFT < BROWSER_LIST_X + BROWSER_TAB_REACH
OWN_GEOMETRY_ROW_LISTS = {
    f'{_MP}/join_game/join_game_items_list': (BROWSER_ROW_PITCH, BROWSER_ROW_HEIGHT,
                                              'server browser: the approved mockup C list'),
    f'{_MP}/join_game_legacy/join_game_items_list': (24, 24, 'legacy Direct Link table: the disc table'),
}


def bar_caption_room(slot):
    x0, x1 = BAR_SLOT_BOUNDS[slot]
    return x1 - x0 - 2 * BAR_INSET - BAR_CAPTION_OFFSET


def bar_caption_font(slot, captions):
    """'large', or 'small' when a caption (any a run-time one can show) does not fit the slot in the large font; the
    one rule for every bar caption. A caption that fits in neither is an error."""
    for font, name in ((BAR_FONT, 'large'), (BAR_SMALL_FONT, 'small')):
        if all(menu_text_metrics.text_width(font, caption) <= bar_caption_room(slot) for caption in captions):
            return name
    raise ValueError(f'{captions} do not fit the {slot} slot in either font')

HEADER_VALUES = (
    ('MENU_FIRST_ROW', FIRST_ROW, "the first option row's y"),
    ('MENU_ROW_HEIGHT', ROW_HEIGHT, "the disc's option row height"),
    ('MENU_COMFORTABLE_PITCH', COMFORTABLE_PITCH, 'a comfortable option row: its tap band and pitch'),
    ('MENU_COMFORTABLE_ROW_ART', COMFORTABLE_ROW_ART, "a comfortable option row's drawn art"),
    ('MENU_COMFORTABLE_ROW_SHIFT', COMFORTABLE_ROW_SHIFT, "a comfortable row's label and value, moved down"),
    ('MENU_PICTURE_ROW_SHIFT', PICTURE_ROW_SHIFT, "a picture-list row's text, moved down in the taller row"),
    ('MENU_LEGEND_Y', LEGEND_Y, "the bottom bar's glyphs and captions"),
    ('MENU_CHEVRON_INSET', CHEVRON_INSET, "the dropdown chevron, in from the value column's right edge"),
    ('MENU_CHEVRON_WIDTH', CHEVRON_WIDTH, 'the dropdown chevron'),
    ('MENU_VALUE_MARGIN', VALUE_MARGIN, 'beside the widest value in a value column'),
    ('MENU_VALUE_MINIMUM_WIDTH', VALUE_MINIMUM_WIDTH, 'a value column at its narrowest'),
    ('MENU_DROPDOWN_PITCH', DROPDOWN_PITCH, 'a pointer dropdown option row'),
    *((f'MENU_BAR_SLOT_{slot.upper()}_X{edge}', BAR_SLOT_BOUNDS[slot][edge], f"the bar's {slot.upper()} slot: its "
       + ('left' if edge == 0 else 'right') + ' edge') for slot in BAR_SLOTS for edge in (0, 1)),
    ('MENU_BAR_GAP', BAR_GAP, 'between two slots of the bar'),
    ('MENU_BAR_INSET', BAR_INSET, "a controller glyph and its caption, in from their slot's edges"),
    ('MENU_BAR_GLYPH_WIDTH', BAR_GLYPH_WIDTH, "a bar action's glyph"),
    ('MENU_BAR_GLYPH_HEIGHT', BAR_GLYPH_HEIGHT, "a bar action's glyph"),
    ('MENU_BAR_CAPTION_OFFSET', BAR_CAPTION_OFFSET, "a bar action's caption, right of its glyph's left edge"),
    ('MENU_BAR_CAPTION_HEIGHT', BAR_CAPTION_HEIGHT, "a bar action's caption"),
    ('MENU_BAR_GLYPH_RAISE', BAR_GLYPH_RAISE, "a bar action's glyph, up from the center of its box onto its caption's capitals"),
    ('MENU_BAR_RULE_Y', RULE_Y, "the bar's top rule"),
)


def header():
    width = max(len(name) for name, _, _ in HEADER_VALUES)
    lines = ['/* Generated by tools/menu_geometry.py; edit that file. The Xbox-look menus\' shared geometry,',
             '   in 640x480 menu units. */',
             '#ifndef MENU_GEOMETRY_H', '#define MENU_GEOMETRY_H', '']
    lines += [f'#define {name.ljust(width)} {value} /* {comment} */' for name, value, comment in HEADER_VALUES]
    lines += ['', '#endif', '']
    return '\n'.join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--check', action='store_true', help='fail when the header is stale')
    args = parser.parse_args(argv)
    if args.check:
        if not HEADER.exists() or HEADER.read_text(encoding='utf-8') != header():
            print(f'{HEADER.relative_to(ROOT)} is stale; run tools/menu_geometry.py', file=sys.stderr)
            return 1
        return 0
    HEADER.write_text(header(), encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main())
