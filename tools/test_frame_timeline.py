"""The frame timeline's late-join ring and latency line (port/runtime/frame_timeline.c): a late shown time joins only
its own row, and the line counts unshown frames and prints unknown fields as "-"."""
import unittest

from c_probe import HOST_INCLUDES, run_probe


class FrameTimelineTests(unittest.TestCase):
    def test_ring_and_latency_line(self):
        run_probe(self, ["port/ios/tests/frame_timeline_probe.c", "port/runtime/frame_timeline.c"], includes=(),
                  extra=["-lm"])

    def test_host_row_waits(self):
        """With the encode thread the row's encoder tail is the barrier's wait less the present's display wait.
        host_frame_timeline.c reads the Mach clock, so this runs on macOS only."""
        run_probe(self, ["port/ios/tests/host_frame_timeline_probe.c", "port/ios/host/host_frame_timeline.c",
                         "port/runtime/frame_timeline.c"],
                  includes=(*HOST_INCLUDES, "port/runtime/include"), extra=["-lm"], darwin_only=True)

    def test_frame_waits(self):
        """A frame's waits: what leaves its CPU time and what is the row's display wait, with and without the encode
        thread, and nothing carried to the next frame."""
        run_probe(self, ["port/ios/tests/frame_waits_probe.c", "port/ios/host/frame_waits.c"], extra=["-lm"])


if __name__ == "__main__":
    unittest.main()
