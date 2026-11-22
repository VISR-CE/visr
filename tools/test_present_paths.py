"""Every present path in `host_gpu_dispatch.c` goes through `present_frame`, which takes each frame through the same
steps around the backend's present: a stereo present that had its own copy skipped the timeline seal (no latency lines
for side-by-side or a Vision Pro), the encode thread's `present_called` and the frame-deadline end."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
DISPATCH = ROOT / "port/ios/host/host_gpu_dispatch.c"
OPERATIONS = ROOT / "port/ios/host/gpu_operation.h"
# present_frame's steps, in order
STEPS = (
    "present_called = called;",
    "gpu_metal_frame_deadline_end();",
    "host_lifecycle_hold(encode_queue_on ? gpu_queue_drain : NULL);",
    "host_perf_hooks_present_begin();",
    "wrapped_backend->stream_frame_end();",
    "next_frame_due = make(frame);",
    "host_frame_timeline_barrier(gpu_queue_barrier_waited());",
    "host_frame_timeline_seal();",
    "host_perf_hooks_present_end();",
    "data_updates_presented();",
    "low_power_frame();",
    "host_thread_cpu_frame();",
    "host_main_sync_log_frame();",
    "host_frame_counter_show(presented++);",
)


def function_bodies(source):
    """Each top-level static function's name and body."""
    for match in re.finditer(r"^static [^\n(]*?\b(\w+)\([^\n]*\)\n\{\n(.*?)^\}", source, re.MULTILINE | re.DOTALL):
        yield match.group(1), match.group(2)


class PresentPathsTests(unittest.TestCase):
    def setUp(self):
        self.source = DISPATCH.read_text()
        self.bodies = dict(function_bodies(self.source))

    def test_every_backend_present_has_a_wrapper_through_present_frame(self):
        operations = set(re.findall(r"#define GPU_OPERATION_CLASS_(present\w*) ", OPERATIONS.read_text()))
        self.assertGreaterEqual(operations, {"present", "present_stereo"})
        for operation in operations:
            with self.subTest(operation=operation):
                wrapper = re.search(rf"wrapped\.{operation} = (\w+);", self.source)
                self.assertIsNotNone(wrapper, f"the dispatch does not wrap {operation}")
                body = self.bodies[wrapper.group(1)].strip()
                self.assertRegex(body, rf"^return present_frame\(make_{operation}, [^;]+\);$")

    def test_only_the_make_functions_call_a_backend_present(self):
        for name, body in self.bodies.items():
            if re.search(r"wrapped_backend->present\w*\(", body):
                with self.subTest(function=name):
                    self.assertTrue(name.startswith("make_present"), f"{name} calls a backend present itself")
                    self.assertRegex(body.strip(), r"^return wrapped_backend->present\w*\([^;]*\);$")

    def test_present_frame_takes_every_step_in_order_and_alone(self):
        body = self.bodies["present_frame"]
        positions = [body.find(step) for step in STEPS]
        for step, position in zip(STEPS, positions):
            with self.subTest(step=step):
                self.assertGreaterEqual(position, 0, f"present_frame skips {step}")
                self.assertEqual(self.source.count(step), 1, f"{step} is made outside present_frame too")
        self.assertEqual(positions, sorted(positions), "present_frame's steps are out of order")
        # (no step may depend on the kind of frame: that is the stereo bug written the other way)
        self.assertNotRegex(body, r"\bmake_present\w*", "present_frame treats one kind of present differently")


if __name__ == "__main__":
    unittest.main()
