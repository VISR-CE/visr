"""debug.encode_thread's queue hands the backend the same commands, bytes and order as a direct call."""
import unittest

from c_probe import run_probe, sanitizer_launcher, sanitizer_links


class GpuQueueTests(unittest.TestCase):
    def test_queued_commands_hash_as_direct_ones(self):
        run_probe(self, ["port/ios/tests/gpu_queue_probe.c", "port/ios/host/gpu_call_hash.c",
                         "port/ios/host/gpu_queue.c", "port/ios/host/gpu_owner_check.c"])

    def test_encode_thread_under_thread_sanitizer(self):
        """The same probe under ThreadSanitizer: the threaded run, its barriers and the wraps that end and restart the
        encode thread share nothing without an order."""
        if not sanitizer_links("thread"):
            self.skipTest("ThreadSanitizer does not build or run here")
        run_probe(self, ["port/ios/tests/gpu_queue_probe.c", "port/ios/host/gpu_call_hash.c",
                         "port/ios/host/gpu_queue.c", "port/ios/host/gpu_owner_check.c"], extra=["-fsanitize=thread"],
                  launcher=sanitizer_launcher())

    def test_second_eye_replay_makes_the_span_again(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="gpu-queue-replay-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-pthread",
                "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/linux/src"),
                str(ROOT / "port/ios/tests/gpu_queue_replay_probe.c"), str(ROOT / "port/ios/host/gpu_queue.c"),
                "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
