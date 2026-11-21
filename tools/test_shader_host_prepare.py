"""The guest's SSAO and soft-depth imports send a warm-up (no values) to the warm operations alone, which compile the
programs and touch no render target, and a call with values to the operation itself (gpu_side_imports.h)."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class SideImportTests(unittest.TestCase):
    def test_warm_ups_reach_only_the_warm_operations(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="gpu-side-imports-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1",
                "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/linux/src"),
                str(ROOT / "port/ios/tests/gpu_side_imports_probe.c"), "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
