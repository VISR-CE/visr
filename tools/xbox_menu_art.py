#!/usr/bin/env python3
"""Draw the Xbox look's fallback art: button glyphs, row outline and the redrawn pictures (needs rsvg-convert).

    python3 tools/xbox_menu_art.py

The files are written to port/assets/menus/xbox/art.
"""
from __future__ import annotations

import argparse
import base64
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'


def browser_art(output):
    """Fallback redraws for map button art and a row outline with inset caps."""
    letters = {
        'a': 'M7 18 L11 8 L15 18 M8.5 15 H13.5',
        'b': 'M8 8 V18 H12 Q17 18 15 14 Q14 12 11 13 H8 M8 8 H12 Q17 8 15 11 Q14 13 11 13',
        'x': 'M7 8 L15 18 M15 8 L7 18',
        'y': 'M7 8 L11 13 L15 8 M11 13 V18',
    }
    colors = {'a': '#24852f', 'b': '#ba222d', 'x': '#247dc0', 'y': '#d4a124'}
    for button, path in letters.items():
        svg = output / f'{button}_butn.svg'
        svg.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32">'
                       '<defs><linearGradient id="shine" x2="0.8" y2="1">'
                       '<stop stop-color="#ffffff" stop-opacity=".65"/><stop offset="1" stop-opacity=".3"/>'
                       '</linearGradient></defs>'
                       f'<ellipse cx="10.5" cy="13" rx="8.5" ry="11" fill="{colors[button]}" stroke="#ddd"/>'
                       '<ellipse cx="10.5" cy="13" rx="7.5" ry="10" fill="url(#shine)"/>'
                       f'<path d="{path}" fill="none" stroke="white" stroke-width="1.6" stroke-linecap="round" '
                       'stroke-linejoin="round"/></svg>\n')
        subprocess.run(['rsvg-convert', '--zoom', '4', '--output', str(output / f'{button}_butn.png'),
                        str(svg)], check=True)
    for index in (0, 1):
        svg = output / f'browser_row__{index}.svg'
        shape = ('<rect x="2" y="1" width="540" height="22" rx="10" fill="#001125" '
                 'fill-opacity=".8" stroke="#2896ff" stroke-width="2"/>') if index else ''
        svg.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="544" height="24" '
                       f'viewBox="0 0 544 24">{shape}</svg>\n')
        subprocess.run(['rsvg-convert', '--zoom', '4', '--output', str(output / f'browser_row__{index}.png'),
                        str(svg)], check=True)


# the pictures of the profile editor and multiplayer rows that ship as our own art (xbox/art/<name>.svg, 512 by 256):
# AUDIO, VIDEO, INTERNET (also NETWORK and DIRECT LINK), and the PC look's ABOUT emblem and MOUSE drawing
REDRAWS = ('audio', 'video', 'internet', 'about', 'mouse')


def redraw_art(output):
    """The redraws' PNGs, at 4 times their drawing's size as the other pictures are."""
    for name in REDRAWS:
        subprocess.run(['rsvg-convert', '--zoom', '4', '--output', str(output / f'{name}.png'),
                        str(output / f'{name}.svg')], check=True)


# the app icon beside every screen's title: as tall as the titles' capitals (22 menu units), drawn at 4 times that
ICON_SOURCE = ROOT / 'port/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png'
TITLE_ICON, TITLE_ICON_SIZE = 'visr_title_icon', 22


def title_icon(output, source=ICON_SOURCE):
    """The app icon, reduced from its 1024-pixel picture, with the rounded corners the Home Screen gives it."""
    size = TITLE_ICON_SIZE * 4
    image = base64.b64encode(Path(source).read_bytes()).decode('ascii')
    # (iOS rounds an icon's corners by about 22.4% of its side)
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{size}" '
           f'height="{size}" viewBox="0 0 1024 1024"><defs><clipPath id="corners">'
           '<rect width="1024" height="1024" rx="229"/></clipPath></defs>'
           f'<image width="1024" height="1024" clip-path="url(#corners)" xlink:href="data:image/png;base64,{image}"/>'
           '</svg>\n')
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder) / f'{TITLE_ICON}.svg'
        path.write_text(svg)
        subprocess.run(['rsvg-convert', '--width', str(size), '--height', str(size), '--output',
                        str(output / f'{TITLE_ICON}.png'), str(path)], check=True)


def generate(menus=MENUS):
    menus = Path(menus)
    output = menus / 'xbox/art'
    output.mkdir(parents=True, exist_ok=True)
    browser_art(output)
    redraw_art(output)
    title_icon(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--menus', type=Path, default=MENUS, help='menu assets folder')
    generate(parser.parse_args().menus)


if __name__ == '__main__':
    main()
