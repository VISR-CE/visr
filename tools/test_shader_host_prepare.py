"""The guest's SSAO and soft-depth imports send a warm-up (no values) to the warm operations alone, which compile the
programs and touch no render target, and a call with values to the operation itself (gpu_side_imports.h)."""
import unittest

from c_probe import run_probe


class SideImportTests(unittest.TestCase):
    def test_warm_ups_reach_only_the_warm_operations(self):
        run_probe(self, ["port/ios/tests/gpu_side_imports_probe.c"])


if __name__ == "__main__":
    unittest.main()
