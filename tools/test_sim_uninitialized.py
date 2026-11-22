"""Every local opted out of automatic zeroing (SIM_UNINITIALIZED) is one a person checked.

The native builds clear every stack local the code leaves uninitialized
(configure.py --guest-auto-init), so a read of unwritten bytes gives the same
value on every run. SIM_UNINITIALIZED (port/linux/include/halo_sim_uninitialized.h)
takes a large local on a hot path out of that; each one was read by hand and is
written before it is read. A new use must be checked the same way and added to
REVIEWED, and a removed one taken out, so the list always matches the source.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "port/linux/include/halo_sim_uninitialized.h"

# (file, local) for each reviewed opt-out
REVIEWED = {
    ("source/physics/collisions.c", "features"),  # collision_fix_pill
    ("source/physics/collisions.c", "features"),  # collision_move_pill
    ("source/physics/collisions.c", "result"),  # collision_get_features_in_sphere
    ("source/physics/collisions.c", "sphere_result"),  # collision_test_sphere
    ("source/units/bipeds.c", "features"),  # biped_update_physics
    ("source/structures/structures.c", "projected_points"),  # sphere_intersects_cluster_portal
    ("source/objects/objects.c", "cannot_interpolate_node_orientations_storage"),  # object_compute_node_matrices
    ("source/objects/objects.c", "temporary_object_indices"),  # objects_in_sphere
    ("source/render/render_objects.c", "name"),  # render_object_list
}
REVIEWED_COUNT = 9

USE = re.compile(r"^\s*SIM_UNINITIALIZED\s+[^;=]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;", re.M)


def uses():
    found = []
    for root in ("source", "port"):
        for path in sorted((ROOT / root).rglob("*.[ch]")):
            if path == HEADER:
                continue
            text = path.read_text(encoding="latin-1")
            if "SIM_UNINITIALIZED" not in text:
                continue
            names = USE.findall(text)
            assert len(names) == text.count("SIM_UNINITIALIZED"), f"{path}: a SIM_UNINITIALIZED that is not a plain local declaration"
            found += [(str(path.relative_to(ROOT)), name) for name in names]
    return found


def test_every_opt_out_is_reviewed():
    found = uses()
    assert set(found) == REVIEWED, f"unreviewed: {sorted(set(found) - REVIEWED)}; gone: {sorted(REVIEWED - set(found))}"
    assert len(found) == REVIEWED_COUNT


def test_each_opt_out_says_why():
    for path in {p for p, _ in uses()}:
        lines = (ROOT / path).read_text(encoding="latin-1").splitlines()
        for index, line in enumerate(lines):
            if "SIM_UNINITIALIZED" in line:
                assert "port:" in lines[index - 1] or "*/" in lines[index - 1], f"{path}:{index + 1} needs a comment above it"


def test_pattern_build_empties_the_macro():
    text = HEADER.read_text()
    assert re.search(r"HALO_AUTO_INIT_PATTERN[^\n]*\n#define SIM_UNINITIALIZED\n", text)
    linux_build = (ROOT / "tools/linux_build.py").read_text()
    assert '"-DHALO_AUTO_INIT_PATTERN"' in linux_build
