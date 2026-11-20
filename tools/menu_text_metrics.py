#!/usr/bin/env python3
"""The menu fonts' glyph advances, and text layout as the game draws it (stdlib only).

    python3 tools/menu_text_metrics.py --update /path/to/ui.map   # rewrite tools/menu_font_metrics.json
    python3 tools/menu_text_metrics.py --check /path/to/ui.map    # fail when the table is stale

The menus draw with the map's own fonts (`ui\\large_ui`, `ui\\small_ui`), which the generator cannot open,
so `menu_font_metrics.json` keeps each font's line metrics and each character's advance and bitmap width
(`font_character.character_width` and `.bitmap_width`). The functions below follow
`source/text/draw_string.c`: a line's cursor starts `leading_width` in from the box's left edge, a
character is drawn only while `bitmap_width + cursor + line_width < x1`, and a box that wraps
(`_draw_text_wrap_horizontally_bit`) breaks the line at its last space instead of dropping the rest.
`text_width` is `ui_widget_port_text_width`: the farther of the cursor and the last glyph's right edge.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
# The table is checked in: numbers only (line metrics and per-character widths), no glyph art. --update rewrites
# it from a ui.map and --check compares it with one; VISR_MENU_FONT_METRICS names another file to use.
TABLE = ROOT / 'tools/menu_font_metrics.json'


def metrics_path():
    named = os.environ.get('VISR_MENU_FONT_METRICS')
    return Path(named) if named else TABLE


def available():
    return metrics_path().exists()
FONTS = ('ui\\large_ui', 'ui\\small_ui')
# a text box wraps when its height holds two lines (widget_instance_render_text_box)
WRAP_LINES = 2


def read_fonts(map_path):
    """The menu fonts' metrics from an Xbox ui.map (tools/ui_map_tags.py reads the tag cache)."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from ui_map_tags import MapReader

    reader = MapReader(Path(map_path).read_bytes())
    fonts = {}
    for tag in reader.tags.values():
        if tag['group'] != 'font' or tag['path'] not in FONTS:
            continue
        # struct font_header (source/text/font_group.h): flags, heights, leading width, 36 bytes of
        # padding, character tables, four style references, then the characters (struct font_character)
        header = reader.offset(tag['base'], 0x9C)
        _, ascending, descending, leading_height, leading_width = reader.unpack('<Ihhhh', header)
        characters = {}
        for offset in reader.block(header + 0x7C, 0x14):
            code, width, bitmap_width = reader.unpack('<Hhh', offset)
            characters[str(code)] = [width, bitmap_width]
        fonts[tag['path']] = {'ascending_height': ascending, 'descending_height': descending,
                              'leading_height': leading_height, 'leading_width': leading_width,
                              'characters': dict(sorted(characters.items(), key=lambda item: int(item[0])))}
    missing = set(FONTS) - set(fonts)
    if missing:
        raise ValueError(f'the map has no {sorted(missing)}')
    return {name: fonts[name] for name in FONTS}


_fonts = None


def fonts():
    global _fonts
    if _fonts is None:
        _fonts = json.loads(metrics_path().read_text(encoding='utf-8'))
    return _fonts


def font(name):
    if name in ('large', None):
        name = 'ui\\large_ui'
    elif name == 'small':
        name = 'ui\\small_ui'
    return fonts()[name]


def line_height(font_name):
    f = font(font_name)
    return f['ascending_height'] + f['descending_height'] + f['leading_height']


def _character(f, character):
    # a character the font lacks is skipped, as font_get_character_by_ascii_code returns none
    return f['characters'].get(str(ord(character)))


def text_width(font_name, text):
    """ui_widget_port_text_width: the widest line's cursor or ink right edge, from the left edge."""
    f = font(font_name)
    widest = 0
    for line in text.replace('\r\n', '\n').replace('\r', '\n').split('\n'):
        cursor, ink = f['leading_width'], 0
        for character in line:
            glyph = _character(f, character)
            if glyph:
                ink = max(ink, cursor + glyph[1])
                cursor += glyph[0]
        widest = max(widest, cursor, ink)
    return widest


def layout(font_name, text, width, wrap):
    """The lines draw_string draws in a box `width` wide (from the text's left edge, that is after
    the box's text_x), and whether every character was drawn. Without wrapping a line stops at the
    first character that does not fit and the rest of that line is lost."""
    f = font(font_name)
    lines, complete = [], True
    for paragraph in text.replace('\r\n', '\n').replace('\r', '\n').split('\n'):
        rest = paragraph
        while True:
            used, drawn, last_break = 0, 0, None
            for index, character in enumerate(rest):
                glyph = _character(f, character)
                if character == ' ' and index:
                    last_break = index
                if glyph is None:
                    drawn = index + 1
                    continue
                if glyph[1] + f['leading_width'] + used < width:
                    used += glyph[0]
                    drawn = index + 1
                else:
                    break
            if drawn >= len(rest):
                lines.append(rest)
                break
            if wrap and last_break:
                lines.append(rest[:last_break])
                rest = rest[last_break:].lstrip(' ')
                continue
            lines.append(rest[:drawn])
            complete = False
            break
    return lines, complete


def wraps(font_name, height, text_y=0):
    """Whether a text box this tall wraps its lines (it holds two)."""
    return height - text_y >= WRAP_LINES * line_height(font_name)


def box_fits(font_name, text, width, height, text_x=0, text_y=0):
    """Whether a text box draws all of `text`: every character inside its width (wrapping when the
    box holds two lines) and every line's baseline inside its height (draw_string draws a line while
    its cursor is above the box's bottom)."""
    wrap = wraps(font_name, height, text_y)
    lines, complete = layout(font_name, text, width - text_x, wrap)
    f = font(font_name)
    last_baseline = (len(lines) - 1) * line_height(font_name) + f['ascending_height']
    return complete and last_baseline < height - text_y


def dump(table):
    """One line per font field, the characters as `"code": [advance, bitmap width]` pairs, eight to a line."""
    lines = ['{']
    for font_index, (name, entry) in enumerate(table.items()):
        lines.append(f' {json.dumps(name)}: {{')
        for key in ('ascending_height', 'descending_height', 'leading_height', 'leading_width'):
            lines.append(f'  {json.dumps(key)}: {entry[key]},')
        lines.append('  "characters": {')
        pairs = [f'{json.dumps(code)}: {json.dumps(widths)}' for code, widths in entry['characters'].items()]
        rows = [', '.join(pairs[start:start + 8]) for start in range(0, len(pairs), 8)]
        lines.extend(f'   {row}' + (',' if index < len(rows) - 1 else '') for index, row in enumerate(rows))
        lines.append('  }')
        lines.append(' }' + (',' if font_index < len(table) - 1 else ''))
    lines.append('}')
    return '\n'.join(lines) + '\n'


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--update', metavar='MAP', help='rewrite the table from a ui.map')
    group.add_argument('--check', metavar='MAP', help='compare the table with a ui.map')
    args = parser.parse_args(argv)
    table = read_fonts(args.update or args.check)
    text = dump(table)
    path = metrics_path()
    if args.update:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        return 0
    if not path.exists() or path.read_text(encoding='utf-8') != text:
        print(f'{path} does not match the map; run --update', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
