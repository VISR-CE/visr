#!/usr/bin/env python3
"""Every game function that reads the eye's own camera, frustum, fog plane,
position or stereo layer, and how the second eye's replay (debug.stereo_replay)
gives eye 1 its value there.

While eye 0's pass draws for both eyes, a value computed from the eye is eye
0's unless the site computes eye 1's too: through rasterizer_other_eye_begin
and _end, rasterizer_eye_constants (a constant block built under eye 1's state, or sent: the one place that
orders the game's store before eye 1's), rasterizer_for_each_eye (the whole pass again for eye 1, its records marked for it),
or rasterizer_eye_matrices (node matrices built from the eye's position). A new reader that is not in
SITES fails the check, so a site cannot skip the mechanism unnoticed.

Kinds:
  routed       the function itself computes eye 1's value through a route
  per-eye      a helper that computes for whichever eye's state is in place;
               every caller named ("path:function", or a function of the same
               file) must call it and be routed, or be a per-eye helper too
  same         the value is the same in both eyes (why given)
  outside      the function does not run in eye 0's replayed pass
  machinery    the replay's own eye switching
  open         eye-dependent and not routed yet (what is missing given);
               verify shows its differences, and --strict fails on it

Usage: stereo_eye_sites.py [--strict] [--list]
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
FOLDERS = ("source", "port/linux/game")
READS = re.compile(
    r"global_window_parameters\.(?:camera|frustum|fog\.plane)|halo_stereo_eye_position\(\)|"
    r"halo_stereo_current_layer\(\)|halo_stereo_repeat_pass\(\)")
ROUTES = ("rasterizer_other_eye_begin(", "rasterizer_eye_matrices(", "rasterizer_for_each_eye(", "rasterizer_eye_constants(",
          "rasterizer_eye_constants_send(")
DEFINITION = re.compile(r"^(?:static\s+|extern\s+)?[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\(")

X = "source/rasterizer/xbox/"
SITES = {
    # the replay's own machinery
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_other_eye_window_begin"): ("machinery", "starts eye 1's window state"),
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_other_eye_swap"): ("machinery", "swaps eye 1's window state in and out"),
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_other_eye_begin"): ("machinery", "sets the layer and eye position to eye 1's"),
    ("source/render/render.c", "render_window"): ("machinery", "the eye loop: opens the span, sets each eye's position and layer"),
    # routed
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_window_fog_constants"): (
        "per-eye", "the fog block (-88)", ["_rasterizer_window_set_fog"]),
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_frustum_constants"): (
        "per-eye", "world-to-clip, eye position and axes (-96), the first-person weapon's eye included",
        ["rasterizer_set_frustum_z"]),
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_first_person_eye"): (
        "per-eye", "the weapon's eye, inside rasterizer_frustum_constants", ["rasterizer_frustum_constants"]),
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_first_person_offset"): (
        "per-eye", "the weapon's move, inside rasterizer_frustum_constants", ["rasterizer_frustum_constants"]),
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_first_person_split_screen"): (
        "per-eye", "mono split screen only; inside rasterizer_frustum_constants", ["rasterizer_frustum_constants"]),
    (X + "rasterizer_xbox_transparent_geometry.c", "transparent_geometry_zsprite_constants"): (
        "per-eye", "the z-sprite block (-63)", ["rasterizer_transparent_geometry_group_draw"]),
    (X + "rasterizer_xbox_transparent_geometry.c", "transparent_geometry_camera_transform"): (
        "per-eye", "register 88's camera transform", ["rasterizer_transparent_geometry_group_draw"]),
    ("source/render/render_sky.c", "render_sky_eye_matrices"): (
        "per-eye", "the sky's node matrices about the eye", ["render_sky"]),
    ("port/linux/game/stereo_far.c", "halo_stereo_far_matrices"): (
        "per-eye", "far scenery's node matrices about the eye", ["source/render/render_objects.c:render_object_list"]),
    ("source/render/render_objects.c", "render_object_list"): (
        "routed", "far scenery: rasterizer_eye_matrices with render_far_scenery_eye_matrices"),
    (X + "rasterizer_xbox_transparent_geometry.c", "rasterizer_transparent_geometry_group_draw"): (
        "routed", "registers 88-90 and the z-sprite block; its other reads are view_to_world axes (the same)"),
    # the same in both eyes
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_set_target"): ("same", "viewport bounds"),
    (X + "rasterizer_xbox_active_camouflage.c", "rasterizer_active_camouflage_cache_primary_render_target"): (
        "same", "viewport bounds"),
    (X + "rasterizer_xbox_dynavobgeom.c", "_rasterizer_psuedo_dynamic_screen_quad_draw"): ("same", "viewport bounds"),
    (X + "rasterizer_xbox_dynavobgeom.c", "_rasterizer_dynamic_unlit_geometry_draw"): (
        "same", "a sort key along the shared forward"),
    (X + "rasterizer_xbox_environment.c", "_rasterizer_environment_reflection_mirror_draw"): ("same", "forward"),
    (X + "rasterizer_xbox_environment.c", "_rasterizer_environment_reflection_draw"): ("same", "forward"),
    (X + "rasterizer_xbox_environment.c", "_rasterizer_environment_transparent_geometry_submit"): (
        "same", "sort keys along the shared forward"),
    (X + "rasterizer_xbox_models.c", "_rasterizer_model_transparent_geometry_submit"): (
        "same", "sort keys along the shared forward"),
    (X + "rasterizer_xbox_widgets.c", "_rasterizer_widget_submit"): ("same", "sort keys along the shared forward"),
    (X + "rasterizer_xbox_widgets.c", "_rasterizer_widget_begin"): ("same", "viewport bounds"),
    (X + "rasterizer_xbox_water.c", "rasterizer_water_draw"): ("same", "forward"),
    (X + "rasterizer_xbox_screen_effect.c", "rasterizer_screen_effect_set_texture_transforms"): (
        "same", "viewport and window bounds"),
    (X + "rasterizer_xbox_screen_effect.c", "_rasterizer_screen_effect"): ("same", "viewport bounds"),
    (X + "rasterizer_xbox_screen_effect.c", "_rasterizer_screen_flash"): ("same", "viewport bounds"),
    (X + "rasterizer_xbox_transparent_geometry.c", "rasterizer_transparent_geometry_groups_end"): (
        "same", "viewport and window bounds"),
    ("port/linux/game/first_person_body.c", "halo_first_person_body"): ("same", "tests the layer for an eye, not which"),
    ("port/linux/game/stereo.c", "halo_stereo_weapon_offset"): ("same", "tests the layer for an eye, not which"),
    # not in eye 0's replayed pass
    ("source/rasterizer/rasterizer.c", "rasterizer_debug_model_vertices"): ("outside", "debug drawing"),
    ("source/rasterizer/rasterizer_debug.c", "rasterizer_debug_line_shaded"): ("outside", "debug drawing"),
    ("source/rasterizer/rasterizer_debug.c", "rasterizer_debug_triangle_shaded"): ("outside", "debug drawing"),
    ("source/rasterizer/xbox/rasterizer_xbox.c", "rasterizer_secondary_render_target_debug"): ("outside", "debug drawing"),
    (X + "rasterizer_xbox_debug.c", "_rasterizer_debug_immediate_begin_screenspace"): ("outside", "debug drawing"),
    (X + "rasterizer_xbox_motion_sensor.c", "_rasterizer_hud_motion_sensor_blip_end"): ("outside", "the HUD pass"),
    (X + "rasterizer_xbox_text.c", "rasterizer_text_begin"): ("outside", "the HUD and menus"),
    ("source/interface/hud_draw.c", "hud_full_layout_apply"): ("outside", "the HUD pass"),
    ("source/interface/hud_draw.c", "hud_portrait_layout_apply"): ("outside", "the HUD pass"),
    ("source/interface/hud_draw.c", "hud_full_layout_save"): ("outside", "the HUD pass"),
    ("source/interface/hud_draw.c", "hud_full_layout_end"): ("outside", "the HUD pass"),
    ("port/linux/game/first_person_legs.c", "first_person_legs_node_matrices"): (
        "outside", "a flat view only (a stereo frame draws the first-person body instead)"),
    # eye-dependent, not routed yet
    ("port/linux/game/traced_shadow_casters.c", "traced_shadow_cover"): (
        "open", "traced shadows' covered rectangle from eye 0's camera; foveated eyes never trace, and traced shadows "
        "in stereo wait on the head-tracked stereo design"),
    ("source/rasterizer/rasterizer.c", "rasterizer_models_begin"): (
        "open", "the models' fog and fade scalars from the eye: millimeter scalars (spec: the center camera)"),
    (X + "rasterizer_xbox_models.c", "_rasterizer_model_begin"): ("open", "millimeter scalars (spec: the center camera)"),
    (X + "rasterizer_xbox_models.c", "rasterizer_model_draw_environment_shader"): (
        "open", "millimeter scalars (spec: the center camera)"),
    (X + "rasterizer_xbox_models.c", "_rasterizer_model_draw"): ("open", "millimeter scalars (spec: the center camera)"),
    (X + "rasterizer_xbox_environment_fog.c", "_rasterizer_environment_fog_begin"): (
        "open", "the fog's plane distance test: a millimeter scalar"),
    (X + "rasterizer_xbox_environment_fog.c", "rasterizer_environment_fog_screen_model_begin"): (
        "open", "fog screen: eye 1's constants"),
    (X + "rasterizer_xbox_environment_fog.c", "rasterizer_environment_fog_screen_is_active"): (
        "open", "fog screen: eye 1's gate (traversal-time state)"),
    (X + "rasterizer_xbox_environment_fog.c", "_rasterizer_environment_fog_screen_begin"): (
        "open", "fog screen: eye 1's constants and history"),
    ("source/rasterizer/rasterizer_lights.c", "lens_flare_window_index"): (
        "per-eye", "eye 1's flares take their own occlusion slot",
        ["lens_flare_submit_for_window", "lens_flares_submit_occlusion_tests_for_window", "lens_flares_draw_for_window"]),
    ("source/rasterizer/rasterizer_lights.c", "lens_flare_evaluate_corona_rotation_function"): (
        "per-eye", "the corona's rotation from this eye", ["lens_flares_draw_for_window"]),
    ("source/rasterizer/rasterizer_lights.c", "lens_flare_submit_for_window"): (
        "per-eye", "queues the flare from this eye's camera", ["rasterizer_lens_flare_submit"]),
    ("source/rasterizer/rasterizer_lights.c", "lens_flares_submit_occlusion_tests_for_window"): (
        "per-eye", "this eye's occlusion tests", ["rasterizer_lens_flares_submit_occlusion_tests"]),
    ("source/rasterizer/rasterizer_lights.c", "lens_flares_draw_for_window"): (
        "per-eye", "this eye's flares", ["rasterizer_lens_flares_draw"]),
    (X + "rasterizer_xbox_lights.c", "rasterizer_sun_glow_draw"): ("open", "sun glow: both-eye emission"),
    (X + "rasterizer_xbox_lights.c", "rasterizer_project_billboard"): ("open", "sprite3d: both-eye emission"),
    (X + "rasterizer_xbox_widgets.c", "rasterizer_widget_project_billboard"): ("open", "sprite3d: both-eye emission"),
    ("source/objects/widgets/glow.c", "glow_update"): ("open", "glow's gate (traversal-time state)"),
}


def functions(path):
    """{name: body text} of a C file's functions (a definition at column 0 to the next '}' at column 0)"""
    found = {}
    name = None
    body = []
    for line in path.read_text(errors="replace").split("\n"):
        if line and not line[0].isspace() and line[0] not in "#/*{}":
            match = DEFINITION.match(line)
            if match and not line.rstrip().endswith(";"):
                if name:
                    found[name] = found.get(name, "") + "\n".join(body)
                name, body = match.group(1), []
        if name:
            body.append(line)
    if name:
        found[name] = found.get(name, "") + "\n".join(body)
    return found


