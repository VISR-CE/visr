"""audio.rate_bus: the rate bus matches the per-voice path within its bound, and each mutation fails."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class AudioRateBusTests(unittest.TestCase):
    def test_bus_matches_per_voice_path_and_mutations_fail(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="audio-rate-bus-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            for flags in (["-O2"], ["-O0", "-fsanitize=address,undefined"]):
                subprocess.run([
                    compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
                    "-I", str(ROOT / "port/linux/src"),
                    str(ROOT / "port/ios/tests/audio_rate_bus_probe.c"), "-o", str(executable), "-lm",
                ], check=True)
                subprocess.run([str(executable)], check=True)
                subprocess.run([str(executable), "far"], check=True)
                for mutation in ("gain_first", "late_start", "no_ramp"):
                    result = subprocess.run([str(executable), mutation])
                    self.assertNotEqual(result.returncode, 0, f"mutation {mutation} escaped")


if __name__ == "__main__":
    unittest.main()
