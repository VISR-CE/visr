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
    enum { BITS = LIGHT_VISIBILITY_FAMILY_LAST - LIGHT_VISIBILITY_FAMILY_FIRST + 1 };
    long roots[BITS], root;
    double importance[BITS];
    int count = 0, bit;
    assert(light_visibility_family_bit(roots, count, -1) == 0);
    assert(light_visibility_family_bit(roots, count, 70) == 0);
    light_visibility_families_offer(roots, importance, &count, BITS, 70, 2.0);
    light_visibility_families_offer(roots, importance, &count, BITS, 71, 1.0);
    light_visibility_families_offer(roots, importance, &count, BITS, -1, 9.0);
    assert(light_visibility_family_bit(roots, count, 70) == LIGHT_VISIBILITY_FAMILY_FIRST);
    assert(light_visibility_family_bit(roots, count, 71) == LIGHT_VISIBILITY_FAMILY_FIRST + 1);
    for (root = 100; root < 200; root++)
        light_visibility_families_offer(roots, importance, &count, BITS, root, 0.5);
    assert(count == BITS);
    for (root = 100; root < 200; root++)
    {
        bit = light_visibility_family_bit(roots, count, root);
        assert(bit == 0 || (bit >= LIGHT_VISIBILITY_FAMILY_FIRST && bit <= LIGHT_VISIBILITY_FAMILY_LAST));
    }
    assert(light_visibility_family_bit(roots, count, 71) == LIGHT_VISIBILITY_FAMILY_FIRST + 1);
    ''')
    shader = (ROOT / "port/linux/src/nv2a_psh.c").read_text()
    assert "uint caster_mask = ~%uu & ~(family >= %uu && family <= %uu ? (1u << family) : 0u);" in shader
    assert "traced_blockers, caster_mask)" in shader
    host = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    assert "instances[index].mask = index ? masks[index] : (uint32_t)TRACED_SHADOWS_MASK_LEVEL;" in host
    game = (ROOT / "port/linux/game/traced_shadow_casters.c").read_text()
    assert "record->family = (uint32_t)light_visibility_family(traced.object_index);" in game


def test_a_handle_with_the_high_salt_bit_still_gets_a_family(tmp_path):
    """The class: treating a negative object handle as no object. The guest's long is 32 bits, so a handle whose salt
    has its high bit set (the player's biped in the Library replay was e4a9023a) is negative, and only NONE (-1) means
    none. A sign test there gave every family bit 0, so a player's own body blocked his flashlight and muzzle flash."""
    _run_rule_program(tmp_path, r'''
    enum { BITS = LIGHT_VISIBILITY_FAMILY_LAST - LIGHT_VISIBILITY_FAMILY_FIRST + 1 };
    long roots[BITS];
    double importance[BITS];
    int count = 0;
    long player = (long)(int)0xe4a9023au, rider = (long)(int)0x8001000cu;
    assert(player < 0 && rider < 0);
    light_visibility_families_offer(roots, importance, &count, BITS, player, 2.0);
    light_visibility_families_offer(roots, importance, &count, BITS, rider, 1.0);
    assert(light_visibility_family_bit(roots, count, player) == LIGHT_VISIBILITY_FAMILY_FIRST);
    assert(light_visibility_family_bit(roots, count, rider) == LIGHT_VISIBILITY_FAMILY_FIRST + 1);
    assert(light_visibility_family_bit(roots, count, -1) == 0);
    ''')
    for path in ("port/linux/game/light_visibility.h", "source/objects/object_lights.c",
                 "port/linux/game/traced_shadow_casters.c"):
        text = (ROOT / path).read_text()
        for name in ("root", "object_index", "owner_index"):
            assert f"if ({name} < 0)" not in text, path


def test_a_caster_sent_before_its_owners_light_gets_the_lights_bit(tmp_path):
    """The class: a family bit that depends on which side asks first. Split screen reuses window 0's caster records
    in window 1, so a caster's record can go out before its owner's light is chosen; had the light assigned the bit,
    the record kept 0 while the light's keep-clear shrank to a centimeter, and the owner blocked his own light. The
    frame's table is made from every light's owner before anything asks, and both sides only look it up."""
    _run_rule_program(tmp_path, r'''
    enum { BITS = LIGHT_VISIBILITY_FAMILY_LAST - LIGHT_VISIBILITY_FAMILY_FIRST + 1 };
    long roots[BITS];
    double importance[BITS];
    long owners[3] = { (long)(int)0xe4a9023au, -1, 1001 };
    double weights[3] = { 2.0, 9.0, 1.0 };
    int count = 0, i, caster, light;
    for (i = 0; i < 3; i++)
        light_visibility_families_offer(roots, importance, &count, BITS, owners[i], weights[i]);
    assert(count == 2);
    caster = light_visibility_family_bit(roots, count, owners[0]);
    light = light_visibility_family_bit(roots, count, owners[0]);
    assert(caster == LIGHT_VISIBILITY_FAMILY_FIRST && light == caster);
    assert(light_visibility_family_bit(roots, count, 1001) == LIGHT_VISIBILITY_FAMILY_FIRST + 1);
    assert(light_visibility_family_bit(roots, count, 1039) == 0 && count == 2);
    ''')
    lights = (ROOT / "source/objects/object_lights.c").read_text()
    assert lights.count("light_visibility_family_bit(") == 1
    assert "light_visibility_family_root(object_index));" in lights
    assert "light_visibility_families_offer(light_visibility_families.roots" in lights


def test_more_light_families_than_bits_never_let_an_owner_block_his_own_light(tmp_path):
    """The class: an owner blocking his own light once a fight has more families than bits. A table held for each
    root's life filled up during one campaign fight; the Flood shooter's muzzle flash got no bit and fell back to a
    0.3-unit owner distance, so his own body, 0.54 units from the flash, shadowed the wall behind him while a Flood
    0.30 away was skipped (c20 frame 5460). Bits go to the most important families each frame, never two to one bit;
    a family past the last has none, and its light is kept clear by its root's bounding sphere instead, which no
    caster blocks it inside, while other families' casters still do."""
    _run_rule_program(tmp_path, r'''
    enum { BITS = LIGHT_VISIBILITY_FAMILY_LAST - LIGHT_VISIBILITY_FAMILY_FIRST + 1, FAMILIES = 300 };
    long roots[BITS];
    double importance[BITS];
    int count = 0, i, bit, seen[64] = { 0 };
    /* more families than the old table's cap of 256 lights, each twice (a unit and its weapon's light, the second
    less important), the most important offered last */
    for (i = 0; i < FAMILIES; i++)
    {
        light_visibility_families_offer(roots, importance, &count, BITS, 3000 + i, 1.0 + i);
        light_visibility_families_offer(roots, importance, &count, BITS, 3000 + i, 0.5);
    }
    assert(count == BITS);
    for (i = 0; i < FAMILIES; i++)
    {
        bit = light_visibility_family_bit(roots, count, 3000 + i);
        if (i >= FAMILIES - BITS)
        {
            assert(bit == LIGHT_VISIBILITY_FAMILY_FIRST + (FAMILIES - 1 - i) && !seen[bit]);
            seen[bit] = 1;
        }
        else
            assert(bit == 0);
    }
    /* a family's best light decides its place, in any order; one dropped for better ones comes back only with a
    better light */
    count = 0;
    light_visibility_families_offer(roots, importance, &count, 1, 7, 1.0);
    light_visibility_families_offer(roots, importance, &count, 1, 8, 2.0);
    light_visibility_families_offer(roots, importance, &count, 1, 7, 1.5);
    assert(count == 1 && roots[0] == 8);
    light_visibility_families_offer(roots, importance, &count, 1, 7, 3.0);
    assert(count == 1 && roots[0] == 7);
    ''')
    shader = (ROOT / "port/linux/src/nv2a_psh.c").read_text()
    assert "caster_end = min(caster_end, max(-half_b - sqrt(inside), 0.0));" in shader
    assert "metal::raytracing::ray caster_ray(p, direction, traced_light.ray_start + nudge, caster_end);" in shader
    game = (ROOT / "port/linux/game/traced_shadow_casters.c").read_text()
    assert "light.keep_clear = family || owner_sphere ? light.ray_start" in game
    assert "if (!family && owner_sphere)" in game
    lights = (ROOT / "source/objects/object_lights.c").read_text()
    assert "light_visibility_pending.owner_sphere[3] = owner->object.bounding_sphere_radius;" in lights
    # the c20 frame 5460 case: the flash 0.54 from the shooter's center (radius 0.44), the other Flood at 0.30 and
    # 0.64: a ray from the wall behind the shooter stops where it enters his sphere, so he does not block it; a ray
    # past the other Flood is untouched
    flash = (16.95, 20.06, -4.41)
    shooter, other = ((17.32, 19.71, -4.59), 0.44), ((17.53, 20.26, -4.60), 0.40)

    def caster_end(point, sphere):
        span = sum((a - b) ** 2 for a, b in zip(flash, point)) ** 0.5
        direction = [(a - b) / span for a, b in zip(flash, point)]
        from_owner = [a - b for a, b in zip(point, sphere[0])]
        half_b = sum(a * b for a, b in zip(from_owner, direction))
        inside = half_b * half_b - (sum(a * a for a in from_owner) - sphere[1] ** 2)
        return span, (min(span, max(-half_b - inside ** 0.5, 0.0)) if inside > 0 else span)

    wall = tuple(f + 3.0 * (s - f) for f, s in zip(flash, shooter[0]))  # behind the shooter, from the flash
    span, end = caster_end(wall, shooter)
    assert end < span - 2.0 * 0.44 + 1e-6  # the ray stops before it reaches the shooter's body
    floor = tuple(f + 3.0 * (o - f) for f, o in zip(flash, other[0]))  # behind the other Flood
    span, end = caster_end(floor, shooter)
    assert end == span  # the shooter's sphere does not shorten it: the other Flood still blocks


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
    assert shader.count("traced_blockers, %uu)") == 1 and "traced_blockers, %uu)" in emitter
    assert shader.count("light_visibility_emit(&text,") >= 1
    for rule in ("bool black = max(max(result.r, result.g), result.b) < %.9g;",
                 "bool facing = dot(n, to_light) > 0.0;",
                 "n = dot(n, float3(traced_light.camera) - p) < 0.0 ? -n : n;",
                 "!black && facing"):
        assert rule in emitter
    # (the normal comes from derivatives taken before any branch: every pixel of the quad still runs there)
    assert shader.index("cross(dfdx(traced_point), dfdy(traced_point))") < shader.index("light_visibility_emit(&text,")
    game = (ROOT / "port/linux/game/traced_shadow_casters.c").read_text()
    assert "light.stop_short = (float)(0.02f * radius > 0.05f ? 0.02f * radius : 0.05f);" in game
    assert "light.stop_short = carried > light.stop_short ? carried : light.stop_short;" in game


def test_the_light_record_is_read_and_written_by_field_name():
    """The class: a light record indexed by bare float numbers in three files (the game writes it, the host sets two
    fields, the shader reads it), where one shifted index reads another field. One field list in
    traced_shadows_format.h makes the C struct and the shader's; no side counts floats."""
    header = (ROOT / "port/linux/src/traced_shadows_format.h").read_text()
    assert "TRACED_SHADOWS_LIGHT_FIELDS(TRACED_SHADOWS_LIGHT_C_FIELD)" in header
    assert "TRACED_SHADOWS_LIGHT_FIELDS(TRACED_SHADOWS_LIGHT_MSL_FIELD)" in header
    assert "sizeof(struct traced_shadows_light) == TRACED_SHADOWS_LIGHT_FLOATS * 4" in header
    for path in ("port/linux/src/nv2a_psh.c", "port/linux/src/nv2a_msl.c"):
        assert not re.search(r"traced_light\[\d", (ROOT / path).read_text()), path
    assert "constant traced_shadows_light &traced_light [[buffer(8)]]" in (ROOT / "port/linux/src/nv2a_msl.c").read_text()
    game = (ROOT / "port/linux/game/traced_shadow_casters.c").read_text()
    assert not re.search(r"\blight\[\d", game)
    host = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    bind = host[host.index("static void traced_light_bind(void)"):]
    assert not re.search(r"\blight\[\d", bind[:bind.index("\n}\n")])


def test_the_visibility_function_compiles_for_metal(tmp_path):
    """The class: shader text that only fails when a device first draws a traced light."""
    if not shutil.which("xcrun") or subprocess.run(["xcrun", "--sdk", "macosx", "--find", "metal"], capture_output=True).returncode:
        pytest.skip("no Metal compiler")
    shader = (ROOT / "port/linux/src/nv2a_psh.c").read_text()

    def literals(chunk):
        return "".join(ast.literal_eval(m) for m in re.findall(r'"(?:[^"\\]|\\.)*"', chunk))

    emitter = shader[shader.index("static void light_visibility_emit"):]
    emitter = emitter[:emitter.index("point, normal, bias, LIGHT_VISIBILITY_BLACK")]
    body = literals(emitter[emitter.index("xgpu_text_append(text,"):]) % (
        "traced_point", "traced_normal", "traced_bias", 1.0 / 512.0, 2, 2, 7, 2)
    first = shader.index('"\\tbool traced_has_coordinate')
    stages = literals(shader[first:shader.index(");", first)]) % (0, 2)
    at = shader.index("float3 traced_point = ")
    before = shader[shader.rindex("xgpu_text_append(&text,", 0, at):]
    before = literals(before[:before.index('");') + 1])
    # (the light record's shader struct, as the header makes it)
    program = tmp_path / "record.c"
    # (and the shared depth decode every MSL shader carries, nv2a_msl.c)
    program.write_text('#include <stdio.h>\n#include "traced_shadows_format.h"\n#include "depth_decode.h"\n'
                       'int main(void) { fputs(TRACED_SHADOWS_LIGHT_MSL DEPTH_VIEW_DISTANCE_MSL, stdout); return 0; }\n')
    compiler = shutil.which("cc") or shutil.which("clang")
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "port/linux/src"),
                    str(program), "-o", str(tmp_path / "record")], check=True)
    record = subprocess.run([str(tmp_path / "record")], capture_output=True, text=True, check=True).stdout
    source = tmp_path / "light.metal"
    source.write_text(
        "#include <metal_stdlib>\nusing namespace metal;\n#include <metal_raytracing>\ntypedef float4 vec4;\n"
        + record +
        "struct FragmentIn { float4 xT1 [[user(xT1)]]; float4 xT2 [[user(xT2)]]; };\n"
        "fragment float4 fragment_main(FragmentIn in [[stage_in]],\n"
        " metal::raytracing::instance_acceleration_structure traced_blockers [[buffer(6)]],\n"
        " metal::raytracing::primitive_acceleration_structure traced_world [[buffer(7)]],\n"
        " constant traced_shadows_light &traced_light [[buffer(8)]], float4 light_position [[position]],\n"
        " constant rasterization_rate_map_data &rate_map [[buffer(1)]])\n{\n\tfloat4 xT1 = in.xT1, xT2 = in.xT2;\n"
        + stages + before + "\tfloat4 result = xT2;\n" + body + "\tresult.rgb *= traced_lit;\n\treturn result;\n}\n")
    subprocess.run(["xcrun", "--sdk", "macosx", "metal", "-c", str(source), "-o", str(tmp_path / "light.air")],
                   check=True)


def test_highlights_are_blocked_where_the_diffuse_light_is():
    """The class: a highlight left on a shiny floor behind the wall that stops the light. The specular point pass
    takes the light's visibility like the diffuse pass, with its own attenuation stage and radius; the spot pass,
    which has no attenuation coordinate, is drawn as the game draws it until its depth path lands."""
    environment = (ROOT / "source/rasterizer/xbox/rasterizer_xbox_environment.c").read_text()
    assert "light_visibility_begin(&light->position, light->radius, LIGHT_VISIBILITY_STAGE_DIFFUSE, light->radius);" in environment
    assert ("light_visibility_begin(&light->position, light->radius, LIGHT_VISIBILITY_STAGE_SPECULAR,\n"
            "\t\t\t\tlight->definition->geometry.specular_radius_multiplier * light->radius);") in environment
    end = environment[environment.index("void _rasterizer_environment_specular_light_end("):]
    assert "light_visibility_end();" in end[:end.index("\n}\n")]
    lights = (ROOT / "source/objects/object_lights.c").read_text()
    assert lights.count("light_visibility_for_slot(light_index, &path, &strength);") == 2
    shader = (ROOT / "port/linux/src/nv2a_psh.c").read_text()
    assert "(traced_diffuse ? xT2.xyz : xT1.xyz)" in shader


def test_a_depth_point_decodes_with_the_shared_decode():
    """The class: a depth decoded with another convention than the one that stored it (near and far derived as if
    GL-style halved every distance, so the spot pass's rays started inside the floor). The depth point uses
    depth_decode.h's decode and row, whose numbers tools/test_depth_decode.py checks; no hand-written copy here."""
    shader = (ROOT / "port/linux/src/nv2a_psh.c").read_text()
    assert "float light_z = depth_view_distance(light_position.z, traced_light.depth_p22, traced_light.depth_p32);" in shader
    game = (ROOT / "port/linux/game/traced_shadow_casters.c").read_text()
    assert "depth_row_from_view(frustum->z_near, frustum->z_far, projection[2][2], projection[3][2]," in game
    assert "depth_p32 / (" not in shader and "z_far - frustum->z_near" not in game


def test_a_pass_without_an_attenuation_coordinate_rebuilds_its_point_from_depth():
    """The class: a point rebuilt in the wrong space (a foveated eye's physical pixels, eye 0's camera in eye 1's
    replay, a view rectangle from another target). The spot pass's point comes from depth like ambient occlusion's,
    through the pass's rate map; the second eye's replay gets a light record with eye 1's camera; and
    debug.traced_light_depth_point runs the same path in every pass, so stills prove it matches the attenuation
    point."""
    environment = (ROOT / "source/rasterizer/xbox/rasterizer_xbox_environment.c").read_text()
    assert "light_visibility_begin(&light->position, light->radius, LIGHT_VISIBILITY_STAGE_NONE, radius);" in environment
    shader = (ROOT / "port/linux/src/nv2a_psh.c").read_text()
    assert "light_decoder.map_physical_to_screen_coordinates(light_position.xy)" in shader
    assert "bool traced_from_depth = !(traced_has_coordinate && traced_light.depth_point_mode < 0.5);" in shader
    assert "float3 traced_point = traced_from_depth ? traced_depth_point : traced_attenuation_point;" in shader
    # (a rebuilt point lies within a fraction of its distance of its surface: its ray starts that far off it, along
    # the normal turned toward the camera, so the surface under it never blocks it; an exact point is not moved)
    assert "float traced_bias = traced_from_depth ? traced_light.depth_bias * light_z : 0.0;" in shader
    emitter = shader[shader.index("static void light_visibility_emit"):]
    emitter = emitter[:emitter.index("\n}\n")]
    assert emitter.index("n = dot(n, float3(traced_light.camera) - p) < 0.0 ? -n : n;") < emitter.index("p += n_length > 0.0")
    assert emitter.index("p += n_length > 0.0") < emitter.index("float3 to_light = float3(traced_light.position) - p;")
    game = (ROOT / "port/linux/game/traced_shadow_casters.c").read_text()
    replay = game[game.index("if (rasterizer_other_eye_draws())"):]
    replay = replay[:replay.index("halo_stereo_replay_eye(HALO_STEREO_REPLAY_BOTH);")]
    assert replay.index("HALO_STEREO_REPLAY_EYE_1") < replay.index("traced_light_view(&light);")
    assert "global_window_parameters.camera.position.x" in game
    host = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    assert "light.foveated = pass_rate_map_data ? 1.0f : 0.0f;" in host


def test_every_instance_mask_bit_is_one_the_gpu_matches():
    """The class: a mask bit the API accepts and the GPU ignores. MTLAccelerationStructureInstanceDescriptor.mask is 32
    bits wide, but only the low 8 are matched: casters given family bits 24-26 were never hit by any ray, so the Flood
    lost every shadow (c20 frame 5460). Every instance mask the traced lights use stays within bits 0-7."""
    header = (ROOT / "port/linux/src/traced_shadows_format.h").read_text()
    values = {name: int(value) for name, value in re.findall(r"TRACED_SHADOWS_(\w+) = (\d+)u?,", header)}
    assert values["MASK_NO_FAMILY"] < 256 and values["MASK_LEVEL"] < 256
    assert values["FAMILY_FIRST"] >= 2 and values["FAMILY_LAST"] <= 7
