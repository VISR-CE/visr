#!/usr/bin/env python3
"""The level's stencil mark has one seam (bug class: each feature that reads the level's pixels grew its own mark
request and its own copy of the stencil state, three times over, so a change to the bit or its meaning had to be
found and made in every copy).

- The game asks once, through `halo_level_pixels_mark`; no per-feature `*_lightmaps_mark` is visible to it.
- The bits are named in `halo_level_stencil.h`: no Metal stencil state, reference or shader test of the level's
  pixels spells the bit as a number.
- Every Metal depth-stencil state that compares against the level's mark is made by `level_stencil_state`.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
METAL = ROOT / "port/ios/host/gpu_metal.m"
DEVICE = ROOT / "port/linux/src/d3d8_device.c"
FIXUPS = ROOT / "port/linux/include/halo_linux_source_fixups.h"
GAME = ROOT / "source"


def code(text):
    """text without C comments (string literals stay: Metal shader source is in them)"""
    return re.sub(r"/\*.*?\*/", " ", text, flags=re.S)


class LevelStencilSeam(unittest.TestCase):
    def test_the_game_asks_once(self):
        callers = []
        for path in GAME.rglob("*.c"):
            text = code(path.read_text(errors="replace"))
            if re.search(r"\bhalo_\w+_lightmaps_mark\s*\(", text):
                callers.append(str(path.relative_to(ROOT)))
        self.assertEqual(callers, [], "the game calls a feature's own mark request; call halo_level_pixels_mark")
        self.assertIn("int halo_level_pixels_mark(int begin);", FIXUPS.read_text())
        self.assertNotRegex(code(FIXUPS.read_text()), r"\bhalo_\w+_lightmaps_mark\b")

    def test_every_reader_is_asked(self):
        text = code(DEVICE.read_text())
        body = re.search(r"int halo_level_pixels_mark\(int begin\)\s*\{(.*?)\n\}", text, re.S)
        self.assertIsNotNone(body)
        readers = set(re.findall(r"static int (\w+_lightmaps_mark)\(int begin\)", text))
        self.assertTrue(readers)
        self.assertEqual(readers, set(re.findall(r"(\w+_lightmaps_mark)\(begin\)", body.group(1))))
        # (no short circuit: each reader remembers its own answer, so each must be asked)
        self.assertNotRegex(body.group(1), r"_lightmaps_mark\(begin\)\s*(\|\||&&)")

    def test_bits_are_named(self):
        text = code(METAL.read_text())
        self.assertNotRegex(text, r"setStencilReferenceValue:\s*0x", "a stencil reference spelled as a number")
        self.assertNotRegex(text, r"\breadMask\s*=\s*0x(?!ff\b)", "a stencil read mask spelled as a number")
        self.assertNotRegex(text, r"&\s*0x10u", "a shader tests the level's bit as a number")

    def test_one_state_builder(self):
        text = code(METAL.read_text())
        builder = re.search(r"static id<MTLDepthStencilState> level_stencil_state\(.*?\n\}", text, re.S)
        self.assertIsNotNone(builder)
        rest = text[:builder.start()] + text[builder.end():]
        # outside the builder, an equality stencil test is only ever a level test through it
        self.assertNotIn("MTLCompareFunctionEqual", rest.replace("depthCompareFunction = MTLCompareFunctionEqual", ""),
                         "a stencil equality state made outside level_stencil_state")


if __name__ == "__main__":
    unittest.main()
