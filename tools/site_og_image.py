#!/usr/bin/env python3
"""Draw the GitHub Pages site's Open Graph image (docs/site/og.png) as a small copy of the page's hero:
the platform line, the app icon beside the VISR wordmark, and the headline, on the page's HUD grid.

    python3 tools/site_og_image.py [--icon PNG] [--out PNG]

Colors and type follow docs/site/shell.css: the --bg background with the 64 px --hud-grid lines, the
.kicker line in --hud-gold monospace capitals with its pulse dot, the .wordmark in Orbitron with its
--hud-glow, and the h1 with "whole new dimension" in --hud-gold. Text comes from tools/build_site.py
(KICKER) so the card says what the page says. Rerun it when the icon, the platform list, the headline
or the page's colors change.

Needs Pillow (pip install pillow), which reads WOFF2 fonts directly, and macOS's system fonts (SF Pro
and SF Mono in /System/Library/Fonts) for the headline and the platform line."""
import argparse
import importlib.util
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
WIDTH, HEIGHT = 1200, 630
BACKGROUND = (5, 14, 20)      # shell.css --bg
GRID = (63, 214, 255, 31)     # --hud-grid, #3fd6ff1f
GRID_STEP = 64
INK = (232, 246, 250)
GOLD = (255, 194, 77)         # --hud-gold
GLOW = (41, 153, 255)         # --hud-glow without its alpha

WORDMARK_FONT = ROOT / "docs/site/fonts/Orbitron-VariableFont_wght.woff2"
HEADLINE_FONT = Path("/System/Library/Fonts/SFNS.ttf")
HEADLINE_ITALIC = Path("/System/Library/Fonts/SFNSItalic.ttf")
MONO_FONT = Path("/System/Library/Fonts/SFNSMono.ttf")

ICON_SIZE = 200
WORDMARK_SIZE = 150
KICKER_SIZE = 26
HEADLINE_SIZE = 50
HEADLINE = [("Experience ", INK, False), ("Halo: Combat Evolved", INK, True), (" in a ", INK, False), ("whole new dimension", GOLD, False)]  # the h1, with its <em>


def kicker_text():
    """The page's platform line, read from tools/build_site.py so the two cannot drift apart."""
    spec = importlib.util.spec_from_file_location("build_site", ROOT / "tools/build_site.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.KICKER.upper()


def font(path, size, weight=None):
    """A face at `size`, with its Weight axis set when it has one (SF Pro's first axis is Width, so set
    axes by name, never by position)."""
    face = ImageFont.truetype(str(path), size)
    if weight is None:
        return face
    try:
        axes = face.get_variation_axes()
    except OSError:
        return face    # a static font has no axes
    face.set_variation_by_axes([weight if axis["name"] in (b"Weight", "Weight") else axis["default"] for axis in axes])
    return face


def tracked_width(draw, text, face, tracking):
    return sum(draw.textlength(ch, font=face) for ch in text) + tracking * (len(text) - 1)


def draw_tracked(draw, xy, text, face, fill, tracking):
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=face, fill=fill)
        x += draw.textlength(ch, font=face) + tracking


