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
        run_probe(self, ["port/ios/tests/gpu_queue_replay_probe.c", "port/ios/host/gpu_call_hash.c",
                         "port/ios/host/gpu_queue.c", "port/ios/host/gpu_owner_check.c"])


if __name__ == "__main__":
    unittest.main()
