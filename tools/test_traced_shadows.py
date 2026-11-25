"""Traced object shadows without Metal: the game's records, skinning and the caster cache
(port/ios/host/traced_shadows.h), compiled and run from port/ios/tests/traced_shadows_probe.c."""
import ast
import pathlib
import re
import shutil
import subprocess
import tempfile

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def test_records_skinning_and_caster_cache():
    compiler = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
    if not compiler:
        pytest.skip("no C compiler")
    with tempfile.TemporaryDirectory() as temp:
        executable = pathlib.Path(temp) / "probe"
        subprocess.run([
            compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1",
            "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/linux/src"),
            str(ROOT / "port/ios/tests/traced_shadows_probe.c"), "-lm", "--output", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)


TRACED_KEYS = ("display.traced_shadows", "display.traced_lights")


@pytest.mark.parametrize("key", TRACED_KEYS)
def test_every_traced_setting_is_a_profile_choice_that_only_enhanced_keeps(key):
    """The class: a setting that traces rays is off unless a device profile turns it on (only GPUs with ray tracing
    hardware should), a profile may set it apart for the variable workload, and Classic and Hardcore turn it off
    whatever the profile says. A traced setting missing from one of the three tables would trace where it must not."""
    import re
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    entry = re.search(r'\{ "%s", _config_(boolean|integer), "([^"]*)"' % re.escape(key), config)
    assert entry and entry.group(2) in ("false", "0"), key
    keys = (ROOT / "port/linux/src/port_profile_keys.h").read_text()
    assert re.search(r'^PROFILE_(BOOLEAN|INTEGER)\("%s"' % re.escape(key), keys, re.M), key
    assert 'PROFILE_VARIABLE("%s")' % key in keys, key
    modes = (ROOT / "port/linux/src/port_graphics_modes.h").read_text()
    row = re.search(r'X\("%s", "([^"]*)", "([^"]*)"\)' % re.escape(key), modes)
    assert row and set(row.groups()) <= {"false", "0"}, key


def test_no_other_setting_traces_outside_the_class():
    """Every display.traced_* setting in the settings table is in TRACED_KEYS, so a new one is checked above."""
    import re
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    assert set(re.findall(r'\{ "(display\.traced_[a-z_]+)"', config)) == set(TRACED_KEYS)


def _run_rule_program(tmp_path, body):
    compiler = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
    if not compiler:
        pytest.skip("no C compiler")
    source = tmp_path / "rule.c"
    source.write_text('#include <assert.h>\n#include "light_visibility.h"\nint main(void) {\n' + body + "\nreturn 0;\n}\n")
    executable = tmp_path / "rule"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "port/linux/game"),
                    str(source), "--output", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


OWNER_TYPES = {"bipd": 0, "vehi": 1, "weap": 2, "eqip": 3, "garb": 4, "proj": 5,
               "scen": 6, "mach": 7, "ctrl": 8, "lifi": 9, "plac": 10, "ssce": 11}


def test_stock_lights_follow_the_owner_rule(tmp_path):
    """The class: a designer-placed light going dark. Every dynamic light on the stock maps, by the object type that
    can own it (tools/traced_light_stock.tsv, from tools/traced_light_census.py), goes through the C rule: scenery
    and device lights are never traced, unit, weapon, item and projectile lights are, and so are effect lights made
    on no object (owner groups that are not object types: damage, materials, globals, scripted effects at a point)."""
    rows = [line.split("\t") for line in (ROOT / "tools/traced_light_stock.tsv").read_text().splitlines()[1:]]
    assert len(rows) > 50 and any(row[3] == "no" for row in rows) and any(row[3] == "yes" for row in rows)
    checks = []
    for light, kind, group, traced, _maps in rows:
        owner = OWNER_TYPES.get(group, -1)
        expected = 0 if traced == "no" else 1
        assert (owner >= 6) == (traced == "no"), (light, kind, group, traced)
        checks.append("assert(light_visibility_owner_blocks(%d) == %d);" % (owner, expected))
    # the flashlights, the case the feature is for, are in the table as traced
    assert {row[0] for row in rows if row[3] == "yes"} >= {
        "characters\\cyborg\\flashlight_cyborg", "characters\\marine_armored\\flashlight_marine"}
    _run_rule_program(tmp_path, "\n".join(sorted(set(checks))))


