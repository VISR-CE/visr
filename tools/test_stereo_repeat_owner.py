"""The Compositor's frame repeat count has one owner.

Bug class: two writers of one setting that disagree. The space opening, `debug.knob_cycle`'s cadence and (later) a
lock to steady 45 Hz each set the layer renderer's repeat count, and `host_stereo_frame_repeat` (the frame rate the
Low Power Mode and the performance reports use) read `display.frame_repeat` on its own, so a knob press left it
reporting the old count. Every writer goes through `host_stereo_repeat_refresh`, which decides the count with
`stereo_repeat_wanted` (`port/ios/host/host_stereo_cadence.h`, its order checked by `stereo_cadence_probe.c`), and
only `frame_repeat_apply` hands a count to the Compositor.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / "port/ios/host"


def functions(source: str):
    """(name, body) for each top-level C function definition."""
    for match in re.finditer(r"^[A-Za-z_][^\n;(]*?\b(\w+)\(([^;{]*)\)\n\{\n(.*?)^\}\n", source, re.M | re.S):
        yield match.group(1), match.group(3)


def callers(name: str):
    found = []
    for path in sorted(HOST.glob("*.[cm]")):
        for function, body in functions(path.read_text()):
            if re.search(rf"\b{name}\(", body):
                found.append((path.name, function))
    return found


def test_only_the_owner_sets_the_repeat_count():
    assert callers("host_theater_set_frame_repeat") == [("host_stereo.m", "host_stereo_repeat_refresh")]
    assert callers("cp_layer_renderer_set_minimum_frame_repeat_count") == [("host_theater.m", "frame_repeat_apply")]


def test_every_writer_and_reader_decides_through_stereo_repeat_wanted():
    source = (HOST / "host_stereo.m").read_text()
    bodies = dict(functions(source))
    # the one place display.frame_repeat is read
    assert [name for name, body in bodies.items() if '"display.frame_repeat"' in body] == ["frame_repeat_in"]
    assert "stereo_repeat_wanted(" in bodies["frame_repeat_in"]
    for writer in ("host_stereo_space_opened", "host_stereo_frame_repeat_set"):
        assert "host_stereo_repeat_refresh()" in bodies[writer], writer
    assert "frame_repeat_in(" in bodies["host_stereo_frame_repeat"]


def test_the_steady_cadence_trigger_never_changes_the_repeat_count():
    """`display.steady_cadence` is log only: the trigger's code hands nothing to the owner, and the owner passes the
    lock as never latched."""
    source = (HOST / "host_stereo.m").read_text()
    bodies = dict(functions(source))
    assert re.search(r"stereo_repeat_wanted\(mode, forced, 0\)", bodies["frame_repeat_in"])
    trigger = [name for name in bodies if "cadence" in name]
    assert "host_stereo_cadence_frame" in trigger and "host_stereo_cadence_report" in trigger
    for name in trigger:
        assert "host_stereo_repeat_refresh(" not in bodies[name], name
        assert "frame_repeat_live" not in bodies[name], name
