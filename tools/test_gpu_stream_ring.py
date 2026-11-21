"""The producer's stream ring never reuses memory the GPU may still read (port/ios/host/gpu_stream_ring.h)."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class GpuStreamRingTests(unittest.TestCase):
    def test_streams_survive_until_the_gpu_completes_them(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="gpu-stream-ring-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-pthread",
                "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/linux/src"),
                str(ROOT / "port/ios/tests/gpu_stream_ring_probe.c"), str(ROOT / "port/ios/host/gpu_stream_ring.c"),
                "-fsanitize=address,undefined", "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