def test_rank_puts_the_own_flashlight_first_and_holds_the_cap(tmp_path):
    """The class: the cap choosing lights by enumeration order, or a fixture taking a traced light's place."""
    _run_rule_program(tmp_path, r'''
    struct light_visibility_candidate lights[] = {
        {10, 5.0, 2, 0}, {11, 9.0, -1, 0}, {12, 1.0, 0, 1}, {13, 50.0, 9, 0}, {14, 9.0, 5, 0}};
    unsigned char chosen[5];
    light_visibility_rank(lights, 5, 2, 0, chosen);
    assert(chosen[2] && chosen[1] && !chosen[0] && !chosen[3] && !chosen[4]); /* own, then 9.0 by lower id */
    light_visibility_rank(lights, 5, 128, 0, chosen);
    assert(chosen[0] && chosen[1] && chosen[2] && !chosen[3] && chosen[4]); /* the light fixture never */
    light_visibility_rank(lights, 5, 128, 1, chosen);
    assert(chosen[3]); /* debug.traced_lights_fixtures */
    light_visibility_rank(lights, 5, 0, 0, chosen);
    assert(!chosen[0] && !chosen[1] && !chosen[2] && !chosen[3] && !chosen[4]);
    ''')


def test_new_lights_block_at_once_and_only_the_cap_eases(tmp_path):
    """The class: a short-lived light (a muzzle flash) easing in and never blocked, or a shadow switching on in one
    frame because another light went out."""
    _run_rule_program(tmp_path, r'''
    struct light_visibility_ramp ramp = {0};
    struct light_visibility_candidate a = {1, 4.0, 2, 0}, b = {2, 1.0, 2, 0}, pair[2];
    unsigned char chosen[2];
    float strength[2];
    pair[0] = a; pair[1] = b;
    /* both new: the chosen one is at full strength on its first frame, the one past the cap at none */
    chosen[0] = 1; chosen[1] = 0;
    light_visibility_ramp_step(&ramp, pair, chosen, 2, 1000, strength);
    assert(strength[0] == 1.f && strength[1] == 0.f);
    /* the cap swaps them: 30 ms later each has moved 30% toward its new place */
    chosen[0] = 0; chosen[1] = 1;
    light_visibility_ramp_step(&ramp, pair, chosen, 2, 1030, strength);
    assert(strength[0] > 0.69f && strength[0] < 0.71f && strength[1] > 0.29f && strength[1] < 0.31f);
    light_visibility_ramp_step(&ramp, pair, chosen, 2, 1200, strength);
    assert(strength[0] == 0.f && strength[1] == 1.f);
    /* a light that goes away and comes back as new is blocked at once */
    light_visibility_ramp_step(&ramp, pair + 1, chosen + 1, 1, 1216, strength);
    chosen[0] = 1;
    light_visibility_ramp_step(&ramp, pair, chosen, 2, 1232, strength);
    assert(strength[0] == 1.f && strength[1] == 1.f);
    ''')


def test_caster_rule_takes_characters_vehicles_scenery_and_devices(tmp_path):
    """The class: a light passing through what should stop it (a closed door, a Flood form in the dark) or blocked by
    what never should (the first-person weapon, a projectile carrying the light). Object types are object_types.h's."""
    _run_rule_program(tmp_path, r'''
    int type, casts[12] = {1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0};
    for (type = 0; type < 12; type++)
        assert(light_visibility_caster_type(type) == casts[type]);
    assert(light_visibility_spheres_touch(0, 0, 0, 1, 2.5f, 0, 0, 2));
    assert(!light_visibility_spheres_touch(0, 0, 0, 1, 3.5f, 0, 0, 2));
    assert(!light_visibility_spheres_touch(0, 0, 0, 0, 0, 0, 0, 0));
    ''')


def test_the_walker_is_the_only_way_a_shadowless_object_casts_for_lights():
    """The class: forcing a shadow (a map tile, a blur and an apply draw) for an object only so that it blocks a
    light. Objects without a shadow go through render_object_light_casters, and the darkness test is the game's."""
    text = (ROOT / "source/render/render_objects.c").read_text()
    assert "shadow_darkness > OBJECT_SHADOW_MINIMUM_DARKNESS)" in text
    assert "traced_light_every_caster" not in text
    assert text.count("render_object_light_casters(data);") == 2


def test_an_owner_family_never_blocks_its_own_light(tmp_path):
    """The class: a light blocked by what carries it (a partner's flashlight inside his body, a Warthog's riders in
    its headlights). Each family takes its own instance mask bit, the same in every window of the frame, and a
    family's light rays leave that bit and the level's out of the caster ray's mask."""
    _run_rule_program(tmp_path, r'''
    long roots[LIGHT_VISIBILITY_FAMILY_LAST - LIGHT_VISIBILITY_FAMILY_FIRST + 1];
    int count = 0, bit;
    long root;
    assert(light_visibility_family_bit(roots, &count, -1, 1) == 0);
    assert(light_visibility_family_bit(roots, &count, 70, 0) == 0);
    assert(light_visibility_family_bit(roots, &count, 70, 1) == LIGHT_VISIBILITY_FAMILY_FIRST);
    assert(light_visibility_family_bit(roots, &count, 71, 1) == LIGHT_VISIBILITY_FAMILY_FIRST + 1);
    assert(light_visibility_family_bit(roots, &count, 70, 0) == LIGHT_VISIBILITY_FAMILY_FIRST);
    for (root = 100; root < 200; root++)
    {
        bit = light_visibility_family_bit(roots, &count, root, 1);
        assert(bit == 0 || (bit >= LIGHT_VISIBILITY_FAMILY_FIRST && bit <= LIGHT_VISIBILITY_FAMILY_LAST));
    }
    assert(count == LIGHT_VISIBILITY_FAMILY_LAST - LIGHT_VISIBILITY_FAMILY_FIRST + 1);
    assert(light_visibility_family_bit(roots, &count, 71, 1) == LIGHT_VISIBILITY_FAMILY_FIRST + 1);
    ''')
    shader = (ROOT / "port/linux/src/nv2a_psh.c").read_text()
    assert "0xFFFFFFFDu & ~(family >= 2u && family <= 31u ? (1u << family) : 0u)" in shader
    assert "traced_blockers, caster_mask)" in shader
    host = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    assert "instances[index].mask = index ? masks[index] : 2u;" in host
    game = (ROOT / "port/linux/game/traced_shadow_casters.c").read_text()
    assert "record->family = (uint32_t)light_visibility_family(traced.object_index);" in game


