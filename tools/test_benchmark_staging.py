"""The guest's closed staging parser and atomic scenario validation."""
import json
import os
import subprocess
from pathlib import Path

import pytest

from test_benchmark_catalog import STAGING, INVALID_STAGINGS
from test_replay_control import function

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def staging_probe(tmp_path_factory):
    folder = tmp_path_factory.mktemp("staging-probe")
    source = folder / "probe.c"
    source.write_text(r'''
#include "benchmark_staging.h"
#include <stdio.h>
#include <strings.h>
#define _strnicmp strncasecmp
#define TAG_STRING_LENGTH 31
#define NONE -1
static int calls;
struct tag_block { int count; void *elements; };
struct scenario { struct tag_block structure_bsp_references, cutscene_flags, ai_encounters; };
struct scenario_cutscene_flag { char name[32]; };
struct encounter_definition { char name[33]; };
#define TAG_BLOCK_GET_ELEMENT(block, index, type) (&((type *)(block)->elements)[index])
/* ENCOUNTER_LOOKUP */
/* PRODUCTION_LOOKUP */
static void apply(void *context, int kind, int a, int b) {
    (void)context; (void)kind; (void)a; (void)b; calls++;
}
int main(int argc, char **argv) {
    struct benchmark_staging staging;
    char error[96];
    struct scenario_cutscene_flag flags[] = {{"Hangar"}};
    struct encounter_definition encounters[] = {{"Covenant"}, {"MARINES"}};
    struct scenario scenario = {{3, NULL}, {1, flags}, {2, encounters}};
    if (argc != 2 || !benchmark_staging_parse(argv[1], &staging)) return 1;
    if (!benchmark_staging_apply(&staging, staging_lookup, apply, &scenario, error, sizeof(error))) {
        printf("%d:%s", calls, error); return 2;
    }
    printf("%d:%s", calls, staging.json);
    return 0;
}
'''.replace('/* ENCOUNTER_LOOKUP */', function('source/ai/ai_scenario_definitions.c', 'scenario_get_encounter_by_name')).replace('/* PRODUCTION_LOOKUP */', function('port/linux/game/input_replay_scenario.c', 'staging_lookup')))
    binary = folder / "probe"
    result = subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                             "-I", str(ROOT / "port/runtime"), str(source), "-o", str(binary)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return binary


def run_staging(binary, staging):
    return subprocess.run([str(binary), json.dumps(staging)], capture_output=True, text=True)


def test_staging_parser_canonicalizes_and_applies(staging_probe):
    result = run_staging(staging_probe, STAGING)
    assert result.returncode == 0, result.stderr
    assert result.stdout == "6:" + json.dumps(STAGING, separators=(",", ":"), sort_keys=True)


@pytest.mark.parametrize("staging", INVALID_STAGINGS)
def test_staging_parser_refuses_invalid_shapes(staging_probe, staging):
    assert run_staging(staging_probe, staging).returncode == 1


@pytest.mark.parametrize("step,missing", [({"teleport_player": "missing"}, "missing"),
    ({"ai_place": "missing"}, "missing"), ({"ai_see_player": "missing"}, "missing"),
    ({"ai_see": ["covenant", "missing"]}, "missing"), ({"switch_bsp": 3}, "3")])
def test_staging_validates_every_reference_before_applying(staging_probe, step, missing):
    result = run_staging(staging_probe, [{"deathless": True}, step])
    assert result.returncode == 2
    assert result.stdout.startswith("0:") and missing in result.stdout


@pytest.mark.parametrize("text", ['[{"deathless":t r u e}]', '[{"switch_bsp":1 2}]',
                                 '[{"switch_bsp":01}]', '[{"ai_place":"a"}]junk'])
def test_staging_parser_refuses_broken_json(staging_probe, text):
    assert subprocess.run([str(staging_probe), text], capture_output=True).returncode == 1


@pytest.mark.parametrize("imported", [False, True])
def test_recording_and_replay_staging_tick_and_runtime_gate(tmp_path, imported):
    binary = tmp_path / "input-replay-probe"
    command = ["cc", "-O2", "-fsanitize=address,undefined", "-I", str(ROOT / "port/linux/src")]
    if imported:
        command.append("-DHALO_ILP32=1")
    subprocess.run([*command, str(ROOT / "port/ios/tests/input_replay_probe.c"), "-o", str(binary)], check=True)
    # The same executable must accept both host answers without recompilation.
    for value in (None, "0", "1", "true", "01"):
        environment = dict(os.environ)
        environment.pop("HALO_DEVELOPMENT_HOOKS", None)
        if value is not None:
            environment["HALO_DEVELOPMENT_HOOKS"] = value
        environment["PROBE_HOST_HOOKS"] = "0" if value == "1" else "1"
        environment["PROBE_EXPECT_HOOKS"] = environment["PROBE_HOST_HOOKS"] if imported else ("1" if value == "1" else "0")
        result = subprocess.run([str(binary)], capture_output=True, text=True, cwd=tmp_path, env=environment)
        assert result.returncode == 0, result.stderr
        assert "staged recording and replay: PASS" in result.stdout


