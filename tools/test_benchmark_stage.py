"""The guest's closed stage parser and atomic scenario validation."""
import json
import subprocess
from pathlib import Path

import pytest

from test_benchmark_catalog import STAGE, INVALID_STAGES
from test_replay_control import function

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def stage_probe(tmp_path_factory):
    folder = tmp_path_factory.mktemp("stage-probe")
    source = folder / "probe.c"
    source.write_text(r'''
#include "benchmark_stage.h"
#include <stdio.h>
static int calls;
struct tag_block { int count; void *elements; };
struct scenario { struct tag_block structure_bsp_references, cutscene_flags, ai_encounters; };
struct scenario_cutscene_flag { char name[32]; };
struct encounter_definition { char name[33]; };
#define TAG_BLOCK_GET_ELEMENT(block, index, type) (&((type *)(block)->elements)[index])
/* PRODUCTION_LOOKUP */
static void apply(void *context, int kind, int a, int b) {
    (void)context; (void)kind; (void)a; (void)b; calls++;
}
int main(int argc, char **argv) {
    struct benchmark_stage stage;
    char error[96];
    struct scenario_cutscene_flag flags[] = {{"hangar"}};
    struct encounter_definition encounters[] = {{"covenant"}, {"marines"}};
    struct scenario scenario = {{3, NULL}, {1, flags}, {2, encounters}};
    if (argc != 2 || !benchmark_stage_parse(argv[1], &stage)) return 1;
    if (!benchmark_stage_apply(&stage, stage_lookup, apply, &scenario, error, sizeof(error))) {
        printf("%d:%s", calls, error); return 2;
    }
    printf("%d:%s", calls, stage.json);
    return 0;
}
'''.replace('/* PRODUCTION_LOOKUP */', function('port/linux/game/input_replay_scenario.c', 'stage_lookup')))
    binary = folder / "probe"
    result = subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                             "-I", str(ROOT / "port/runtime"), str(source), "-o", str(binary)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return binary


def run_stage(binary, stage):
    return subprocess.run([str(binary), json.dumps(stage)], capture_output=True, text=True)


def test_stage_parser_canonicalizes_and_applies(stage_probe):
    result = run_stage(stage_probe, STAGE)
    assert result.returncode == 0, result.stderr
    assert result.stdout == "6:" + json.dumps(STAGE, separators=(",", ":"), sort_keys=True)


@pytest.mark.parametrize("stage", INVALID_STAGES)
def test_stage_parser_refuses_invalid_shapes(stage_probe, stage):
    assert run_stage(stage_probe, stage).returncode == 1


@pytest.mark.parametrize("step,missing", [({"teleport_player": "missing"}, "missing"),
    ({"ai_place": "missing"}, "missing"), ({"ai_see_player": "missing"}, "missing"),
    ({"ai_see": ["covenant", "missing"]}, "missing"), ({"switch_bsp": 3}, "3")])
def test_stage_validates_every_reference_before_applying(stage_probe, step, missing):
    result = run_stage(stage_probe, [{"deathless": True}, step])
    assert result.returncode == 2
    assert result.stdout.startswith("0:") and missing in result.stdout


@pytest.mark.parametrize("text", ['[{"deathless":t r u e}]', '[{"switch_bsp":1 2}]',
                                 '[{"switch_bsp":01}]', '[{"ai_place":"a"}]junk'])
def test_stage_parser_refuses_broken_json(stage_probe, text):
    assert subprocess.run([str(stage_probe), text], capture_output=True).returncode == 1


@pytest.mark.parametrize("development", [False, True])
def test_recording_and_replay_stage_tick_and_development_gate(tmp_path, development):
    binary = tmp_path / "input-replay-probe"
    command = ["cc", "-O2", "-fsanitize=address,undefined", "-I", str(ROOT / "port/linux/src")]
    if development:
        command.append("-DHALO_DEVELOPMENT_HOOKS=1")
    subprocess.run([*command, str(ROOT / "port/ios/tests/input_replay_probe.c"), "-o", str(binary)], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
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
    long tick;
    assert(benchmark_stage_parse("[{\"deathless\":true}]", &scene.stage));
    scene.stage_tick = 31;
    staged = benchmark_plan_signature(&plan);
    assert(staged != baseline);
    scene.stage_tick++;
    assert(staged != benchmark_plan_signature(&plan));
    assert(benchmark_scene_from_route("map = a50\ndifficulty = hard\nstage = [{\"deathless\":true}]\nstage_tick = 31\n", "battle", &route));
    assert(route.scene.stage_tick == 31 && route.scene.stage.count == 1);
    assert(!benchmark_stage_route("stage = [{\"deathless\":true}]\n", &scene.stage, &tick));
    assert(!benchmark_stage_route("stage_tick = 31\n", &scene.stage, &tick));
    assert(benchmark_stage_tick("2147483647", &tick) && tick == 2147483647L);
    assert(!benchmark_stage_tick("2147483648", &tick));
    assert(!benchmark_stage_tick("9999999999", &tick));
    return 0;
}
''')
    binary = tmp_path / "plan"
    subprocess.run(["cc", "-fsanitize=address,undefined", "-I", str(ROOT / "port/runtime"), str(source),
                    str(ROOT / "port/runtime/benchmark_plan.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
