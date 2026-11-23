"""Every local opted out of automatic zeroing (SIM_UNINITIALIZED) is one a person checked.

The native builds clear every stack local the code leaves uninitialized
(configure.py --guest-auto-init), so a read of unwritten bytes gives the same
value on every run. SIM_UNINITIALIZED (port/linux/include/halo_sim_uninitialized.h)
takes a large local on a hot path out of that; each one was read by hand and is
written before it is read, and its comment names the measurement that justified it.
A new use must be checked the same way and added to REVIEWED, and a removed one
taken out, so the list always matches the source.
"""

import re
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "port/linux/include/halo_sim_uninitialized.h"
sys.path.insert(0, str(ROOT))

from tools.linux_build import auto_init_flags  # noqa: E402
from tools.windows_build import check_auto_init  # noqa: E402

# (file, function, local) for each reviewed opt-out
REVIEWED = {
    ("source/physics/collisions.c", "collision_fix_pill", "features"),
    ("source/physics/collisions.c", "collision_move_pill", "features"),
    ("source/physics/collisions.c", "collision_get_features_in_sphere", "result"),
    ("source/physics/collisions.c", "collision_test_sphere", "sphere_result"),
    ("source/units/bipeds.c", "biped_update_physics", "features"),
    ("source/structures/structures.c", "sphere_intersects_cluster_portal", "projected_points"),
    ("source/objects/objects.c", "object_compute_node_matrices", "cannot_interpolate_node_orientations_storage"),
    ("source/objects/objects.c", "objects_in_sphere", "temporary_object_indices"),
    ("source/render/render_objects.c", "render_object_list", "name"),
    ("source/objects/objects.c", "objects_garbage_collection", "garbage_object_indices"),
    ("source/game/players.c", "players_update_before_game", "actions"),
    ("source/models/models.c", "render_model", "relative_node_matrices"),
    ("source/structures/structure_render.c", "structure_render_shadow", "surface_indices"),
    ("port/linux/src/d3d8_device.c", "prepare_draw", "key"),
}

USE = re.compile(r"^\s*SIM_UNINITIALIZED\s+[^;=]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;")
# a function definition's first line: starts at column 0 and names the function before its "("
DEFINITION = re.compile(r"^[A-Za-z_][^;]*?\b(\w+)\s*\(")
# the measurement an opt-out comment names: "Measured:" and a figure
MEASURED = re.compile(r"Measured:[^*]*\d(?:\.\d+)?%")


def enclosing_function(lines, index):
    """the function whose body holds lines[index]: the definition above the nearest "{" in column 0"""
    while not lines[index].startswith("{"):
        index -= 1
    index -= 1
    while not DEFINITION.match(lines[index]):
        index -= 1
    return DEFINITION.match(lines[index]).group(1)


def uses():
    """(file, function, local, line index, lines) for every SIM_UNINITIALIZED in the source"""
    found = []
    for root in ("source", "port"):
        for path in sorted((ROOT / root).rglob("*.[ch]")):
            if path == HEADER:
                continue
            text = path.read_text(encoding="latin-1")
            if "SIM_UNINITIALIZED" not in text:
                continue
            lines = text.splitlines()
            for index, line in enumerate(lines):
                if "SIM_UNINITIALIZED" not in line:
                    continue
                match = USE.match(line)
                assert match, f"{path}:{index + 1}: a SIM_UNINITIALIZED that is not a plain local declaration"
                found.append((str(path.relative_to(ROOT)), enclosing_function(lines, index), match.group(1), index, lines))
    return found


def test_every_opt_out_is_reviewed():
    found = [(path, function, local) for path, function, local, _, _ in uses()]
    assert len(found) == len(set(found)), f"the same local opted out twice in one function: {sorted(found)}"
    assert set(found) == REVIEWED, f"unreviewed: {sorted(set(found) - REVIEWED)}; gone: {sorted(REVIEWED - set(found))}"


def test_each_opt_out_names_its_measurement():
    for path, _, local, index, lines in uses():
        above = index - 1
        while "SIM_UNINITIALIZED" in lines[above]:  # one comment may cover adjacent opt-outs
            above -= 1
        start = above
        while "/*" not in lines[start]:
            start -= 1
        comment = " ".join(lines[start:above + 1])
        assert lines[above].rstrip().endswith("*/"), f"{path}:{index + 1} ({local}) needs a comment above it"
        assert MEASURED.search(comment), f"{path}:{index + 1} ({local}): the comment must name its measurement (Measured: ... N%)"


@pytest.mark.parametrize("mode, flags", [
    ("off", []),
    ("zero", ["-ftrivial-auto-var-init=zero"]),
    ("pattern", ["-ftrivial-auto-var-init=pattern", "-DHALO_AUTO_INIT_PATTERN"]),
])
def test_auto_init_flags(mode, flags):
    assert auto_init_flags(SimpleNamespace(port_auto_init=mode)) == flags


def test_off_is_the_default():
    assert auto_init_flags(SimpleNamespace()) == []


def test_pattern_build_empties_the_macro():
    assert re.search(r"HALO_AUTO_INIT_PATTERN[^\n]*\n#define SIM_UNINITIALIZED\n", HEADER.read_text())


def test_windows_build_refuses_auto_init():
    check_auto_init(SimpleNamespace(port_auto_init="off"))
    for mode in ("zero", "pattern"):
        with pytest.raises(SystemExit, match="Windows build does not apply it"):
            check_auto_init(SimpleNamespace(port_auto_init=mode))
