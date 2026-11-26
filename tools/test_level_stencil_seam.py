#!/usr/bin/env python3
"""The level's stencil mark has one seam (bug class: each feature that reads the level's pixels grew its own mark
request and its own copy of the stencil state, three times over, so a change to the bit or its meaning had to be
found and made in every copy).

- The game asks once, through `halo_level_pixels_mark`; no per-feature mark request is visible to it, and every
  mark request the port defines is a static reader the seam asks.
- The bits are named in `halo_level_stencil.h`: no Metal stencil state, reference or shader test of the level's
  pixels, in any host file, spells the bit as a number (hex or decimal, with or without a `u`).
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
PORT_C = [ROOT / "port/linux/src", ROOT / "port/linux/game"]
HOST = [ROOT / "port/ios", ROOT / "port/macos", ROOT / "port/shared"]
HOST_SUFFIXES = {".m", ".mm", ".c", ".h", ".metal"}


def host_files():
    for top in HOST:
        for path in sorted(top.rglob("*")):
            if path.suffix in HOST_SUFFIXES and path.is_file():
                yield path


def string_literals(text):
    return re.findall(r'"((?:[^"\\\n]|\\.)*)"', text)


def code(text):
    """text without C comments (string literals stay: Metal shader source is in them)"""
    return re.sub(r"/\*.*?\*/", " ", text, flags=re.S)


class LevelStencilSeam(unittest.TestCase):
    def test_the_game_asks_once(self):
        callers = []
        for path in GAME.rglob("*.c"):
            text = code(path.read_text(errors="replace"))
            # (any halo_*_mark but the one seam: a feature's own request under another name is the same bug)
            if re.search(r"\bhalo_(?!level_pixels_mark\b)\w+_mark\s*\(", text):
                callers.append(str(path.relative_to(ROOT)))
        self.assertEqual(callers, [], "the game calls a feature's own mark request; call halo_level_pixels_mark")
        self.assertIn("int halo_level_pixels_mark(int begin);", FIXUPS.read_text())
        self.assertNotRegex(code(FIXUPS.read_text()), r"\bhalo_(?!level_pixels_mark\b)\w+_mark\b")

    def test_every_reader_is_asked(self):
        text = code(DEVICE.read_text())
        body = re.search(r"int halo_level_pixels_mark\(int begin\)\s*\{(.*?)\n\}", text, re.S)
        self.assertIsNotNone(body)
        asked = set(re.findall(r"(\w+_mark)\(begin\)", body.group(1)))
        self.assertTrue(asked)
        # every mark request the port defines, under any name and in any of its files, is a reader the seam asks
        readers = {}
        for top in PORT_C:
            for path in sorted(top.rglob("*.c")):
                for linkage, name in re.findall(r"^(static )?int (\w+_mark)\(int \w+\)\s*\{",
                                                code(path.read_text(errors="replace")), re.M):
                    if name != "halo_level_pixels_mark":
                        readers[name] = (str(path.relative_to(ROOT)), linkage)
        self.assertEqual(set(readers), asked, "a mark request the seam does not ask, or asks one that is gone")
        for name, (where, linkage) in readers.items():
            self.assertEqual((where, linkage), ("port/linux/src/d3d8_device.c", "static "),
                             f"{name} must be a static reader beside the seam")
        # (no short circuit: each reader remembers its own answer, so each must be asked)
        self.assertNotRegex(body.group(1), r"_mark\(begin\)\s*(\|\||&&)")

    def assert_spelled_by_name(self, pattern, text, message):
        found = re.search(pattern, text)
        self.assertIsNone(found, f"{message}: {found.group(0) if found else ''}")

    def test_bits_are_named(self):
        for path in host_files():
            text = code(path.read_text(errors="replace"))
            where = str(path.relative_to(ROOT))
            # (zero, the reset, is not a bit)
            self.assert_spelled_by_name(r"setStencilReferenceValue:\s*(0x0*[1-9a-fA-F][0-9a-fA-F]*|[1-9][0-9]*)\b",
                                        text, f"{where}: a stencil reference spelled as a number")
            self.assert_spelled_by_name(r"\breadMask\s*=\s*(?!0xff\b|255\b)(0x[0-9a-fA-F]+|[0-9]+)\b", text,
                                        f"{where}: a stencil read mask spelled as a number")
            for literal in string_literals(text):
                self.assert_spelled_by_name(r"&\s*(0x0*1[01]|1[67])[uU]?\b", literal,
                                            f"{where}: a shader tests the level's bit as a number")

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
