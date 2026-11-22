"""debug.encode_thread's queue hands the backend the same commands, bytes and order as a direct call."""
import unittest

from c_probe import run_probe


class GpuQueueTests(unittest.TestCase):
    def test_queued_commands_hash_as_direct_ones(self):
        run_probe(self, ["port/ios/tests/gpu_queue_probe.c", "port/ios/host/gpu_call_hash.c",
                         "port/ios/host/gpu_queue.c", "port/ios/host/gpu_owner_check.c"])


if __name__ == "__main__":
    unittest.main()
