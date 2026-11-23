"""The soft particle compute pass stays behind its GPU-family check.

The bug class: a GPU path first enabled on an older GPU family than anyone ran it on. `soft_depth_linearize` first ran on
the A8 (Apple GPU family 2, the Apple TV HD) and the A10X (family 3) after the dispatch fix, and the GPU faulted ("victim of GPU error/recovery")
whatever the kernel read. `soft_depth_program`, which the warm-up, the usable check and the prepare all call, returns
false where `soft_particles_supported()` (the gate's answer) is false, and the front end learns it from `gpu_capabilities`.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
METAL = (ROOT / "port/ios/host/gpu_metal.m").read_text()
DEVICE = (ROOT / "port/linux/src/d3d8_device.c").read_text()


def soft_particle_code():
    """the predicate and the compute pass's program setup: everything that decides whether the pass may run"""
    first = METAL.index("static BOOL soft_particles_supported(void)")
    second = METAL.index("static BOOL soft_depth_program(void)")
    return METAL[first:METAL.index("\n}\n", first)] + METAL[second:METAL.index("\n}\n", second)]


def test_the_predicate_reads_the_gate_and_nothing_checks_a_family():
    # one place decides the floor: the pass table's row in metal_gate.h; no soft particle code asks Metal for a family
    code = soft_particle_code()
    assert "metal_pass(METAL_PASS_SOFT_PARTICLES)" in code
    assert "supportsFamily" not in code and "MTLGPUFamily" not in code
    gate = (ROOT / "port/ios/host/metal_gate.h").read_text()
    row = gate[gate.index("X(SOFT_PARTICLES,"):]
    row = row[:row.index('"soft particles"')]
    assert re.search(r"METAL_FLOOR_\w+, 1,", row), "the row must be enforced, or the backend does not read it"


def test_every_entry_goes_through_the_gated_program():
    start = METAL.index("static BOOL soft_depth_program(void)")
    body = METAL[start:start + 700]
    assert "soft_particles_supported()" in body
    assert body.index("soft_particles_supported()") < body.index("soft.tried")
    assert METAL.index("static BOOL soft_particles_supported(void)") < start


def test_capability_reports_the_predicate():
    assert re.search(r"capabilities->soft_particles = soft_particles_supported\(\)", METAL)


def test_front_end_asks_the_capability():
    assert DEVICE.count("device_capabilities.soft_particles") >= 2


def test_reports_record_soft_particles_as_drawn():
    # one helper answers for the frame; the draw-time check adds the view's targets, the report records the helper
    start = DEVICE.index("static BOOL soft_particles_enabled(void)")
    body = DEVICE[start:DEVICE.index("\n}\n", start)]
    assert "device_capabilities.soft_particles" in body and "halo_stereo_frame()" in body
    assert "soft_particles_enabled()" in DEVICE[DEVICE.index("static BOOL soft_view_wanted(void)"):][:400]
    effective = DEVICE[DEVICE.index("int halo_soft_particles_effective(void)"):][:120]
    assert "soft_particles_enabled()" in effective
    suite = (ROOT / "port/linux/src/benchmark_suite.c").read_text()
    assert "halo_soft_particles_effective()" in suite and '\\"soft_particles\\"' in suite
