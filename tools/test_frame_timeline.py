"""The frame timeline's late-join ring and latency line (port/runtime/frame_timeline.c): a late shown time joins only
its own row, and the line counts unshown frames and prints unknown fields as "-"."""
import pathlib
import shutil
import subprocess
import sys
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

    def test_host_row_waits(self):
        """With the encode thread the row's encoder tail is the barrier's wait less the present's display wait."""
        compiler = shutil.which("clang")
        if not compiler or sys.platform != "darwin":
            self.skipTest("clang on macOS is required (host_frame_timeline.c reads the Mach clock)")
        with tempfile.TemporaryDirectory(prefix="host-frame-timeline-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-pthread",
                "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/linux/src"), "-I", str(ROOT / "port/runtime/include"),
                str(ROOT / "port/ios/tests/host_frame_timeline_probe.c"), str(ROOT / "port/ios/host/host_frame_timeline.c"),
                str(ROOT / "port/runtime/frame_timeline.c"), "-lm", "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
