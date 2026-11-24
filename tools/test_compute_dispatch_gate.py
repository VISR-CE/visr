"""No compute dispatch runs on a GPU family without a gate that names it.

The bug class: a GPU path first enabled on an older family than anyone ran it on (the soft particle compute pass faulted
the A8 and the A10X). Every compute encoder in the app is listed here with what gates it; a new one fails this test
until it is listed with its gate, so its author has to say which families it runs on.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / "port/ios/host"

# file -> (number of compute encoders, what gates them)
LEDGER = {
    "gpu_metal.m": (4, "metal_pass: SSAO (AMBIENT_OCCLUSION), soft particles (SOFT_PARTICLES, soft_particles_supported) "
                       "and the traced shadows filter (TRACED_SHADOWS, through gpu_metal_traced_shadows_ready)"),
    "host_stereo.m": (1, "metal_pass(METAL_PASS_STEREO_HUD_DEPTH)"),
    "texture_refine.m": (2, "Neural Engine: the generator needs the model (texture_upscale_model_create), so no refine "
                            "runs without one; the A8 and A10X have none"),
}


def encoders():
    found = {}
    for path in sorted(HOST.glob("*.m")) + sorted(HOST.glob("*.mm")):
        count = len(re.findall(r"computeCommandEncoder", path.read_text()))
        if count:
            found[path.name] = count
    return found


def test_every_compute_encoder_is_in_the_ledger():
    found = encoders()
    assert set(found) == set(LEDGER), (found, "list the new compute encoder with its gate")
    for name, count in found.items():
        assert count == LEDGER[name][0], (name, count, "a new compute encoder needs its gate named in LEDGER")


def test_soft_particles_and_stereo_read_the_gate():
    assert "metal_pass(METAL_PASS_SOFT_PARTICLES)" in (HOST / "gpu_metal.m").read_text()
    assert "metal_pass(METAL_PASS_STEREO_HUD_DEPTH)" in (HOST / "host_stereo.m").read_text()


def test_texture_refine_only_runs_behind_the_model():
    # texture_refine is reached only from the generator, which cannot be created without the Neural Engine model
    for path in HOST.glob("*.m"):
        if path.name in ("texture_refine.m", "texture_upscale_generate.m"):
            continue
        assert not re.search(r"texture_refine_(color|bump)", path.read_text()), path.name
    generate = (HOST / "texture_upscale_generate.m").read_text()
    assert "texture_upscale_model_create(" in generate
    model = (HOST / "texture_upscale_model.m").read_text()
    assert "MLNeuralEngineComputeDevice" in model and "texture_upscale_model_available()" in model
