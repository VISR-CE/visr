"""With the encode queue on, each backend call comes from the thread its class says (gpu_owner_check.c): a call from
another thread is counted and reported once per owner."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class OwnerCheckTests(unittest.TestCase):
    def test_a_second_thread_is_reported_once_per_owner(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="gpu-owner-check-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-pthread",
                "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/linux/src"),
                str(ROOT / "port/ios/tests/gpu_owner_check_probe.c"), str(ROOT / "port/ios/host/gpu_owner_check.c"),
                "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
