"""audio.fast_filter: the fast resampling filter matches the reference within its bound, and mono is exact."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class AudioFilterTests(unittest.TestCase):
    def test_fast_filter_matches_reference_within_bound(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="audio-filter-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            for flags in (["-O2"], ["-O0", "-fsanitize=address,undefined"]):
                subprocess.run([
                    compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
                    "-I", str(ROOT / "port/linux/src"),
                    str(ROOT / "port/ios/tests/audio_filter_probe.c"), "-o", str(executable), "-lm",
                ], check=True)
                subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