def test_the_light_instances_rebuild_when_the_caster_set_changes():
    """The class: a window reusing another window's blockers because it holds as many casters (stale families or
    shadows). The rebuild is keyed on each caster's shadow, structure and family, not on their count."""
    host = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    assert "casters = traced_light_casters();" in host
    assert "casters != traced_lights.made_with" in host
    assert "casters += traced.shadow_slot" not in host


def test_every_traced_light_pass_runs_the_one_visibility_function():
    """The class: light passes that disagree (a highlight left behind a wall the diffuse light stops at) or a ray
    that blocks what should stay lit: a pixel the light leaves black, a surface facing away (its ray starts inside
    it), a light resting on what it touches. Every pass's rays are light_visibility_emit's."""
    shader = (ROOT / "port/linux/src/nv2a_psh.c").read_text()
    emitter = shader[shader.index("static void light_visibility_emit"):]
    emitter = emitter[:emitter.index("\n}\n")]
    assert shader.count("traced_blockers, 2u)") == 1 and "traced_blockers, 2u)" in emitter
    assert shader.count("light_visibility_emit(&text,") >= 1
    for rule in ("bool black = max(max(result.r, result.g), result.b) < 1.0 / 512.0;",
                 "bool facing = dot(n, to_light) > 0.0;",
                 "n = dot(n, traced_light[3].xyz - p) < 0.0 ? -n : n;",
                 "!black && facing"):
        assert rule in emitter
    # (the normal comes from derivatives taken before any branch: every pixel of the quad still runs there)
    assert shader.index("cross(dfdx(traced_point), dfdy(traced_point))") < shader.index("light_visibility_emit(&text,")
    game = (ROOT / "port/linux/game/traced_shadow_casters.c").read_text()
    assert "light[7] = (float)(0.02f * radius > 0.05f ? 0.02f * radius : 0.05f);" in game
    assert "light[7] = carried > light[7] ? carried : light[7];" in game


def test_the_visibility_function_compiles_for_metal(tmp_path):
    """The class: shader text that only fails when a device first draws a traced light."""
    if not shutil.which("xcrun") or subprocess.run(["xcrun", "--sdk", "macosx", "--find", "metal"], capture_output=True).returncode:
        pytest.skip("no Metal compiler")
    shader = (ROOT / "port/linux/src/nv2a_psh.c").read_text()

    def literals(chunk):
        return "".join(ast.literal_eval(m) for m in re.findall(r'"(?:[^"\\]|\\.)*"', chunk))

    emitter = shader[shader.index("static void light_visibility_emit"):]
    emitter = emitter[:emitter.index("point, normal);")]
    body = literals(emitter[emitter.index("xgpu_text_append(text,"):]) % ("traced_point", "traced_normal")
    at = shader.index("float3 traced_point = ")
    before = shader[shader.rindex("xgpu_text_append(&text,", 0, at):]
    before = literals(before[:before.index('");') + 1])
    source = tmp_path / "light.metal"
    source.write_text(
        "#include <metal_stdlib>\nusing namespace metal;\n#include <metal_raytracing>\ntypedef float4 vec4;\n"
        "struct FragmentIn { float4 xT2 [[user(xT2)]]; };\n"
        "fragment float4 fragment_main(FragmentIn in [[stage_in]],\n"
        " metal::raytracing::instance_acceleration_structure traced_blockers [[buffer(6)]],\n"
        " metal::raytracing::primitive_acceleration_structure traced_world [[buffer(7)]],\n"
        " constant float4 *traced_light [[buffer(8)]])\n{\n\tfloat4 xT2 = in.xT2;\n"
        + before + "\tfloat4 result = xT2;\n" + body + "\tresult.rgb *= traced_lit;\n\treturn result;\n}\n")
    subprocess.run(["xcrun", "--sdk", "macosx", "metal", "-c", str(source), "-o", str(tmp_path / "light.air")],
                   check=True)
