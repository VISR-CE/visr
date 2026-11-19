"""The redrawn menu text places each glyph inside the game's advance by the font's side-bearing proportions;
a glyph with a negative bearing (j's tail) must not run away from its pen position."""
import re
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / 'source/rasterizer/rasterizer_text.c'


def place(width, advance, left, right):
    """mirrors the redraw branch of `rasterizer_draw_hires_glyph`: the ink's left edge relative to the pen"""
    ink, lsb, rsb = right - left, left, advance - right
    gap = width - ink
    if lsb < 0 or rsb < 0:
        return (width - advance) * 0.5 + lsb
    return gap * lsb / (lsb + rsb) if lsb + rsb > 0 else gap * 0.5


def test_source_has_the_negative_bearing_branch():
    text = SOURCE.read_text()
    assert re.search(r'if \(lsb < 0\.0f \|\| rsb < 0\.0f\)\n\t+left = pen_x \+ \(font_character->character_width - glyph\.advance\) \* 0\.5f \+ lsb;', text)


def test_j_stays_within_a_letter_of_its_pen():
    # a j-like glyph: ink 4.2 wide starting 1.0 left of the pen, advance 3.3 (rsb +0.1 makes lsb + rsb ~ -0.9)
    for left, right, advance in ((-1.0, 3.2, 3.3), (-1.0, 3.0, 2.9), (-0.9, 3.1, 3.3)):
        assert -2.0 < place(6.0, advance, left, right) < 3.0


def test_ordinary_glyph_keeps_proportional_gap():
    assert abs(place(10.0, 9.0, 1.0, 7.0) - 4.0 / 3.0) < 1e-6
