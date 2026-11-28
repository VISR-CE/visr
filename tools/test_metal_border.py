"""Exercise production Metal sampler construction/binding without submitting GPU work."""
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from probekit import source as probe_source  # noqa: E402

PATH = "port/ios/host/gpu_metal.m"
SOURCE = ROOT / PATH


def test_zero_border_resource_transitions_select_and_bind_the_correct_sampler():
    source = SOURCE.read_text()
    cache_start = source.index("#define FRONT_CACHE_ENTRIES")
    cache_end = source.index("/* every pipeline made", cache_start)
    stages_end = source.index("} stage_samplers[GPU_STAGE_COUNT];") + len("} stage_samplers[GPU_STAGE_COUNT];")
    stages_start = source.rfind("static struct", 0, stages_end)
    actual = source[cache_start:cache_end] + "\n"
    for name in ("border_color", "exact_borders", "format_has_alpha", "zero_border_eligible", "sampler_state"):
        # The initial red run predates the eligibility helper.
        if probe_source.defined(PATH, name):
            actual += probe_source.function(PATH, name) + "\n"
    actual += source[stages_start:stages_end] + "\n"
    # bind_stages records whether it bound the stages' rate map, for the traced light bound after it
    if "static BOOL stages_rate_map;" in source:
        actual += "static BOOL stages_rate_map;\n"
    actual += probe_source.function(PATH, "bind_stages")
    fixture = (ROOT / "port/ios/tests/metal_border_probe.m").read_text()
    with tempfile.TemporaryDirectory(prefix="metal-border-") as directory:
        directory = Path(directory)
        generated = directory / "probe.m"
        generated.write_text(fixture.replace("/* PRODUCTION_SAMPLER_AND_BINDING */", actual))
        binary = directory / "probe"
        subprocess.run([
            "xcrun", "--sdk", "macosx", "clang", "-fobjc-arc", "-O1",
            "-fsanitize=address,undefined", "-Iport/linux/src", "-Iport/ios/host",
            "-framework", "Foundation", "-framework", "Metal", str(generated), "-o", str(binary),
        ], cwd=ROOT, check=True)
        # The capability is immutable in production; each mode needs empty caches.
        for native in ("legacy", "native"):
            subprocess.run([str(binary), native], cwd=ROOT, check=True)


if __name__ == "__main__":
    test_zero_border_resource_transitions_select_and_bind_the_correct_sampler()
