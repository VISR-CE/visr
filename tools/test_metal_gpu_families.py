"""Metal features that older Apple GPUs do not have stay out of the app, or behind a check of the GPU family.

The bug class: a Metal call or shader function that needs a newer GPU family than the oldest device VISR runs on (the
A8 Apple TV HD, Apple GPU family 2). Non-uniform threadgroups (`dispatchThreads:`) need family 4 (A11): on the A10X
iPad the soft particle pass aborted the app with 'APINotSupported' at a map's first frame.

- Compute work is dispatched in whole threadgroups (`metal_dispatch_grid` in `port/ios/host/metal_compute_grid.h`),
  never with `dispatchThreads:` or `dispatchThreadsPerTile:`.
- Every kernel that reads its grid position returns, or writes nothing, for a position outside its size before its
  first write, so the threads a whole threadgroup adds past the grid's edge change nothing.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCES = sorted(
    path
    for folder in ("port/ios", "port/macos", "port/shared")
    if (ROOT / folder).is_dir()
    for path in (ROOT / folder).rglob("*")
    if path.suffix in {".m", ".mm", ".c", ".h", ".swift", ".metal"}
)
GRID_HELPER = ROOT / "port/ios/host/metal_compute_grid.h"


def shader_text(source):
    """The file with adjacent C string literals joined and their escapes undone, so a kernel kept in an Objective-C
    string reads as it would in a .metal file."""
    joined = re.sub(r'"\s*\n\s*"', "", source)
    return joined.replace("\\n", "\n").replace("\\t", "\t")


def balanced(text, start, opening, closing):
    """The text from start (at an opening character) to its matching closing character, inclusive."""
    depth = 0
    for index in range(start, len(text)):
        depth += (text[index] == opening) - (text[index] == closing)
        if depth == 0:
            return text[start : index + 1]
    raise ValueError("unbalanced " + opening + closing)


def kernels(text):
    """(name, parameters, body) for every `kernel void` in text."""
    for match in re.finditer(r"\bkernel\s+void\s+(\w+)\s*\(", text):
        parameters = balanced(text, match.end() - 1, "(", ")")
        body_start = text.index("{", match.end() - 1 + len(parameters))
        yield match.group(1), parameters, balanced(text, body_start, "{", "}")


def without_comments(source):
    """source with its C comments blanked, line numbers kept (a comment may name the call it forbids)."""
    blank = lambda match: re.sub(r"[^\n]", " ", match.group(0))
    return re.sub(r"//[^\n]*", blank, re.sub(r"/\*[\s\S]*?\*/", blank, source))


def test_no_nonuniform_threadgroups():
    calls = []
    for path in SOURCES:
        for number, line in enumerate(without_comments(path.read_text(errors="replace")).splitlines(), 1):
            if re.search(r"\bdispatchThreads(PerTile)?:", line):
                calls.append(f"{path.relative_to(ROOT)}:{number}: {line.strip()}")
    assert not calls, (
        "dispatchThreads: needs Apple GPU family 4 (A11) and aborts on A8-A10X; dispatch whole threadgroups with "
        "metal_dispatch_grid (port/ios/host/metal_compute_grid.h) and bound the kernel:\n" + "\n".join(calls)
    )


def test_grid_helper_dispatches_whole_threadgroups():
    helper = GRID_HELPER.read_text()
    for dimension in ("width", "height", "depth"):
        rounded = rf"\(grid\.{dimension} \+ group\.{dimension} - 1\) / group\.{dimension}"
        assert re.search(rounded, helper), f"metal_grid_threadgroups must round {dimension} up"
    assert "dispatchThreadgroups:groups threadsPerThreadgroup:group" in helper


def test_grid_kernels_return_outside_their_size():
    checked, unbounded = [], []
    for path in SOURCES:
        text = shader_text(path.read_text(errors="replace"))
        for name, parameters, body in kernels(text):
            position = re.search(r"(\w+)\s*\[\[\s*thread_position_in_grid\s*\]\]", parameters)
            if not position:
                continue
            checked.append(name)
            grid = re.escape(position.group(1))
            store = re.search(r"\.write\s*\(|\b\w+\s*\[[^\]]+\]\s*=(?!=)", body)
            guard = re.search(rf"\bif\s*\([^;{{]*\b{grid}\b[^;{{]*(>=|<)[^;{{]*\)", body)
            if not store:
                continue
            if not guard or guard.start() > store.start():
                unbounded.append(f"{path.relative_to(ROOT)}: {name}")
    assert checked, "no kernels found: the scan is broken"
    assert "soft_depth_linearize" in checked and "resample" in checked, checked
    assert not unbounded, (
        "these kernels write before checking their grid position against their size; a whole threadgroup runs "
        "threads past the grid's edge:\n" + "\n".join(unbounded)
    )


if __name__ == "__main__":
    test_no_nonuniform_threadgroups()
    test_grid_helper_dispatches_whole_threadgroups()
    test_grid_kernels_return_outside_their_size()
    print("metal gpu families: ok")
