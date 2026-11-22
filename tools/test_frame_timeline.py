"""The frame timeline's late-join ring and latency line (port/runtime/frame_timeline.c): a late shown time joins only
its own row, and the line counts unshown frames and prints unknown fields as "-"."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class FrameTimelineTests(unittest.TestCase):
    def test_ring_and_latency_line(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="frame-timeline-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-pthread",
                str(ROOT / "port/ios/tests/frame_timeline_probe.c"), str(ROOT / "port/runtime/frame_timeline.c"),
                "-lm", "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
