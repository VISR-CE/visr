#!/usr/bin/env python3
"""Redraw the Xbox server browser's text-free art at four pixels per menu unit.

Run ``python3 tools/browser_assets.py``. Requires rsvg-convert; the supplied
CC0 icon redraws live beside the generated art so regeneration is self-contained.
Widget dimensions in tools/xbox_menus.py must match the menu-unit sizes here.
"""

import argparse
import math
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import menu_geometry  # noqa: E402


ROOT = Path(__file__).resolve().parent.parent
ART = ROOT / "port/assets/menus/xbox/art/browser"
SCALE = 4
ROW_HEIGHT, ROW_TAB_WIDTH, ROW_ART_WIDTH = (menu_geometry.BROWSER_ROW_HEIGHT, menu_geometry.BROWSER_TAB_WIDTH,
                                            menu_geometry.BROWSER_TAB_REACH)
PANEL_FILL = menu_geometry.BROWSER_PANEL_FILL
GAMETYPES = ("coop", "ctf", "slayer", "oddball", "king", "race")
SVG_NS = "http://www.w3.org/2000/svg"


def svg(width, height, content):
    return (f'<svg xmlns="{SVG_NS}" width="{width}" height="{height}" '
            f'viewBox="0 0 {width} {height}">{content}</svg>')


def rounded(width, height, fill, stroke="none", radius=7, top=0, stroke_width=1.5):
    # Inset the outline so it is not clipped by the widget or its PNG frame.
    return svg(width, height,
               f'<rect x="{stroke_width / 2}" y="{top + stroke_width / 2}" width="{width - stroke_width}" '
               f'height="{height - top - stroke_width}" rx="{radius}" '
               f'fill="{fill}" stroke="{stroke}" stroke-width="{stroke_width}"/>')


def render(output, name, width, height, document, scale=SCALE):
    subprocess.run(["rsvg-convert", "--width", str(width * scale),
                    "--height", str(height * scale), "--output",
                    str(output / f"{name}.png")], input=document.encode(), check=True)


def joined_outline_path(chosen):
    """The panel and the chosen game's tab as ONE outline (a path, in menu units from the tab's left and the panel's top).

    chosen is the visible row (0 to BROWSER_ROWS - 1) or None for the panel alone. The tab reaches from the left edge
    into the panel's left side; the first and last rows share the panel's top and bottom, so there the edge runs
    straight across with no panel corner left behind. Every other row meets the panel's left side at a square inside
    corner. The stroke is inset by half its width so it is not clipped."""
    g, h = menu_geometry, menu_geometry.BROWSER_BORDER / 2
    right = g.BROWSER_JOINT_WIDTH - h
    bottom = g.BROWSER_DETAIL_HEIGHT - h
    big, small = g.BROWSER_PANEL_RADIUS, g.BROWSER_TAB_RADIUS
    left_x = g.BROWSER_PANEL_LEFT - g.BROWSER_LIST_X + h  # the panel's left line
    if chosen is None:
        return (f'M{left_x + big} {h} H{right - big} a{big} {big} 0 0 1 {big} {big} V{bottom - big} '
                f'a{big} {big} 0 0 1 {-big} {big} H{left_x + big} a{big} {big} 0 0 1 {-big} {-big} '
                f'V{h + big} a{big} {big} 0 0 1 {big} {-big} Z')
    top = chosen * g.BROWSER_ROW_PITCH + h
    low = top - h + g.BROWSER_ROW_HEIGHT - h
    d = f'M{h} {top + small} a{small} {small} 0 0 1 {small} {-small}'
    if chosen == 0:
        d += f' H{right - big}'
    else:
        d += f' H{left_x} V{h + big} a{big} {big} 0 0 1 {big} {-big} H{right - big}'
    d += f' a{big} {big} 0 0 1 {big} {big} V{bottom - big} a{big} {big} 0 0 1 {-big} {big}'
    if chosen == g.BROWSER_ROWS - 1:
        d += f' H{h + small}'
    else:
        d += f' H{left_x + big} a{big} {big} 0 0 1 {-big} {-big} V{low} H{h + small}'
    return d + f' a{small} {small} 0 0 1 {-small} {-small} Z'


