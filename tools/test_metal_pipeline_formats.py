"""The Metal backend's render pipelines take their color format from the target they draw into.

The class of bug: a pipeline built once with a fixed color format (BGRA8) keeps drawing after its target changed format
(display.scene_precision 16 makes the scene's targets RGBA16Unorm and moves the layer to a 10-bit format), which Metal
refuses at draw time. The phone's companion picture under an external display was one (its pipeline stayed BGRA8 while
the phone layer had moved to BGR10_XR). Every assignment of a pipeline's color format must name a variable; a literal
format is allowed only where the target's format is fixed by design and listed here with why.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "port/ios/host/gpu_metal.m"

# literal color formats that are fixed by design: (the format, a phrase from the code around it, why)
FIXED = [
    ("MTLPixelFormatBGRA8Unorm", "resolve_color",
     "the foveated eye's resolve: stereo and foveated targets stay BGRA8 at every scene precision"),
    ("MTLPixelFormatR32Float", "resolve_depth", "a depth resolve into a float color target"),
]


def literal_assignments(text):
    """(line number, the format, the lines around it) for each pipeline color format set to a literal"""
    lines = text.splitlines()
    found = []
    for number, line in enumerate(lines, 1):
        match = re.search(r"colorAttachments\[\d+\]\.pixelFormat\s*=\s*(MTLPixelFormat\w+)", line)
        if match:
            found.append((number, match.group(1), "\n".join(lines[max(0, number - 4):number + 2])))
    return found


def test_pipeline_color_formats_follow_their_targets():
    unexplained = []
    for number, literal, context in literal_assignments(SOURCE.read_text()):
        if not any(literal == fixed and phrase in context for fixed, phrase, _ in FIXED):
            unexplained.append(f"gpu_metal.m:{number}: {literal}")
    assert not unexplained, ("a pipeline with a fixed color format: take the format from the target it draws into "
                             "(quad_pipeline(fragment, target.pixelFormat)), or list it in FIXED with why: "
                             f"{unexplained}")


def test_the_check_finds_a_literal_format():
    sample = "x;\ny;\nz;\n\tdescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;\n"
    assert [(number, literal) for number, literal, _ in literal_assignments(sample)] == [(4, "MTLPixelFormatBGRA8Unorm")]


def test_the_companion_picture_follows_the_phone_layer():
    text = SOURCE.read_text()
    start = text.index("static void companion_present(")
    body = text[start:text.index("\n}\n", start)]
    assert "quad_pipeline(companion_function, drawable.texture.pixelFormat)" in body
