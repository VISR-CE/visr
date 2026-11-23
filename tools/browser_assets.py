#!/usr/bin/env python3
"""Redraw the Xbox server browser's text-free art at four pixels per menu unit.

Run ``python3 tools/browser_assets.py``. Requires rsvg-convert; the supplied
CC0 icon redraws live beside the generated art so regeneration is self-contained.
Widget dimensions in tools/xbox_menus.py must match the menu-unit sizes here.
"""

import argparse
import math
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
ART = ROOT / "port/assets/menus/xbox/art/browser"
SCALE = 4
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


def generate(output, icons):
    output.mkdir(parents=True, exist_ok=True)

    def draw(name, width, height, document, scale=SCALE):
        render(output, name, width, height, document, scale)

    # Backdrop band: transparent under the title, a bright rule, then the dark
    # blue fill to the bottom of the screen, behind the legend. The menu widens
    # it to the display and it stretches over the 480 design lines.
    draw("backdrop", 4, 480, svg(4, 480,
         '<defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1">'
         '<stop offset="0" stop-color="#072751" stop-opacity="0.69"/>'
         '<stop offset="1" stop-color="#062348" stop-opacity="0.69"/></linearGradient></defs>'
         '<rect x="0" y="66" width="4" height="343" fill="url(#g)"/>'
         '<rect x="0" y="66" width="4" height="1" fill="#36afff"/>'
         '<rect x="0" y="409" width="4" height="1" fill="#2896ff"/>'
         '<rect x="0" y="410" width="4" height="34" fill="#000000" fill-opacity="0.696"/>'), scale=2)

    for index, (fill, border) in enumerate((("#0a2245", "none"),
                                           ("#12396e", "#7fc4ff"))):
        draw(f"row__{index}", 270, 50, rounded(270, 50, fill, border))
        draw(f"option__{index}", 532, 54, rounded(532, 54, fill, border))

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
    draw("detail", 312, 273, rounded(312, 273, "#0d2c57", "#4aa8ff", radius=9))
    draw("big", 294, 20, rounded(294, 20, "#e8b520", radius=3))
    # The detail panel's pills: normal (HUD blue) and blocked (grayed as the disabled rows are, never red).
    for index, border in enumerate(("#1f5fae", "#4b5663")):
        draw(f"badge__{index}", 120, 18, rounded(120, 18, "none", border, radius=3))
    # The lock inside the password pill.
    draw("lock", 12, 14, svg(12, 14,
         '<path d="M3 6V4.5a3 3 0 0 1 6 0V6" fill="none" stroke="#cfe8ff" stroke-width="1.4"/>'
         '<rect x="1" y="6" width="10" height="7.2" rx="1.4" fill="#cfe8ff"/>'))
    for index, (fill, border) in enumerate((("none", "none"),
                                           ("#174b89", "#cfe8ff"))):
        draw(f"join__{index}", 140, 45, rounded(140, 45, fill, border))

    for index, name in enumerate(GAMETYPES):
        icon = ET.parse(icons / f"{name}-nobg.svg").getroot()
        icon.set("width", "36")
        icon.set("height", "36")
        draw(f"gametype__{index}", 36, 36, ET.tostring(icon, encoding="unicode"))

    # The detail panel's gametype picture: the same redraws at the map picture's 145x118, then the
    # original "?" frame of game_type_grafix for when no game is chosen. Two pixels per unit keeps
    # seven frames cheap on small devices.
    for index, name in enumerate(GAMETYPES + ("unknown",)):
        icon = ET.parse(icons / f"{name}-nobg.svg").getroot()
        icon.set("width", "145")
        icon.set("height", "118")
        draw(f"gametype_large__{index}", 145, 118, ET.tostring(icon, encoding="unicode"), scale=2)

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