def rounded_mask(size, radius):
    """An antialiased rounded-square mask, drawn at 4x and reduced."""
    big = Image.new("L", (size * 4, size * 4), 0)
    ImageDraw.Draw(big).rounded_rectangle((0, 0, size * 4 - 1, size * 4 - 1), radius=radius * 4, fill=255)
    return big.resize((size, size), Image.LANCZOS)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--icon", type=Path, default=ROOT / "port/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png")
    parser.add_argument("--out", type=Path, default=ROOT / "docs/site/og.png")
    args = parser.parse_args()

    card = Image.new("RGBA", (WIDTH, HEIGHT), BACKGROUND + (255,))
    grid = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    lines = ImageDraw.Draw(grid)
    for x in range(0, WIDTH, GRID_STEP):
        lines.line((x, 0, x, HEIGHT), fill=GRID)
    for y in range(0, HEIGHT, GRID_STEP):
        lines.line((0, y, WIDTH, y), fill=GRID)
    card = Image.alpha_composite(card, grid)
    draw = ImageDraw.Draw(card)

    kicker = kicker_text()
    kicker_face = font(MONO_FONT, KICKER_SIZE)
    kicker_tracking = 0.14 * KICKER_SIZE
    wordmark_face = font(WORDMARK_FONT, WORDMARK_SIZE, 500)
    wordmark_tracking = 0.08 * WORDMARK_SIZE
    def headline_faces(size):
        return font(HEADLINE_FONT, size, 700), font(HEADLINE_ITALIC, size, 700)

    def headline_width(faces):
        return sum(draw.textlength(text, font=faces[italic]) for text, _, italic in HEADLINE)

    headline_size = HEADLINE_SIZE
    faces = headline_faces(headline_size)
    while headline_width(faces) > WIDTH - 2 * GRID_STEP:
        headline_size -= 2    # keep a grid square of margin each side
        faces = headline_faces(headline_size)

    # Vertical layout: kicker, then the brand row, then the headline, centered as a block
    kicker_h = KICKER_SIZE
    brand_h = ICON_SIZE
    headline_h = HEADLINE_SIZE
    gap_a, gap_b = 40, 44
    block = kicker_h + gap_a + brand_h + gap_b + headline_h
    top = (HEIGHT - block) // 2

    # The platform line with its pulse dot, centered
    dot = 14
    kicker_w = dot + 16 + tracked_width(draw, kicker, kicker_face, kicker_tracking)
    kx = (WIDTH - kicker_w) / 2
    glow_dot = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    ImageDraw.Draw(glow_dot).ellipse((kx - 6, top + 6 - 6, kx + dot + 6, top + 6 + dot + 6), fill=GOLD + (150,))
    card = Image.alpha_composite(card, glow_dot.filter(ImageFilter.GaussianBlur(8)))
    draw = ImageDraw.Draw(card)
    draw.ellipse((kx, top + 6, kx + dot, top + 6 + dot), fill=GOLD)
    draw_tracked(draw, (kx + dot + 16, top - 4), kicker, kicker_face, GOLD, kicker_tracking)

    # The brand row: icon and wordmark, centered together
    brand_top = top + kicker_h + gap_a
    word_w = tracked_width(draw, "VISR", wordmark_face, wordmark_tracking)
    gap = 36
    left = round((WIDTH - (ICON_SIZE + gap + word_w)) / 2)
    mask = rounded_mask(ICON_SIZE, round(0.22 * ICON_SIZE))
    halo = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    ImageDraw.Draw(halo).rounded_rectangle((left - 10, brand_top - 10, left + ICON_SIZE + 10, brand_top + ICON_SIZE + 10),
                                           radius=60, fill=(31, 169, 212, 115))
    card = Image.alpha_composite(card, halo.filter(ImageFilter.GaussianBlur(30)))
    icon = Image.open(args.icon).convert("RGBA").resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
    card.paste(icon, (left, brand_top), mask)

    cap_top, cap_bottom = wordmark_face.getbbox("VISR")[1], wordmark_face.getbbox("VISR")[3]
    wx = left + ICON_SIZE + gap
    wy = brand_top + ICON_SIZE / 2 - (cap_bottom - cap_top) / 2 - cap_top
    glow = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    draw_tracked(ImageDraw.Draw(glow), (wx, wy), "VISR", wordmark_face, GLOW + (255,), wordmark_tracking)
    card = Image.alpha_composite(card, glow.filter(ImageFilter.GaussianBlur(22)))
    card = Image.alpha_composite(card, glow.filter(ImageFilter.GaussianBlur(8)))
    draw = ImageDraw.Draw(card)
    draw_tracked(draw, (wx, wy), "VISR", wordmark_face, INK, wordmark_tracking)

    # The headline, centered, with its accent words in gold
    head_top = brand_top + brand_h + gap_b
    hx = (WIDTH - headline_width(faces)) / 2
    for text, color, italic in HEADLINE:
        draw.text((hx, head_top), text, font=faces[italic], fill=color)
        hx += draw.textlength(text, font=faces[italic])

    card.convert("RGB").save(args.out, optimize=True)
    print(f"{args.out}: {WIDTH}x{HEIGHT}")


if __name__ == "__main__":
    main()
