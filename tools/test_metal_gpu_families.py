"""Metal features that older Apple GPUs do not have stay out of the app, or behind a check of the GPU family.

The bug class: a Metal call or shader function that needs a newer GPU family than the oldest device VISR runs on (the
A8 Apple TV HD, Apple GPU family 2). Non-uniform threadgroups (`dispatchThreads:`) need family 4 (A11): on the A10X
iPad the soft particle pass aborted the app with 'APINotSupported' at a map's first frame. SIMD-group functions in a
shader need family 4, 6 or 7 depending on the kind.

- Compute work is dispatched in whole threadgroups (`metal_dispatch_grid` in `port/ios/host/metal_compute_grid.h`),
  never with `dispatchThreads:` or `dispatchThreadsPerTile:`.
- Every kernel that reads its grid position returns, or writes nothing, for a position outside its size before its
  first write, so the threads a whole threadgroup adds past the grid's edge change nothing.
- A source file whose shaders use SIMD-group or quad-group functions asks the GPU-family gate
  (`port/ios/host/metal_gate.h`) for a pass whose floor is a family that has them (or a later one).
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
        if "port/ios/tests" in path.as_posix():
            continue  # probes compare against dispatchThreads: on the Mac; they never ship
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


# Metal Feature Set Tables: the earliest Apple GPU family with each kind of SIMD-group function
SIMD_FAMILIES = (
    (4, r"\bquad_(shuffle\w*|broadcast\w*)\s*\("),
    (6, r"\bsimd_(shuffle\w*|broadcast\w*)\s*\("),
    (7, r"\bsimd_(sum|product|min|max|and|or|xor|prefix_\w+|ballot|all|any)\s*\("),
)


def gate_pass_floors():
    """Each METAL_PASSES row's floor family, from metal_gate.h."""
    import test_metal_gate

    return {row[0]: int(row[3]) for row in test_metal_gate.macro_rows("METAL_PASSES")}


def gated_family(source, floors):
    """The highest floor among the gate passes source asks for (`metal_pass(METAL_PASS_X)`)."""
    return max((floors[name] for name in re.findall(r"\bmetal_pass\(\s*METAL_PASS_(\w+)\s*\)", source)), default=0)


def test_simd_group_functions_check_the_gpu_family():
    floors = gate_pass_floors()
    missing = []
    for path in SOURCES:
        source = path.read_text(errors="replace")
        text = shader_text(source)
        shaders = "".join(body for _, _, body in kernels(text))
        shaders += "".join(re.findall(r"\b(?:fragment|vertex)\s+\w[\s\S]*?\n\}", text))
        needed = max((family for family, pattern in SIMD_FAMILIES if re.search(pattern, shaders)), default=0)
        if not needed:
            continue
        if gated_family(source, floors) < needed:
            missing.append(f"{path.relative_to(ROOT)}: needs a metal_pass() whose floor is Apple{needed} or later")
    assert not missing, "\n".join(missing)


def test_scan_finds_the_known_simd_shaders():
    # the scan itself works: the SSAO gather (permutes) and the stereo HUD depth (reductions) are found and gated
    floors = gate_pass_floors()
    for name, family in (("port/ios/host/gpu_metal.m", 6), ("port/ios/host/host_stereo.m", 7)):
        source = (ROOT / name).read_text()
        shaders = "".join(body for _, _, body in kernels(shader_text(source)))
        needed = max(f for f, pattern in SIMD_FAMILIES if re.search(pattern, shaders))
        assert needed == family, (name, needed)


# Every shader that calls a SIMD-group function, by file and function (a generated shader by the C function that writes
# it), and the gate pass whose floor covers it: the pipeline is made only when metal_pass() of that gate passes. A new
# use fails until it is listed here with its gate.
SIMD_USES = {
    ("port/ios/host/gpu_metal.m", "ssao_gather"): "AMBIENT_OCCLUSION",
    ("port/ios/host/host_stereo.m", "hud_depth_reduce"): "STEREO_HUD_DEPTH",
    # (the apply draws' darkened bounds; the traced_shadow key is set only for a frame traced_shadows_supported allows)
    ("port/linux/src/nv2a_psh.c", "nv2a_pixel_shader_translate()"): "TRACED_SHADOWS",
}
SIMD_SOURCES = SOURCES + sorted((ROOT / "port/linux/src").glob("*.[ch]"))


def simd_uses():
    """{(file, shader function): highest family needed} for every SIMD-group call in a shader string: the function is
    the nearest `kernel`/`fragment`/`vertex` declaration above it in the strings, else the C function writing it."""
    pattern = "|".join(p for _, p in SIMD_FAMILIES)
    uses = {}
    for path in SIMD_SOURCES:
        lines = without_comments(path.read_text(errors="replace")).splitlines()
        for index, line in enumerate(lines):
            if '"' not in line or not re.search(pattern, line.split('"', 1)[1]):
                continue
            name = None
            for back in range(index, -1, -1):
                above = lines[back]
                shader = re.search(r"\b(?:kernel|fragment|vertex)\s+[\w<>:]+\s+(\w+)\s*\(", above)
                if shader and '"' in above:
                    name = shader.group(1)
                    break
                function = re.match(r"^(?:static\s+)?[\w\s\*]+?\b(\w+)\s*\(", above)
                if function and not above.startswith((" ", "\t", "#", '"')):
                    name = function.group(1) + "()"
                    break
            family = max(f for f, p in SIMD_FAMILIES if re.search(p, line))
            key = (str(path.relative_to(ROOT)), name)
            uses[key] = max(uses.get(key, 0), family)
    return uses


def test_each_simd_shader_is_under_a_gate_that_covers_it():
    """bug class: the check only asked that some gate in a SIMD file covered the family, so a SIMD shader made
    outside its gate (or a new one in a file with another high gate) passed. Each use names its gate here."""
    floors = gate_pass_floors()
    asked = "".join(path.read_text(errors="replace") for path in SOURCES)
    uses = simd_uses()
    assert ("port/ios/host/gpu_metal.m", "ssao_gather") in uses, "the scan no longer finds the known uses"
    problems = []
    for key, family in sorted(uses.items()):
        gate = SIMD_USES.get(key)
        if gate is None:
            problems.append(f"{key}: a SIMD-group function (Apple{family}) with no gate listed in SIMD_USES")
        elif floors[gate] < family:
            problems.append(f"{key}: needs Apple{family}, its gate {gate} starts at Apple{floors[gate]}")
        elif not re.search(rf"\bmetal_pass\(\s*METAL_PASS_{gate}\s*\)", asked):
            problems.append(f"{key}: its gate {gate} is never asked (metal_pass)")
    stale = sorted(set(SIMD_USES) - set(uses))
    assert not problems and not stale, "\n".join(problems + [f"{key}: listed but no longer found" for key in stale])


if __name__ == "__main__":
    test_no_nonuniform_threadgroups()
    test_grid_helper_dispatches_whole_threadgroups()
    test_grid_kernels_return_outside_their_size()
    test_simd_group_functions_check_the_gpu_family()
    test_scan_finds_the_known_simd_shaders()
    test_each_simd_shader_is_under_a_gate_that_covers_it()
    print("metal gpu families: ok")
