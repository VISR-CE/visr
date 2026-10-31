"""A run's measurements (port/runtime/benchmark_metrics.c) parse as JSON and carry the CPU split."""
from __future__ import annotations

import json
import re
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
DRIVER = r"""
#include "benchmark_metrics.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    struct benchmark_metrics metrics = { 0 };
    benchmark_metrics_reset(&metrics, 60.0);
    for (int index = 0; index < 30; index++) {
        if (argc > 2) benchmark_metrics_frame_split(&metrics, index + 1, index + 1, index + 1,
            index + 1, index + 1, index + 1, 1);
        else if (argc > 1) benchmark_metrics_frame(&metrics, 16.7, 11.0, 8.0);
        else benchmark_metrics_frame_split(&metrics, 16.7, 11.0, 8.0, 4.0 + index % 3, 6.0, 3.0, 1 + index % 2);
    }
    char *members = benchmark_metrics_json(&metrics);
    if (!members) return 1;
    printf("{%s}\n", members);
    free(members);
    benchmark_metrics_free(&metrics);
    return 0;
}
"""


def measurements(tmp_path: Path, *arguments: str) -> dict:
    compiler = shutil.which("cc") or shutil.which("clang")
    if not compiler:
        pytest.skip("no C compiler")
    (tmp_path / "driver.c").write_text(DRIVER)
    program = tmp_path / "driver"
    subprocess.run([compiler, "-std=c99", "-I", str(ROOT / "port/runtime"), str(tmp_path / "driver.c"),
                    str(ROOT / "port/runtime/benchmark_metrics.c"), "-lm", "-o", str(program)], check=True)
    return json.loads(subprocess.run([str(program), *arguments], check=True, capture_output=True, text=True).stdout)


def test_split_fields(tmp_path):
    record = measurements(tmp_path)
    for name in ("interval_ms", "gpu_ms", "cpu_ms", "simulation_cpu_ms", "render_cpu_ms", "translation_cpu_ms"):
        assert set(record[name]) == {"mean", "median", "p90", "p95", "p99", "worst"}
    assert record["simulation_cpu_ms"]["mean"] == pytest.approx(5.0)
    assert record["simulation_cpu_ms"]["worst"] == pytest.approx(6.0)
    assert record["render_cpu_ms"]["mean"] == pytest.approx(6.0)
    assert record["cpu_ms"]["mean"] == pytest.approx(11.0)
    assert record["ticks_per_frame"] == pytest.approx(1.5)
    assert record["translation_cpu_ms"]["mean"] == pytest.approx(3.0)


def test_split_unknown_is_null(tmp_path):
    record = measurements(tmp_path, "unsplit")
    assert record["simulation_cpu_ms"] is None
    assert record["render_cpu_ms"] is None
    assert record["ticks_per_frame"] is None
    assert record["translation_cpu_ms"] is None
    assert record["cpu_ms"]["mean"] == pytest.approx(11.0)


def test_split_semantics():
    source = (ROOT / "port/ios/host/host_benchmark_presentation.m").read_text()
    assert '@"simulation_cpu_ms": @"simulation_cpu_duration"' in source
    assert '@"render_cpu_ms": @"render_cpu_duration"' in source
    assert '@"ticks_per_frame": @"simulation_ticks_per_frame"' in source
    assert '@"translation_cpu_ms": @"translation_cpu_duration"' in source


def test_translation_timed_at_device_entry_points():
    device = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    for name in ("D3DDevice_DrawVertices", "D3DDevice_DrawIndexedVertices", "D3DDevice_End", "D3DDevice_Clear",
                 "D3DDevice_SetVertexShaderConstant", "D3DDevice_SetRenderTarget", "halo_draw_quad_ranges"):
        start = device.index(f" {name}(")
        body = device[device.index("{", start):device.index("{", start) + 200]
        # (the name is the profiling build's section for the call, docs/profiling.md)
        assert re.search(r"TRANSLATION_TIMED\(\w+\);", body), name


def test_p90_uses_nearest_rank_for_every_timing(tmp_path):
    record = measurements(tmp_path, "ramp", "ramp")
    for name in ("interval_ms", "cpu_ms", "gpu_ms", "simulation_cpu_ms", "render_cpu_ms", "translation_cpu_ms"):
        assert record[name]["p90"] == 27.0
        assert record[name]["p95"] == 29.0
        assert record[name]["p99"] == 30.0


def host_function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]


