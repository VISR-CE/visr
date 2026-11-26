"""The real draw_unicode_string_compute_bounds_from_cursor (draw_string.c) and the layout it measures with: a piece of
text before or after an icon (draw_string_and_hack_in_icons, the HUD's messages) is drawn where it was measured, so
the last line before an icon is drawn ("Press" of SELECT PROFILE's "Press A to select"). The game drew each piece in
its measured ink's width, where the widest line wrapped at its last letter and the line after it dropped below the
box. Bug class: text drawn in a box other than the one it was measured in."""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, ROOT, build, enum_with, function, mutated, read, run, structure  # noqa: E402

CASES = ["last-line-before-an-icon", "drawn-where-measured", "cursor-after-the-piece"]

HELPER = "draw_unicode_string_compute_bounds_from_cursor"

# a fault in the function and the case that must catch it
NEGATIVE_CONTROLS = {
    "drawn-in-the-ink-width": (("\tdraw_bounds->x1 = bounds->x1;\n", ""), "last-line-before-an-icon"),
    "drawn-from-the-ink-edge": (("\tdraw_bounds->x0 = bounds->x0;\n", ""), "drawn-where-measured"),
    "no-indent-to-the-cursor": (("draw_string_set_indents(initial_indent > 0 ? initial_indent : 0, 0);",
                                 "draw_string_set_indents(0, 0);"), "cursor-after-the-piece"),
}

FUNCTIONS = ("draw_string_set_indents", "parse_string_new", "parse_unicode_string", "draw_unicode_string_partial",
             "draw_unicode_string", "text_bounds_draw_character", "draw_unicode_string_compute_bounds", HELPER)


def generated(fault=None):
    source = read("source/text/draw_string.c")
    config = enum_with(source, "_text_justification_left") + "\n"
    for name in ("font_drawing_globals", "font_character", "parse_string_state"):
        config += structure(source, name) + "\n"
    config += "static struct font_drawing_globals font_drawing_globals;\n"
    config += "static struct { rectangle2d bounds; struct font_header *last_font_header; } text_bounds_globals;\n"
    code = ""
    for name in FUNCTIONS:
        text = function(source, name)
        if fault and name == HELPER:
            text = mutated(text, *fault)
        code += text.replace("void draw_", "static void draw_", 1) if text.startswith("void draw_") else text
        code += "\n\n"
    return (("config.inc", config), ("under_test.inc", code))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("text_from_cursor", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("text_from_cursor", generated(fault)), case)
    assert status == CHECK_FAILED, f"draw_string.c with a fault ({control}) passed '{case}': the test cannot see it"


def test_pieces_at_a_cursor_use_the_function():
    """Every piece of text measured from a cursor (an indent set from it) is measured and boxed by the one function:
    no caller widens or narrows the measured box itself."""
    offenders = []
    paths = sorted(str(path.relative_to(ROOT)) for folder in ("source", "port") for path in (ROOT / folder).rglob("*.c"))
    for path in paths:
        if path == "source/text/draw_string.c":
            continue
        source = read(path)
        for match in re.finditer(r"(?<!void )draw_string_set_indents\((?!0, 0\))[^;]*;", source):
            offenders.append(f"{path}:{source.count(chr(10), 0, match.start()) + 1}: {match.group(0)}")
        for match in re.finditer(r"draw_unicode_string_compute_bounds\([^;]*;\s*\w+\.x0 -= 3;", source):
            offenders.append(f"{path}:{source.count(chr(10), 0, match.start()) + 1}: cursor moved by hand")
    assert not offenders, "\n".join(offenders)
    assert (ROOT / "source/text/draw_string.h").read_text(encoding="latin-1").count(HELPER) == 1
