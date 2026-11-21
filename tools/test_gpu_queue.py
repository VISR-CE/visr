"""debug.encode_thread's queue hands the backend the same commands, bytes and order as a direct call."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class GpuQueueTests(unittest.TestCase):
    def test_queued_commands_hash_as_direct_ones(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="gpu-queue-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-pthread",
                "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/linux/src"),
                str(ROOT / "port/ios/tests/gpu_queue_probe.c"), str(ROOT / "port/ios/host/gpu_call_hash.c"),
                str(ROOT / "port/ios/host/gpu_queue.c"), "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