def test_staged_route_and_plan_signature(tmp_path):
    source = tmp_path / "plan.c"
    source.write_text(r'''
#include "benchmark_plan.h"
#include <assert.h>
int main(void) {
    struct benchmark_scene scene = {.id="battle", .map="a50", .difficulty="hard"};
    struct benchmark_plan plan = {.scenes=&scene, .scene_count=1};
    struct benchmark_route route;
    unsigned long baseline = benchmark_plan_signature(&plan), staged;
    long tick, segment;
    assert(benchmark_staging_parse("[{\"deathless\":true}]", &scene.staging));
    scene.staging_tick = 31;
    staged = benchmark_plan_signature(&plan);
    assert(staged != baseline);
    scene.staging_segment++;
    assert(staged != benchmark_plan_signature(&plan));
    scene.staging_segment--;
    scene.staging_tick++;
    assert(staged != benchmark_plan_signature(&plan));
    assert(benchmark_scene_from_route("map = a50\ndifficulty = hard\nstaging = [{\"deathless\":true}]\nstaging_tick = 31\nstaging_segment = 2\n", "battle", &route));
    assert(route.scene.staging_tick == 31 && route.scene.staging.count == 1 && route.scene.staging_segment == 2);
    assert(!benchmark_staging_route("staging = [{\"deathless\":true}]\n", &scene.staging, &tick, &segment));
    assert(!benchmark_staging_route("staging_tick = 31\n", &scene.staging, &tick, &segment));
    assert(benchmark_staging_tick("2147483647", &tick) && tick == 2147483647L);
    assert(!benchmark_staging_tick("2147483648", &tick));
    assert(!benchmark_staging_tick("9999999999", &tick));
    return 0;
}
''')
    binary = tmp_path / "plan"
    subprocess.run(["cc", "-fsanitize=address,undefined", "-I", str(ROOT / "port/runtime"), str(source),
                    str(ROOT / "port/runtime/benchmark_plan.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


@pytest.mark.parametrize("reason", ["malformed-staging", "missing-staging-name", "missing-auto-staging-name"])
def test_recording_rejects_staging_failure_immediately(tmp_path, reason):
    binary = tmp_path / "recording-probe"
    subprocess.run(["cc", "-O2", "-I", str(ROOT / "port/linux/src"),
                    str(ROOT / "port/ios/tests/input_replay_probe.c"), "-o", str(binary)], check=True)
    result = subprocess.run([str(binary), "--" + reason], cwd=tmp_path, capture_output=True, text=True,
                            env={**os.environ, "HALO_DEVELOPMENT_HOOKS": "1"})
    assert result.returncode == 1, result.stderr


def test_deathless_staging_restores_previous_value(tmp_path):
    source = tmp_path / "deathless.c"
    implementation = "\n".join(function("port/linux/game/input_replay_scenario.c", name)
                                  for name in ("input_replay_staging_restore", "staging_apply"))
    source.write_text(r'''
#include "benchmark_staging.h"
#include <assert.h>
#define TRUE 1
#define FALSE 0
#define NONE -1
typedef int BOOL;
static BOOL staging_deathless_owned, staging_deathless_previous;
static struct { BOOL deathless_player; } cheat;
struct player_datum { int unit_index; };
static struct player_datum *player_try_and_get(int i) { (void)i; return 0; }
static int local_player_get_player_index(int i) { return i; }
static void scenario_switch_structure_bsp(short i) { (void)i; }
static void hs_object_teleport(int i, short j) { (void)i; (void)j; }
static void ai_scripting_place(int i) { (void)i; }
static void ai_scripting_magically_see_encounter(int i, int j) { (void)i; (void)j; }
static void ai_scripting_magically_see_players(int i) { (void)i; }
static void platform_log(const char *format, ...) { (void)format; }
/* IMPLEMENTATION */
int main(void) {
    for (int previous = 0; previous <= 1; previous++) {
        cheat.deathless_player = previous;
        staging_apply(0, BENCHMARK_STAGING_DEATHLESS, 0, 0);
        staging_apply(0, BENCHMARK_STAGING_DEATHLESS, 0, 0);
        assert(cheat.deathless_player == 1);
        input_replay_staging_restore();
        assert(cheat.deathless_player == previous);
        input_replay_staging_restore();
        assert(cheat.deathless_player == previous);
    }
}
'''.replace("/* IMPLEMENTATION */", implementation))
    binary = tmp_path / "deathless"
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-I", str(ROOT / "port/runtime"), str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


@pytest.mark.parametrize("development", [False, True])
def test_apple_host_development_hooks_match_target_policy(tmp_path, development):
    source = tmp_path / "host-hooks.c"
    source.write_text(function("port/ios/host/host_benchmark.m", "host_development_hooks") +
                      "\nint main(void) { return host_development_hooks(); }\n")
    binary = tmp_path / "host-hooks"
    command = ["cc", "-Wall", "-Wextra", "-Werror"]
    if development:
        command.append("-DHALO_DEVELOPMENT_HOOKS=1")
    subprocess.run([*command, str(source), "-o", str(binary)], check=True)
    assert subprocess.run([str(binary)]).returncode == int(development)
