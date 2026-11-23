"""The producer's stream ring never reuses memory the GPU may still read (port/ios/host/gpu_stream_ring.h)."""
import unittest

from c_probe import run_probe


class GpuStreamRingTests(unittest.TestCase):
    def test_streams_survive_until_the_gpu_completes_them(self):
        run_probe(self, ["port/ios/tests/gpu_stream_ring_probe.c", "port/ios/host/gpu_stream_ring.c"],
                  extra=["-fsanitize=address,undefined"])


if __name__ == "__main__":
    unittest.main()
