#!/usr/bin/env python3
"""The Xbox-look menus' shared geometry, in 640x480 menu units: the one place these numbers live.

    python3 tools/menu_geometry.py           # rewrite port/linux/include/menu_geometry.h
    python3 tools/menu_geometry.py --check   # fail when the header is stale

`tools/xbox_menus.py` builds the screens from these values; the engine reads the same values from
the generated `menu_geometry.h` (`menu_tags.c` packs and fits rows with them at load time, `ui_widget.c`
draws a row's dropdown chevron with them).
"""
from __future__ import annotations

import math
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
# The title's rule. The title picture is 64 units high at TITLE_TOP; its capitals stand 22 units tall from 33 units
# down (ui_widget.c menu_title_icon), so they end at TITLE_CAPS_BOTTOM. The disc's screens (measured on the Direct IP
# screen) leave TITLE_RULE_GAP units between the capitals and the rule under them. Every generated backdrop draws its
# rule at TITLE_RULE_Y, and a screen's content starts TITLE_RULE_Y - TITLE_RULE_LEGACY below where it did when the
# rule sat on the capitals (the server browser's and its filters'), BROWSER_CONTENT_SHIFT.
TITLE_TOP = 11
TITLE_CAPS_BOTTOM = TITLE_TOP + 33 + 22
TITLE_RULE_GAP = 5
TITLE_RULE_Y = TITLE_CAPS_BOTTOM + TITLE_RULE_GAP
TITLE_RULE_LEGACY = 66  # where the rule was drawn before, touching the capitals
BROWSER_CONTENT_SHIFT = TITLE_RULE_Y - TITLE_RULE_LEGACY
BROWSER_ROW_PITCH, BROWSER_ROW_HEIGHT = 46, 43
# The server browser's rows are tabs, as the gametype editor's: the tab (BROWSER_TAB_WIDTH wide) is outlined all around
# and the chosen game's reaches BROWSER_TAB_REACH, over the detail panel's left border, in the panel's own fill, so
# tab and panel are one shape. The panel is drawn first so the tab lies over it.
BROWSER_LIST_X, BROWSER_PANEL_LEFT = 24, 304
BROWSER_BORDER = 1.5  # the joined outline's stroke
# The tab and the panel are ONE shape: browser_assets.joined_outline_path draws the panel with the chosen game's tab
# merged into its left side, as a single outlined, singly filled path (the "joint" art, one per visible row, plus one
# for no chosen game). The tab's own widget draws nothing while chosen, so no outline or fill is drawn twice and the
# translucent fill cannot double up where the two meet.
BROWSER_ROWS = 6
BROWSER_TAB_WIDTH, BROWSER_TAB_REACH = 270, 282
BROWSER_PANEL_WIDTH, BROWSER_PANEL_RADIUS, BROWSER_TAB_RADIUS = 312, 9, 7
BROWSER_PANEL_TOP = 110 + BROWSER_CONTENT_SHIFT
BROWSER_JOINT_WIDTH = BROWSER_PANEL_LEFT - BROWSER_LIST_X + BROWSER_PANEL_WIDTH  # from the tab's left to the panel's right
# The panel's fill is the menu's own translucent blue (the backdrop's opacity), so the animated scene shows through
# the tab and the panel alike.
BROWSER_PANEL_FILL = '#0d2c57'
BROWSER_PANEL_FILL_OPACITY = 0.69
BROWSER_PANEL_BORDER = '#4aa8ff'  # the panel's outline and the chosen tab's rules: one line where they meet
assert BROWSER_PANEL_TOP + (BROWSER_ROWS - 1) * BROWSER_ROW_PITCH + BROWSER_ROW_HEIGHT == BROWSER_PANEL_TOP + 273, \
    'the first and last rows reach the panel top and bottom: the joint merges their edges'
