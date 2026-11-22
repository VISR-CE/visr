"""With the encode queue on, each backend call comes from the thread its class says (gpu_owner_check.c): a call from
another thread is counted and reported once per owner."""
import unittest

from c_probe import run_probe


class OwnerCheckTests(unittest.TestCase):
    def test_a_second_thread_is_reported_once_per_owner(self):
        run_probe(self, ["port/ios/tests/gpu_owner_check_probe.c", "port/ios/host/gpu_owner_check.c"])


if __name__ == "__main__":
    unittest.main()