def joint_svg(chosen):
    g = menu_geometry
    return svg(g.BROWSER_JOINT_WIDTH, g.BROWSER_DETAIL_HEIGHT,
               f'<path d="{joined_outline_path(chosen)}" fill="{PANEL_FILL}" '
               f'fill-opacity="{g.BROWSER_PANEL_FILL_OPACITY}" stroke="{g.BROWSER_PANEL_BORDER}" '
               f'stroke-width="{g.BROWSER_BORDER}" stroke-linejoin="round"/>')


def generate(output, icons):
    output.mkdir(parents=True, exist_ok=True)

    def draw(name, width, height, document, scale=SCALE):
        render(output, name, width, height, document, scale)

    # Backdrop band: transparent under the title, a bright rule, then the dark
    # blue fill to the bottom of the screen, behind the legend. The menu widens
    # it to the display and it stretches over the 480 design lines.
    rule = menu_geometry.TITLE_RULE_Y
    draw("backdrop", 4, 480, svg(4, 480,
         '<defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1">'
         '<stop offset="0" stop-color="#072751" stop-opacity="0.69"/>'
         '<stop offset="1" stop-color="#062348" stop-opacity="0.69"/></linearGradient></defs>'
         f'<rect x="0" y="{rule}" width="4" height="{409 - rule}" fill="url(#g)"/>'
         f'<rect x="0" y="{rule}" width="4" height="1" fill="#36afff"/>'
         '<rect x="0" y="409" width="4" height="1" fill="#2896ff"/>'
         '<rect x="0" y="410" width="4" height="34" fill="#000000" fill-opacity="0.696"/>'), scale=2)

    for index, (fill, border) in enumerate((("#0a2245", "none"),
                                           ("#12396e", "#7fc4ff"))):
        draw(f"option__{index}", 532, 54, rounded(532, 54, fill, border))

    # The game rows are tabs, as the gametype editor's: outlined all around, and the selected one open on its right
    # edge and filled as the detail panel is, reaching into the panel (drawn over its left border) so tab and panel
    # are one shape. The art is ROW_ART_WIDTH wide, the tab itself ROW_TAB_WIDTH of it.
    height, radius, stroke = ROW_HEIGHT, 7, menu_geometry.BROWSER_BORDER
    half = stroke / 2
    draw("row__0", ROW_ART_WIDTH, height, svg(ROW_ART_WIDTH, height,
         f'<rect x="{half}" y="{half}" width="{ROW_TAB_WIDTH - stroke}" height="{height - stroke}" rx="{radius}" '
         f'fill="#0a2245" stroke="#1f5fae" stroke-width="{stroke}"/>'))
    # (the chosen game's tab is drawn by its joint, with the panel: this frame draws nothing)
    draw("row__1", ROW_ART_WIDTH, ROW_HEIGHT, svg(ROW_ART_WIDTH, ROW_HEIGHT, ""))

    # Four states let code distinguish focus from the selected type chip.
    states = (("none", "#1f5fae"), ("#12396e", "#7fc4ff"),
              ("#0d2c57", "#7fc4ff"), ("#174b89", "#cfe8ff"))
    for name, width in (("chip_all", 78), ("chip_multiplayer", 144),
                        ("chip_coop", 100), ("toggle", 174)):
        for index, (fill, border) in enumerate(states):
            draw(f"{name}__{index}", width, 54,
                 rounded(width, 54, fill, border, top=22, stroke_width=3 if index >= 2 else 1.5))

    # (transparent, so 1x: thirty preview frames fall back to it, and at 4x they cost the Apple TV HD about 90 MB)
    draw("preview_none", 310, 155, svg(310, 155, ""), scale=1)
    for chosen in (*range(menu_geometry.BROWSER_ROWS), None):
        draw(f"joint__{'none' if chosen is None else chosen}", menu_geometry.BROWSER_JOINT_WIDTH,
             menu_geometry.BROWSER_DETAIL_HEIGHT, joint_svg(chosen))
    draw("big", 294, 20, rounded(294, 20, "#e8b520", radius=3))
    # The detail panel's password pill (HUD blue)
    # The detail panel's info pills: normal (HUD blue) and blocked (grayed as the disabled rows are, never red).
    for index, border in enumerate(("#1f5fae", "#4a5563")):
        draw(f"badge__{index}", menu_geometry.BROWSER_PILL_WIDTH, menu_geometry.BROWSER_PILL_HEIGHT,
             rounded(menu_geometry.BROWSER_PILL_WIDTH, menu_geometry.BROWSER_PILL_HEIGHT, "none", border, radius=3))
    # The lock inside the password pill.
    draw("lock", 12, 14, svg(12, 14,
         '<path d="M3 6V4.5a3 3 0 0 1 6 0V6" fill="none" stroke="#cfe8ff" stroke-width="1.4"/>'
         '<rect x="1" y="6" width="10" height="7.2" rx="1.4" fill="#cfe8ff"/>'))

    for index, name in enumerate(GAMETYPES):
        icon = ET.parse(icons / f"{name}-nobg.svg").getroot()
        icon.set("width", "36")
        icon.set("height", "36")
        draw(f"gametype__{index}", 36, 36, ET.tostring(icon, encoding="unicode"))

    # The detail panel's gametype picture, over the map picture's top right: the same redraws at the badge's size
    # (menu_geometry), then the original "?" frame of game_type_grafix for when no game is chosen.
    badge = (menu_geometry.BROWSER_BADGE_WIDTH, menu_geometry.BROWSER_BADGE_HEIGHT)
    for index, name in enumerate(GAMETYPES + ("unknown",)):
        icon = ET.parse(icons / f"{name}-nobg.svg").getroot()
        icon.set("width", str(badge[0]))
        icon.set("height", str(badge[1]))
        draw(f"gametype_large__{index}", *badge, ET.tostring(icon, encoding="unicode"))

    # The scrim along the map picture's foot, under the server name and the gametype line: a dark band, a little
    # lighter at its top so it reads as shading of the picture, not a box.
    scrim_width, scrim_height = menu_geometry.BROWSER_PREVIEW_WIDTH, menu_geometry.BROWSER_SCRIM_HEIGHT
    draw("scrim", scrim_width, scrim_height, svg(scrim_width, scrim_height,
         '<defs><linearGradient id="s" x1="0" y1="0" x2="0" y2="1">'
         '<stop offset="0" stop-color="#030c1c" stop-opacity="0.45"/>'
         '<stop offset="0.35" stop-color="#030c1c" stop-opacity="0.78"/>'
         '<stop offset="1" stop-color="#030c1c" stop-opacity="0.86"/></linearGradient></defs>'
         f'<rect width="{scrim_width}" height="{scrim_height}" fill="url(#s)"/>'))

    circumference = 2 * math.pi * 17.5
    for index in range(33):
        track = '<circle cx="21" cy="21" r="17.5" fill="none" stroke="#133e75" stroke-width="3.5"/>'
        fill = ""
        if index:
            arc = circumference * index / 32
            # A whole circle avoids a round-cap seam on the fully occupied ring.
            dash = (f' stroke-dasharray="{arc:.6f} {circumference:.6f}"'
                    if index < 32 else "")
            fill = (f'<circle cx="21" cy="21" r="17.5" fill="none" '
                    f'stroke="#4aa8ff" stroke-width="3.5" stroke-linecap="round"'
                    f'{dash} transform="rotate(-90 21 21)"/>')
        draw(f"gauge__{index:02d}", 42, 42, svg(42, 42, track + fill))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ART)
    parser.add_argument("--icons", type=Path, default=ART / "icons")
    args = parser.parse_args()
    generate(args.output, args.icons)


if __name__ == "__main__":
    main()