def else_after_eye_block(root):
    """problems for a port block `if (... rasterizer_other_eye_begin()) { ... }` followed by `else`: the block was
    inserted between the game's own `if` and its `else`, so the game's `else` now runs whenever the replay is off
    (the class behind a transparent group reporting a failed constant store every frame)"""
    problems = []
    for folder in FOLDERS:
        for path in sorted((root / folder).rglob("*.c")):
            text = path.read_text(errors="replace")
            for match in re.finditer(r"if\s*\([^{;]*rasterizer_other_eye_begin\(\)\s*\)\s*\{", text):
                depth, index = 1, match.end()
                while depth and index < len(text):
                    depth += (text[index] == "{") - (text[index] == "}")
                    index += 1
                if re.match(r"\s*else\b", text[index:]):
                    line = text.count("\n", 0, match.start()) + 1
                    problems.append(f"{path.relative_to(root).as_posix()}:{line}: a rasterizer_other_eye_begin block is "
                                    "followed by else, which would take the game's own if: put the block after the "
                                    "game's whole if/else")
    return problems


def check(root=ROOT, sites=SITES, strict=False):
    """problems (strings), and the open sites"""
    problems, open_sites, readers, bodies = [], [], set(), {}
    for folder in FOLDERS:
        for path in sorted((root / folder).rglob("*.c")):
            relative = path.relative_to(root).as_posix()
            for name, body in functions(path).items():
                bodies[(relative, name)] = body
                if READS.search(body):
                    readers.add((relative, name))
    problems += else_after_eye_block(root)
    for site in sorted(readers):
        if site not in sites:
            problems.append(f"{site[0]}: {site[1]} reads the eye's camera, position or layer and is not in "
                            "tools/stereo_eye_sites.py SITES: route it (rasterizer_other_eye_begin or "
                            "rasterizer_eye_matrices) or say why eye 1 needs nothing")
    for site, entry in sorted(sites.items()):
        kind = entry[0]
        if site not in readers:
            problems.append(f"{site[0]}: {site[1]} is in SITES but no longer reads the eye's state (or is gone)")
            continue
        if kind == "routed" and not any(route in bodies[site] for route in ROUTES):
            problems.append(f"{site[0]}: {site[1]} is routed but calls neither {' nor '.join(ROUTES)}")
        elif kind == "per-eye":
            for caller in entry[2]:
                key = tuple(caller.split(":", 1)) if ":" in caller else (site[0], caller)
                body = bodies.get(key)
                if body is None:
                    problems.append(f"{site[1]}'s caller {caller} is not found")
                elif site[1] not in body:
                    problems.append(f"{key[0]}: {key[1]} does not call {site[1]}")
                elif not any(route in body for route in ROUTES) and sites.get(key, ("",))[0] != "per-eye":
                    problems.append(f"{key[0]}: {key[1]} calls {site[1]} but does not route eye 1 "
                                    f"({' or '.join(ROUTES)})")
        elif kind == "open":
            open_sites.append(f"{site[0]}: {site[1]}: {entry[1]}")
        elif kind not in ("same", "outside", "machinery", "routed"):
            problems.append(f"{site[1]}: unknown kind {kind}")
    if strict:
        problems += [f"open: {line}" for line in open_sites]
    return problems, open_sites


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--strict", action="store_true", help="fail on open sites too")
    parser.add_argument("--list", action="store_true", help="list the open sites")
    args = parser.parse_args()
    problems, open_sites = check(strict=args.strict)
    for problem in problems:
        print(problem)
    if args.list:
        for line in open_sites:
            print(f"open: {line}")
    print(f"stereo eye sites: {len(SITES)} classified, {len(open_sites)} open, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
