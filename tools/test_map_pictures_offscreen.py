"""Map import frames use Metal's unpresented target without changing benchmark rules."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def test_import_offscreen_rule(tmp_path):
    probe = tmp_path / "probe"
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "port/ios/host"),
                    str(ROOT / "port/ios/tests/benchmark_offscreen_probe.c"),
                    "-o", str(probe)], check=True)
    subprocess.run([str(probe), "map_pictures_import"], check=True)


def test_import_uses_same_rule_for_pacer_and_present():
    source = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    assert "if (!render_offscreen_active())\n\t\tframe_pacer_frame_start(pacing.off);" in source
    assert "BOOL offscreen = render_offscreen_active();" in source
    assert "host_render_offscreen_frame(host_benchmark_offscreen_active()," in source
    assert "host_map_pictures_import_stage(), TARGET_OS_VISION)" in source


def test_picture_readback_precedes_present():
    source = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    start = source.index("if (map_pictures_render_wants_frame())")
    end = source.index("gpu_present(back_buffer->target.texture)", start)
    assert "target_read(picture, pixels, picture_size)" in source[start:end]
    metal = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    start = metal.index("static uint32_t gpu_metal_texture_read(")
    body = metal[start:metal.index("/* level 0 of source", start)]
    assert "copyFromTexture:record->texture" in body
    assert body.rindex("waitUntilCompleted") < body.rindex("memcpy(pixels, staging.contents")
