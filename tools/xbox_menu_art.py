#!/usr/bin/env python3
"""Draw the Xbox look's fallback art: button glyphs, row outline, the redrawn pictures and the titles the game has no
picture for (needs rsvg-convert).

    python3 tools/xbox_menu_art.py

The files are written to port/assets/menus/xbox/art.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess

import menu_geometry

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
    # the status box under the list: the same outline as the rows and the text fields
    svg = output / 'browser_status__0.svg'
    svg.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="544" height="64" viewBox="0 0 544 64">'
                   '<rect x="2" y="1" width="540" height="62" rx="10" fill="#001125" fill-opacity=".8" '
                   'stroke="#2896ff" stroke-width="2"/></svg>\n')
    subprocess.run(['rsvg-convert', '--zoom', '4', '--output', str(output / 'browser_status__0.png'),
                    str(svg)], check=True)


# the pictures of the profile editor and multiplayer rows that ship as our own art (xbox/art/<name>.svg, 512 by 256):
# AUDIO, VIDEO, INTERNET (also NETWORK and DIRECT LINK), and the PC look's ABOUT emblem and MOUSE drawing
REDRAWS = ('audio', 'video', 'internet', 'internet_off', 'about', 'mouse')


def redraw_art(output):
    """The redraws' PNGs, at 4 times their drawing's size as the other pictures are."""
    for name in REDRAWS:
        subprocess.run(['rsvg-convert', '--zoom', '4', '--output', str(output / f'{name}.png'),
                        str(output / f'{name}.svg')], check=True)


# the VISR helmet beside every screen's title, drawn in the menus' outline style (xbox/art/visr_title_icon.svg, 64 by 64
# with the helmet in the middle 52 by 39 and room for its glow). The helmet is as tall as the titles' capitals
# (22 menu units), so the picture is 22 * 64 / 39 = 36 units square, drawn at 4 times that.
ICON_SOURCE = MENUS / 'xbox/art/visr_title_icon.svg'
TITLE_ICON, TITLE_ICON_SIZE = 'visr_title_icon', 36


def title_icon(output, source=ICON_SOURCE):
    """The helmet drawing rendered to its picture."""
    size = TITLE_ICON_SIZE * 4
    subprocess.run(['rsvg-convert', '--width', str(size), '--height', str(size), '--output',
                    str(output / f'{TITLE_ICON}.png'), str(source)], check=True)


def toggle_art(output):
    """A pill of menu_geometry's switch size, rasterized at 4x for larger displays."""
    width, height = menu_geometry.SWITCH_WIDTH, menu_geometry.SWITCH_HEIGHT
    for on in (0, 1):
        svg = output / f'toggle_switch__{on}.svg'
        color = '#4ea0ff' if on else '#424b57'
        radius = height / 2
        center = width - radius if on else radius
        svg.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
                       f'viewBox="0 0 {width} {height}">'
                       f'<rect width="{width}" height="{height}" rx="{radius}" fill="{color}"/>'
                       f'<circle cx="{center}" cy="{radius}" r="{radius - 2}" fill="#fff"/></svg>\n')
        subprocess.run(['rsvg-convert', '--zoom', '4', '--output',
                        str(output / f'toggle_switch__{on}.png'), str(svg)], check=True)


# Option row pitch in menu units; matches HALO_DROPDOWN_PITCH in halo_menu_dropdown.h.
DROPDOWN_PITCH = 52


def dropdown_art(output):
    """Menu-blue pointer controls; panel frames keep the 1.5-unit border at every height."""
    from PIL import Image

    shapes = [
        ('dropdown_chevron', 12, 8,
         '<path d="M2 2 L6 6 L10 2" fill="none" stroke="#4ea0ff" stroke-width="2"/>'),
        ('dropdown_check', 12, 12,
         '<path d="M2 6 L5 9 L10 3" fill="none" stroke="#fff" stroke-width="2"/>'),
    ]
    for rows in range(1, 8):
        height = rows * DROPDOWN_PITCH + 3
        shapes.append((f'dropdown_panel__{rows - 1}', 165, height,
                       f'<rect x=".75" y=".75" width="163.5" height="{height - 1.5}" '
                       'fill="#001125" stroke="#4ea0ff" stroke-width="1.5"/>'))
    for name, width, height, shape in shapes:
        svg = output / f'{name}.svg'
        svg.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
                       f'viewBox="0 0 {width} {height}">{shape}</svg>\n')
        png = output / f'{name}.png'
        subprocess.run(['rsvg-convert', '--zoom', '4', '--output', str(png), str(svg)], check=True)
        # rsvg-convert drops the alpha channel from fully opaque art; embed_assets.py needs RGBA.
        Image.open(png).convert('RGBA').save(png)