@pytest.mark.parametrize("scenario", ["context", "bounds", "begin_failure", "record_failure", "end_failure"])
def test_capacity_skip_record_journal_resume_and_summary(tmp_path, scenario):
    """Catch lost skip receipts, repeated skipped runs, and missing report context."""
    import sys
    if sys.platform != "darwin":
        pytest.skip("The host report probe requires Foundation")
    host = (ROOT / "port/ios/host/host_benchmark.m").read_text()
    functions = "\n".join(host_function(host, signature) for signature in (
        "void host_benchmark_workload(", "static void reset_run_record(", "static void record_run_context(", "static BOOL skip_capacity_run(", "static NSString *report_summary("))
    next_run = host_function(host, "int host_benchmark_next(")
    skip_branch = next_run[next_run.index("    scene = &plan.scenes[run.scene];"):next_run.index("    host_benchmark_phase(HOST_BENCHMARK_LOADING);")]
    profile = (ROOT / "port/ios/host/host_profile.c").read_text()
    functions = host_function(profile, "int host_profile_value(") + "\n" + host_function(host, "static int profile_player_capacity(") + "\n" + functions
    source = tmp_path / "capacity.m"
    source.write_text(r'''
#import <Foundation/Foundation.h>
#include <assert.h>
#include <stdatomic.h>
#include <os/lock.h>
#include <limits.h>
#include "benchmark_plan.h"
#include "host_profile.h"
static struct host_profile fixture_profile;
void host_device_profile_copy(struct host_profile *profile) { *profile=fixture_profile; }
static struct benchmark_plan plan;
static struct benchmark_run run;
static struct benchmark_outcome outcomes[BENCHMARK_RUNS_MAXIMUM];
static int outcome_count, run_player_capacity, run_workload_variable, run_workload_seen, run_open, run_finishing;
static _Atomic long window_ticks;
static double run_started_at, window_started_at;
static int run_capped, window_presented, window_open, window_interrupted, window_width, window_height;
static unsigned dynamic_line_count, dynamic_line_other, upscaler_requests, upscaler_paths;
static unsigned upscaler_frames[5];
static NSDictionary *graphics_first, *graphics_last, *run_shader_before, *run_shader_after;
static NSDictionary *run_environment_start, *run_environment_end, *run_output;
static BOOL graphics_changed;
static NSUInteger graphics_snapshots, graphics_invalid;
static NSString *run_shader_directory;
static os_unfair_lock lock=OS_UNFAIR_LOCK_INIT;
static NSString *shader_directory(void) { return @"shaders"; }
static enum benchmark_capacity_action run_capacity;
static NSString *journal_path;
static int failure_mode, write_attempts;
static _Atomic int finish_requested;
static NSMutableArray *flags;
static void flag(NSString *value) { [flags addObject:value]; }
static void host_benchmark_request_finish(void) { atomic_store(&finish_requested, 1); }
static int host_benchmark_next(char *, unsigned int);
#define JOURNAL journal_path.fileSystemRepresentation
static NSString *run_path(int index) { return [NSString stringWithFormat:@"run-%d.json", index]; }
static void record_run_context(NSMutableDictionary *, const struct benchmark_scene *, enum benchmark_status);
static BOOL write_run(const struct benchmark_run *finished, enum benchmark_status status, NSString *note,
    const char *measurements, NSDictionary *presentation) {
    (void)note; (void)measurements; (void)presentation;
    write_attempts++;
    if (failure_mode==2) return NO;
    if (failure_mode==3) journal_path=@"missing-directory/journal";
    NSMutableDictionary *record = [@{@"scene": @(plan.scenes[finished->scene].id), @"stage": @"A",
        @"status": @(benchmark_status_name(status))} mutableCopy];
    record_run_context(record, &plan.scenes[finished->scene], status);
    NSData *data = [NSJSONSerialization dataWithJSONObject:record options:0 error:nil];
    return [data writeToFile:run_path(finished->index) atomically:YES];
}
static NSString *host_benchmark_submission_summary(NSDictionary *record) { (void)record; return @"measured\n"; }
''' + functions + r'''
/* Exercise the host's real capacity branch; stand in for its completion UI. */
static int host_benchmark_next(char *text, unsigned int size) {
    const struct benchmark_scene *scene;
    if (atomic_load(&finish_requested)) return 2;
    if (!benchmark_plan_next(&plan,outcomes,outcome_count,&run)) return 2;
''' + skip_branch + r'''
    (void)scene;
    return 1;
}
int main(int argc, char **argv) { @autoreleasepool {
    assert(argc==2);
    flags=[NSMutableArray array];
    assert(profile_player_capacity()==0);
    fixture_profile.count=1;
    snprintf(fixture_profile.values[0].name,sizeof(fixture_profile.values[0].name),"network.player_capacity");
    snprintf(fixture_profile.values[0].text,sizeof(fixture_profile.values[0].text),"0");
    assert(profile_player_capacity()==0);
    snprintf(fixture_profile.values[0].text,sizeof(fixture_profile.values[0].text),"16");
    assert(profile_player_capacity()==16);
    fixture_profile.count=2; fixture_profile.values[1]=fixture_profile.values[0]; fixture_profile.drop_in=1;
    snprintf(fixture_profile.values[1].text,sizeof(fixture_profile.values[1].text),"8");
    assert(profile_player_capacity()==8);
    snprintf(fixture_profile.values[1].scene,sizeof(fixture_profile.values[1].scene),"variable");
    assert(profile_player_capacity()==16);
    const struct benchmark_scene scenes[] = {
        { .id="bg16", .map="bloodgulch", .kind=BENCHMARK_SCENE_MULTIPLAYER, .players=16 },
        { .id="sw128", .map="sidewinder", .kind=BENCHMARK_SCENE_MULTIPLAYER, .players=128 },
        { .id="campaign", .map="a10", .kind=BENCHMARK_SCENE_GAMEPLAY }
    };
    plan = (struct benchmark_plan){ .mode=BENCHMARK_UNIFIED, .scenes=scenes, .scene_count=3 };
    journal_path = @"journal";
    assert(benchmark_journal_start(JOURNAL, plan.mode, "CAPACITY", &plan.device, benchmark_plan_signature(&plan)));
    window_width=1920; window_height=1080; dynamic_line_count=8; dynamic_line_other=3;
    upscaler_requests=upscaler_paths=4; upscaler_frames[2]=100;
    graphics_first=graphics_last=@{@"stale": @YES}; graphics_changed=YES; graphics_snapshots=42;
    run_shader_before=run_shader_after=@{@"stale": @YES};
    run_started_at=window_started_at=10; window_ticks=1800; run_capped=window_presented=1;
    host_benchmark_workload(0); reset_run_record();
    NSMutableDictionary *unknown=[NSMutableDictionary dictionary];
    record_run_context(unknown,&scenes[0],BENCHMARK_CRASHED);
    if (!strcmp(argv[1],"context")) {
        assert(unknown[@"workload_context"]==NSNull.null && !run_workload_seen);
    }
    assert(!window_width && !window_height && !dynamic_line_count && !dynamic_line_other);
    assert(!upscaler_requests && !upscaler_paths && !upscaler_frames[2]);
    assert(!graphics_first && !graphics_last && !graphics_changed && !graphics_snapshots);
    assert(!run_shader_before && !run_shader_after && !run_started_at && !window_ticks && !run_capped && !window_presented);
    run_open=1; host_benchmark_workload(1);
    assert(run_workload_variable==1);
    if (!strcmp(argv[1],"context")) assert(run_workload_seen);
    run_finishing=1; host_benchmark_workload(0);
    assert(run_workload_variable==1 && run_workload_seen);
    run_finishing=0;
    run_player_capacity=8; run_capacity=BENCHMARK_CAPACITY_PROBE;
    NSMutableDictionary *record=[NSMutableDictionary dictionary];
    record_run_context(record, &scenes[0], BENCHMARK_OK);
    assert([record[@"workload_context"] isEqual:@"variable"]);
    assert([record[@"player_capacity"] intValue]==8 && [record[@"capacity_probe"] boolValue]);
    assert(CFGetTypeID((__bridge CFTypeRef)record[@"capacity_probe"])==CFBooleanGetTypeID());
    assert(!record[@"skip_reason"]);
    run_player_capacity=0; run_capacity=BENCHMARK_CAPACITY_RUN;
    record=[NSMutableDictionary dictionary]; record_run_context(record,&scenes[0],BENCHMARK_FAILED);
    assert([record[@"player_capacity"] intValue]==0 && ![record[@"capacity_probe"] boolValue]);
    host_benchmark_workload(0);
    record=[NSMutableDictionary dictionary]; record_run_context(record,&scenes[2],BENCHMARK_OK);
    assert([record[@"workload_context"] isEqual:@"predictable"] && !record[@"player_capacity"]);
    if (!strcmp(argv[1],"bounds")) {
        outcome_count=BENCHMARK_RUNS_MAXIMUM;
        outcomes[BENCHMARK_RUNS_MAXIMUM-1].status=BENCHMARK_FAILED;
        run=(struct benchmark_run){.index=0};
        snprintf(run.key,sizeof(run.key),"A sw128");
        assert(skip_capacity_run());
        assert(outcome_count==BENCHMARK_RUNS_MAXIMUM);
        assert(outcomes[BENCHMARK_RUNS_MAXIMUM-1].status==BENCHMARK_FAILED);
        return 0;
    }
    if (strstr(argv[1],"_failure")) {
        outcome_count=1;
        outcomes[0].status=BENCHMARK_OK;
        fixture_profile.count=1;
        snprintf(fixture_profile.values[0].text,sizeof(fixture_profile.values[0].text),"8");
        if (!strcmp(argv[1],"begin_failure")) { failure_mode=1; journal_path=@"missing-directory/journal"; }
        else failure_mode=!strcmp(argv[1],"record_failure") ? 2 : 3;
        write_attempts=0;
        assert(host_benchmark_next(NULL,1)==2);
        assert(atomic_load(&finish_requested) && [flags containsObject:@"storage_failed"]);
        assert(outcome_count==1 && write_attempts==(failure_mode==1 ? 0 : 1));
        assert(host_benchmark_next(NULL,1)==2);
        assert(write_attempts==(failure_mode==1 ? 0 : 1));
        return 0;
    }
    reset_run_record();
    assert(!run_workload_seen && !run_workload_variable);
    run_open=0;
    host_benchmark_workload(1);
    record=[NSMutableDictionary dictionary]; record_run_context(record,&scenes[0],BENCHMARK_FAILED);
    assert(record[@"workload_context"]==NSNull.null);
    run_open=1; run_finishing=1;
    host_benchmark_workload(0);
    record=[NSMutableDictionary dictionary]; record_run_context(record,&scenes[2],BENCHMARK_CRASHED);
    assert(record[@"workload_context"]==NSNull.null);
    run_open=run_finishing=0;
    /* The real host skip helper writes the receipt and the real journal advances past it on resume. */
    run_player_capacity=8; run_capacity=BENCHMARK_CAPACITY_SKIP;
    assert(benchmark_plan_next(&plan,outcomes,0,&run));
    assert(benchmark_journal_begin(JOURNAL,run.index,run.key));
    outcomes[outcome_count++]=(struct benchmark_outcome){.status=BENCHMARK_OK};
    assert(benchmark_journal_end(JOURNAL,0,&outcomes[0]));
    assert(benchmark_plan_next(&plan,outcomes,1,&run));
    assert(skip_capacity_run());
    NSData *data=[NSData dataWithContentsOfFile:run_path(1)];
    record=[NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:nil];
    assert([record[@"status"] isEqual:@"skipped"] && [record[@"skip_reason"] isEqual:@"over_profile_limit"]);
    assert([record[@"players"] intValue]==128 && [record[@"player_capacity"] intValue]==8);
    assert(![record[@"capacity_probe"] boolValue]);
    assert(record[@"workload_context"]==NSNull.null);
    assert(CFGetTypeID((__bridge CFTypeRef)record[@"capacity_probe"])==CFBooleanGetTypeID());
    assert([report_summary() isEqual:@"sw128: skipped: over profile limit\n"]);
    struct benchmark_journal journal; benchmark_journal_load(JOURNAL,&journal);
    assert(journal.valid && !journal.cut_off && journal.count==2 && journal.outcomes[1].status==BENCHMARK_SKIPPED);
    assert(benchmark_plan_next(&plan,journal.outcomes,journal.count,&run) && run.scene==2);
    record[@"skip_reason"]=@"missing_map";
    [[NSJSONSerialization dataWithJSONObject:record options:0 error:nil] writeToFile:run_path(1) atomically:YES];
    assert([report_summary() isEqual:@"sw128: not run on this device\n"]);
} }
''')
    binary = tmp_path / "capacity"
    subprocess.run(["xcrun", "clang", "-Wall", "-Wextra", "-Werror", "-fobjc-arc",
                    "-fsanitize=address,undefined", "-framework", "Foundation",
                    "--include-directory=" + str(ROOT / "port/runtime"),
                    "--include-directory=" + str(ROOT / "port/ios/host"), str(source),
                    str(ROOT / "port/runtime/benchmark_plan.c"), str(ROOT / "port/runtime/benchmark_journal.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary), scenario], cwd=tmp_path, check=True)


def test_workload_bridge_uses_game_decision_and_host_gates_before_loading():
    host = (ROOT / "port/ios/host/host_benchmark.m").read_text()
    game = (ROOT / "port/linux/game/scene_class.c").read_text()
    decision = host_function(game, "void halo_workload_decide(")
    assert decision.index("config_workload_set_limited(") < decision.index("host_benchmark_workload(config_workload_variable())")
    assert "host_benchmark_workload" in (ROOT / "port/runtime/host_imports.list").read_text()
    assert "void host_benchmark_workload(int variable);" in (ROOT / "port/runtime/guest/runtime/guest_host.h").read_text()
    next_run = host_function(host, "int host_benchmark_next(")
    assert next_run.index("skip_capacity_run()") < next_run.index("host_benchmark_phase(HOST_BENCHMARK_LOADING)")
    assert next_run.index("reset_run_record()") < next_run.index("skip_capacity_run()")
    write_run = host_function(host, "static BOOL write_run(")
    assert "record_run_context(record, scene, status)" in write_run
