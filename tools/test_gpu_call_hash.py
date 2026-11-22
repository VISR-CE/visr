"""debug.gpu_call_hash changes a frame's hash for a dropped, reordered or stale command and not for queries."""
import unittest

from c_probe import run_probe


class GpuCallHashTests(unittest.TestCase):
    def test_hash_catches_dropped_reordered_and_stale_commands(self):
        run_probe(self, ["port/ios/tests/gpu_call_hash_probe.c"])


if __name__ == "__main__":
    unittest.main()