def slider_art(output):
    """Slider track pieces and a white 15-unit thumb with a soft blue halo."""
    from PIL import Image
    shapes = [
        ('slider_track', 1, 5, '<rect width="1" height="5" fill="#17304d"/>'),
        ('slider_fill', 1, 5, '<rect width="1" height="5" fill="#4ea0ff"/>'),
        ('slider_thumb', 25, 25,
         '<defs><radialGradient id="halo"><stop offset="0.5" stop-color="#4ea0ff" stop-opacity=".6"/>'
         '<stop offset="1" stop-color="#4ea0ff" stop-opacity="0"/></radialGradient></defs>'
         '<circle cx="12.5" cy="12.5" r="12.5" fill="url(#halo)"/>'
         '<circle cx="12.5" cy="12.5" r="7.5" fill="#fff"/>'),
    ]
    for name, width, height, shape in shapes:
        svg = output / f'{name}.svg'
        svg.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
                       f'viewBox="0 0 {width} {height}">{shape}</svg>\n')
        png = svg.with_suffix('.png')
        subprocess.run(['rsvg-convert', '--zoom', '4', '--output', str(png), str(svg)], check=True)
        Image.open(png).convert('RGBA').save(png)


# the titles of screens the Xbox look makes that the game has no title picture for at the standard size: picture
# (in xbox/art), text
TITLES = {
    'header_edit_multiplayer_gametype': 'EDIT MULTIPLAYER GAMETYPE',
}
# a title's plate fades out before the picture's right edge: the most alpha (of 255) its last column may keep
TITLE_EDGE_ALPHA = 4


def title_art(output, titles=None):
    """Each title set as the port's titles are (OpenCE at TITLE_CAP from TITLE_LEFT, over title_backdrop: see
    tools/port_settings.py), 512 by 64 units at 4x. A title too long for its plate to fade out inside the picture is
    set smaller, in steps of 2%, until it does."""
    import sys

    import numpy as np
    from PIL import Image, ImageDraw, ImageFont

    sys.path.insert(0, str(ROOT / 'tools'))
    import port_settings

    scale = 4
    face = ROOT / 'port/assets/fonts/OpenCE-Regular.ttf'
    probe = ImageFont.truetype(str(face), 1000)
    _, h_top, _, h_bottom = probe.getbbox('H')
    for name, text in (titles or TITLES).items():
        cap = port_settings.TITLE_CAP
        while True:
            font = ImageFont.truetype(str(face), round(cap * scale * 1000 / (h_bottom - h_top)))
            svg = output / f'{name}.svg'
            svg.write_text(port_settings.title_backdrop(font.getlength(text) / scale))
            png = output / f'{name}.png'
            subprocess.run(['rsvg-convert', '--zoom', str(scale), '--output', str(png), str(svg)], check=True)
            image = Image.open(png).convert('RGBA')
            draw = ImageDraw.Draw(image)
            draw.text((port_settings.TITLE_LEFT * scale, port_settings.TITLE_TOP * scale - font.getbbox('H')[1]), text,
                      font=font, fill=port_settings.TITLE_COLOR)
            if int(np.asarray(image)[:, -1, 3].max()) <= TITLE_EDGE_ALPHA or cap < port_settings.TITLE_CAP / 2:
                break
            cap *= 0.98
        svg.unlink()
        image.save(png, optimize=True)


def generate(menus=MENUS):
    menus = Path(menus)
    output = menus / 'xbox/art'
    output.mkdir(parents=True, exist_ok=True)
    toggle_art(output)
    dropdown_art(output)
    slider_art(output)
    browser_art(output)
    redraw_art(output)
    title_icon(output)
    title_art(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--menus', type=Path, default=MENUS, help='menu assets folder')
    generate(parser.parse_args().menus)


if __name__ == '__main__':
    main()
