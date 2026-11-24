"""The in-game graphics report separates applied overrides from observed rendering."""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    end = source.index("\n}\n\nstatic void benchmark_graphics_window", start)
    return source[start:end + 2]


def test_renderer_observations_do_not_infer_assets_or_shadow_targets(tmp_path):
    suite = (ROOT / "port/linux/src/benchmark_suite.c").read_text()
    actual = function(suite, "static int benchmark_graphics_snapshot_json(")
    source = tmp_path / "graphics_state.c"
    source.write_text(r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_GRAPHICS_MAXIMUM_BYTES (32 * 1024)
static long render_lines = 540, shadow_override = 512, aniso_override = 8;
static long shadow_target, sampler_max;
static unsigned long long texture_replacement_selections;
static int model_lod_override = 1, model_lod = 0;
static int config_graphics_settings_json(int scene, char *text, size_t size) {
    (void)scene;
    return snprintf(text,size,"{\"schema\":1,\"scene\":\"gameplay\",\"configuration_revision\":0,\"external_output\":false,\"values\":{\"display.upscaled_textures\":true,\"display.shadow_map_size\":256}}") < (int)size;
}
static void halo_benchmark_override_state(long *lines, long *shadow, long *anisotropy) {
    *lines=render_lines; *shadow=shadow_override; *anisotropy=aniso_override;
}
static void halo_benchmark_renderer_observations(long *shadow, long *anisotropy) {
    *shadow=shadow_target; *anisotropy=sampler_max;
}
static int halo_model_lod_override_state(void) { return model_lod_override; }
static int halo_model_lod_effective(void) { return model_lod; }
static int ambient_occlusion = 0;
static int halo_ambient_occlusion_effective(void) { return ambient_occlusion; }
static int soft_particles = 1;
static int halo_soft_particles_effective(void) { return soft_particles; }
''' + actual + r'''
int main(void) {
    char json[CONFIG_GRAPHICS_MAXIMUM_BYTES];
    assert(benchmark_graphics_snapshot_json(0,json,sizeof(json)));
    puts(json);
    shadow_target=512; sampler_max=8; texture_replacement_selections=3; ambient_occlusion=1; soft_particles=0;
    assert(benchmark_graphics_snapshot_json(0,json,sizeof(json)));
    puts(json);
    char tiny[32]="unsafe";
    assert(!benchmark_graphics_snapshot_json(0,tiny,sizeof(tiny)) && !tiny[0]);
}
''')
    binary = tmp_path / "graphics_state"
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)], check=True)
    first, last = map(json.loads, subprocess.run([str(binary)], capture_output=True, text=True, check=True).stdout.splitlines())
    assert first["schema"] == last["schema"] == 2
    assert first["values"]["display.shadow_map_size"] == 256
    assert first["applied_overrides"] == {"render_lines": 540, "shadow_map_size": 512,
        "anisotropic_filtering": 8, "model_lod": "max"}
    assert first["renderer_state"]["model_lod"] == "auto"
    assert first["renderer_state"]["shadow_target_size"] is None
    assert first["renderer_state"]["anisotropic_sampler_max"] is None
    assert first["renderer_state"]["texture_replacement_selections"] == 0
    assert first["renderer_state"]["texture_replacement_source"] == "unavailable"
    assert first["renderer_state"]["upscaled_texture_assets"] == "unavailable"
    assert last["renderer_state"]["shadow_target_size"] == 512
    assert last["renderer_state"]["anisotropic_sampler_max"] == 8
    assert last["renderer_state"]["texture_replacement_selections"] == 3
    assert last["renderer_state"]["texture_replacement_source"] == "debug_directory"
    # ambient occlusion as drawn, not as configured: off where the GPU cannot run it
    assert first["renderer_state"]["ambient_occlusion"] is False
    assert last["renderer_state"]["ambient_occlusion"] is True
    # soft particle edges as drawn: off where the GPU cannot run the pass (gpu_capabilities.soft_particles)
    assert first["renderer_state"]["soft_particles"] is True
    assert last["renderer_state"]["soft_particles"] is False


def test_reports_record_ambient_occlusion_as_drawn():
    # A GPU without SIMD-group permutes (gpu_capabilities.ambient_occlusion 0) draws no ambient occlusion, so the
    # front end's answer is off whatever the setting says, and the performance report takes that answer in place of
    # the configured value.
    device = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    mode = device[device.index("static int ssao_mode(void)"):]
    mode = mode[:mode.index("\n}\n")]
    assert mode.index("!device_capabilities.ambient_occlusion") < mode.index("config_integer(\"debug.ssao\")"), mode
    snapshot = device[device.index("config_graphics_settings_snapshot(scene, &perf_graphics)"):]
    snapshot = snapshot[:snapshot.index("host_perf_reports_graphics_snapshot(&perf_graphics)")]
    assert '"display.ambient_occlusion"' in snapshot and "halo_ambient_occlusion_effective()" in snapshot
    reports = (ROOT / "port/ios/host/host_perf_reports.m").read_text()
    assert '@"display.ambient_occlusion"' in reports
    metal = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    assert "capabilities->ambient_occlusion = ssao_supported() ? 1 : 0;" in metal
