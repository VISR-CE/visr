#!/usr/bin/env python3
"""Draw the main menu's CUSTOM CAMPAIGN label in the lettering of its other rows (needs rsvg-convert).

    python3 tools/xbox_menu_art.py

The main menu's labels are pictures: two frames (blue; white with a glow) of 256 x 64 drawn from the letters of the
Halo main menu font (port/assets/menus/svg/shell/main_menu/menu_*__0.svg, from ui-svg-handmade). CUSTOM CAMPAIGN is
not one of them, so its frames are laid out here from the same letters, scaled down to fit, and written next to the
Xbox look's files (port/assets/menus/xbox/art). The letters' advances are measured from their shapes (the font's
own are not in the files), which spaces them evenly rather than by the font's kerning.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import tempfile
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'
SOURCES = ('menu_load_campaign', 'menu_settings', 'menu_quit', 'menu_profile')
TEXT = 'CUSTOM CAMPAIGN'
LETTERS = {'C': 'gC', 'U': 'gU', 'S': 'gS', 'T': 'gT', 'O': 'gO', 'M': 'gM', 'A': 'gA', 'P': 'gP', 'I': 'gI',
           'G': 'gG', 'N': 'gN'}
# the existing labels' letter size, their top, the room a label has and the gap between letters and words
SCALE, TOP, HEIGHT, ROOM, GAP, SPACE = 0.9037, 6.57, 21.5, 244.0, 2.6, 11.0
NS = '{http://www.w3.org/2000/svg}'


def glyph_paths(svg_dir):
    paths = {}
    for name in SOURCES:
        for path in ET.parse(svg_dir / f'{name}__0.svg').getroot().iter(NS + 'path'):
            paths.setdefault(path.get('id'), path.get('d'))
    return paths


def glyph_widths(paths):
    """Each letter's width at the font's size, from rendering it alone."""
    from PIL import Image
    widths = {}
    with tempfile.TemporaryDirectory() as folder:
        for identifier, d in paths.items():
            svg = Path(folder) / f'{identifier}.svg'
            svg.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="-8 -8 64 64">'
                           f'<path d="{d}" fill="#fff"/></svg>')
            png = Path(folder) / f'{identifier}.png'
            subprocess.run(['rsvg-convert', '--zoom', '8', '--output', str(png), str(svg)], check=True)
            box = Image.open(png).convert('RGBA').getchannel('A').getbbox()
            widths[identifier] = (box[2] - box[0]) / 8
    return widths


def layout(widths):
    """The letters' x places (before scale) and the scale that fits the label."""
    places, x = [], 0.0
    for letter in TEXT:
        if letter == ' ':
            x += SPACE
            continue
        identifier = LETTERS[letter]
        places.append((identifier, x))
        x += widths[identifier] + GAP
    total = x - GAP
    scale = min(SCALE, ROOM / total)
    return places, total, scale


def label_svg(paths, places, total, scale, highlighted):
    left = (256 - total * scale) / 2
    top = TOP + HEIGHT * (SCALE - scale) / 2
    uses = ''.join(f'\n    <use href="#{i}" transform="translate({left + x * scale:.2f},{top:.2f}) scale({scale:.4f})"/>'
                   for i, x in places)
    defs = ''.join(f'\n    <path id="{i}" d="{paths[i]}"/>' for i in sorted({i for i, _ in places}))
    if highlighted:
        body = ('<filter id="glow" filterUnits="userSpaceOnUse" x="0" y="0" width="256" height="64">\n'
                '      <feGaussianBlur stdDeviation="2.19"/>\n    </filter>')
        groups = (f'  <g fill="#2895ff" stroke="#2895ff" stroke-width="6.3" stroke-linejoin="round" opacity="0.650" '
                  f'filter="url(#glow)">{uses}\n  </g>\n  <g id="text" fill="#fff">{uses}\n  </g>\n')
    else:
        body = ''
        groups = f'  <g id="text" fill="#2895ff" fill-opacity="0.467">{uses}\n  </g>\n'
    return ('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="256" '
            f'height="64" viewBox="0 0 256 64">\n  <defs>{defs}\n    {body}\n  </defs>\n  <!-- "{TEXT}" -->\n{groups}</svg>\n')


def generate(menus=MENUS):
    menus = Path(menus)
    paths = glyph_paths(menus / 'svg/shell/main_menu')
    widths = glyph_widths({i: paths[i] for i in LETTERS.values()})
    places, total, scale = layout(widths)
    output = menus / 'xbox/art'
    output.mkdir(parents=True, exist_ok=True)
    for frame in (0, 1):
        svg = output / f'menu_custom_campaign__{frame}.svg'
        svg.write_text(label_svg(paths, places, total, scale, frame == 1))
        subprocess.run(['rsvg-convert', '--width', '1024', '--height', '256', '--output',
                        str(output / f'menu_custom_campaign__{frame}.png'), str(svg)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--menus', type=Path, default=MENUS, help='menu assets folder')
    generate(parser.parse_args().menus)


if __name__ == '__main__':
    main()
