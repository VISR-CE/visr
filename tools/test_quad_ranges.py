"""A batched quad draw's dense upload reads the vertex bytes and colors the game's own draws read."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class QuadRangesTests(unittest.TestCase):
    def test_dense_layout_reads_what_the_game_draws_read(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="quad-ranges-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1",
                str(ROOT / "port/ios/tests/quad_ranges_probe.c"), "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)

    def test_the_draw_uses_the_tested_layout(self):
        source = (ROOT / "port/linux/src/d3d8_device.c").read_text()
        start = source.index("void halo_draw_quad_ranges(")
        body = source[start:source.index("\n}\n", start)]
        for call in ("quad_ranges_vertex_count(", "quad_ranges_dense(", "quad_ranges_indices(", "quad_ranges_colors(",
                     "quad_ranges_mismatches("):
            self.assertIn(call, body)
        # the layouts live in quad_ranges.h alone: no second copy of the index or color loops in the draw
        self.assertNotIn("/ 255.0f", body)
        self.assertIn("quad_ranges_gather(", source)


if __name__ == "__main__":
    unittest.main()