assert BROWSER_LIST_X + BROWSER_TAB_WIDTH < BROWSER_PANEL_LEFT < BROWSER_LIST_X + BROWSER_TAB_REACH
# The detail panel's preview, one set of numbers for the art, the widgets, the text and the pictures the engine loads.
# The map's picture is the photo-mode frame, 2:1 and whole (its native shape: no crop), inset in the panel; the
# server name and the gametype line sit over a darkened band (the scrim) along its foot, the gametype's silhouette
# over its top right; the rules and the host follow it, then the room for the explainer and the password pill.
# The picture starts inside the panel's outline: a whole unit past its stroke (BROWSER_BORDER, 1.5), never over it.
BROWSER_PICTURE_INSET = math.ceil(BROWSER_BORDER)
BROWSER_PREVIEW_X, BROWSER_PREVIEW_Y = 313, BROWSER_PANEL_TOP + BROWSER_PICTURE_INSET
assert BROWSER_PREVIEW_Y >= BROWSER_PANEL_TOP + BROWSER_BORDER, 'the picture clears the panel outline'
BROWSER_PREVIEW_WIDTH, BROWSER_PREVIEW_HEIGHT = 294, 147
BROWSER_PICTURE_TEXTURE = (512, 256)  # the picture's menu copy (map_pictures_render.c writes a 512x256 BMP)
BROWSER_SCRIM_HEIGHT = 44
BROWSER_SCRIM_TOP = BROWSER_PREVIEW_Y + BROWSER_PREVIEW_HEIGHT - BROWSER_SCRIM_HEIGHT
BROWSER_NAME_Y, BROWSER_NAME_HEIGHT = BROWSER_SCRIM_TOP, 25
BROWSER_TYPE_Y, BROWSER_TYPE_HEIGHT = BROWSER_NAME_Y + 24, 20
BROWSER_BADGE_WIDTH, BROWSER_BADGE_HEIGHT, BROWSER_BADGE_INSET = 62, 50, 6
BROWSER_DETAIL_LINE = 20  # the pitch of the rows below the picture
BROWSER_RULES_Y = BROWSER_PREVIEW_Y + BROWSER_PREVIEW_HEIGHT + 4
BROWSER_HOST_Y = BROWSER_RULES_Y + BROWSER_DETAIL_LINE
BROWSER_STATUS_Y = BROWSER_HOST_Y + BROWSER_DETAIL_LINE
BROWSER_NOTICE_Y, BROWSER_NOTICE_HEIGHT = BROWSER_STATUS_Y + 20, 38  # the explainer (two lines) or the big-game notice
BROWSER_PILL_Y = BROWSER_NOTICE_Y + BROWSER_NOTICE_HEIGHT + 2
BROWSER_DETAIL_HEIGHT = 273
assert BROWSER_PREVIEW_WIDTH * BROWSER_PICTURE_TEXTURE[1] == BROWSER_PREVIEW_HEIGHT * BROWSER_PICTURE_TEXTURE[0], \
    'the preview shows the picture whole, at its own shape'
assert BROWSER_NAME_Y + BROWSER_NAME_HEIGHT <= BROWSER_TYPE_Y + 1 and BROWSER_TYPE_Y + BROWSER_TYPE_HEIGHT \
    <= BROWSER_PREVIEW_Y + BROWSER_PREVIEW_HEIGHT, 'the name and the gametype line sit inside the scrim'
assert BROWSER_PILL_Y + 18 <= BROWSER_PANEL_TOP + BROWSER_DETAIL_HEIGHT, 'the pill stays inside the panel'
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
    ('MENU_TITLE_RULE_Y', TITLE_RULE_Y, "the rule under a screen's title"),
    ('MENU_BAR_RULE_Y', RULE_Y, "the bar's top rule"),
    ('MENU_BROWSER_ROWS', BROWSER_ROWS, "the server browser's visible rows, one joined panel outline for each"),
    ('MENU_BROWSER_PICTURE_WIDTH', BROWSER_PICTURE_TEXTURE[0], "the server browser's map picture, whole (2:1)"),
    ('MENU_BROWSER_PICTURE_HEIGHT', BROWSER_PICTURE_TEXTURE[1], "the server browser's map picture, whole (2:1)"),
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
