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


SOURCES = ("*.c", "*.m", "*.mm", "*.h")


def call_sites(name: str):
    """Every call of NAME in the host's sources, outside comments, whatever encloses it (a C function, an Objective-C
    method, a block): (file, line) pairs. Declarations and the definition (a line starting with a return type) are
    left out."""
    found = []
    for pattern in SOURCES:
        for path in sorted((ROOT / "port/ios").rglob(pattern)):
            if "tests" in path.parts:
                continue
            text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), path.read_text(errors="replace"), flags=re.S)
            for number, line in enumerate(text.splitlines(), 1):
                code = line.split("//")[0]
                if re.search(rf"\b{name}\(", code) and not re.match(rf"\s*(?:static\s+)?\w+\s+\*?{name}\(", code):
                    found.append((path.name, number))
    return found


def functions(source: str):
    """(name, body) for each top-level C function definition: the readable half of the checks. It does not see
    Objective-C methods or blocks, so the call-site counts above are what catch a second writer."""
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
    # one call of each anywhere in the app's sources: a second writer in any function, method or block fails here
    assert len(call_sites("host_theater_set_frame_repeat")) == 1, call_sites("host_theater_set_frame_repeat")
    assert len(call_sites("cp_layer_renderer_set_minimum_frame_repeat_count")) == 1, \
        call_sites("cp_layer_renderer_set_minimum_frame_repeat_count")
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
    """`display.steady_cadence` is log only. What guards that is the literal 0 for the lock in `frame_repeat_in`, the
    only caller of the owner's decision; the checks on the trigger's functions (found by "cadence" in their names)
    are the readable half."""
    source = (HOST / "host_stereo.m").read_text()
    bodies = dict(functions(source))
    assert re.search(r"stereo_repeat_wanted\(mode, forced, 0\)", bodies["frame_repeat_in"])
    trigger = [name for name in bodies if "cadence" in name]
    assert "host_stereo_cadence_frame" in trigger and "host_stereo_cadence_report" in trigger
    for name in trigger:
        assert "host_stereo_repeat_refresh(" not in bodies[name], name
        assert "frame_repeat_live" not in bodies[name], name
