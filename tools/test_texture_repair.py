"""The texture repair filter's property tests (port/runtime/texture_repair.c) on synthetic gradients, with no map data.

Bug class: a repair that repaints (moves a texel farther than the stored rounding allows) or blurs detail (changes a
texel outside a flagged area). The probe checks every repaired texel against exact rational rounding (R5G6B5) and
against the block's original endpoints (DXT1). The same checks run on every stock map where the maps are, through
tools/texture_repair_sweep.py.
"""
import hashlib
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Bug class: a change to what the texture repair produces (the filter, its limits, which textures it takes, the storing
# rule) that ships without raising TEXTURE_REPAIR_VERSION, so devices keep repaired records and upscaled textures made
# from the old repair. The digest covers texture_repair.h and texture_repair.c without comments and spacing; each version
# keeps the digest it shipped with.
REPAIR_DIGESTS = {
    1: "62dbfb861b5c5980053fe7b60f203955b7f98748a0d423698e57aaa19470229f",
}


def repair_digest():
    digest = hashlib.sha256()
    for name in ("texture_repair.h", "texture_repair.c"):
        text = (ROOT / "port/runtime" / name).read_text()
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        text = re.sub(r"//[^\n]*", "", text)
        digest.update(" ".join(text.split()).encode())
    return digest.hexdigest()


def test_texture_repair_version_covers_the_filter():
    header = (ROOT / "port/runtime/texture_repair.h").read_text()
    version = int(re.search(r"#define TEXTURE_REPAIR_VERSION (\d+)u", header).group(1))
    digest = repair_digest()
    assert version == max(REPAIR_DIGESTS), f"TEXTURE_REPAIR_VERSION is {version}: add its digest {digest} to REPAIR_DIGESTS"
    assert REPAIR_DIGESTS[version] == digest, (
        f"texture_repair.c or texture_repair.h changed (digest {digest}) under version {version}: raise "
        f"TEXTURE_REPAIR_VERSION to {version + 1} and add {version + 1}: \"{digest}\" to REPAIR_DIGESTS, so every device "
        "makes its repaired and upscaled textures again")


def test_texture_repair_version_reaches_both_recipes():
    """The repair's records and the upscaler's global recipe both carry the one version."""
    repair = (ROOT / "port/ios/host/host_texture_repair.m").read_text()
    upscale = (ROOT / "port/ios/host/host_texture_upscale.m").read_text()
    cache = (ROOT / "port/runtime/texture_cache.c").read_text()
    assert re.search(r"#define REPAIR_RECIPE \(0x[0-9a-f]+ull \| TEXTURE_REPAIR_VERSION\)", repair)
    assert not re.search(r"#define \w*REPAIR\w*VERSION", repair + upscale), "a second repair version would drift"
    assert ".repair_version=TEXTURE_REPAIR_VERSION" in upscale
    assert ";repair=%u" in cache and "g->repair_version" in cache
    # the upscaler's source is repaired with the same whole-texture call the repair's records use
    assert "texture_repair_texture(" in upscale and "texture_repair_texture(" in repair


def test_texture_repair_properties(tmp_path):
    probe = tmp_path / "texture-repair-probe"
    subprocess.run(["cc", "-std=c99", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=undefined", "-I", str(ROOT / "port/runtime"),
                    str(ROOT / "port/ios/tests/texture_repair_probe.c"), str(ROOT / "port/runtime/texture_repair.c"),
                    "-lm", "-o", str(probe)], check=True)
    result = subprocess.run([str(probe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "texture repair: ok"


def test_background_texture_work_stays_out_of_measured_runs():
    """Bug class: background work running behind a benchmark's numbers. Every host file that starts background texture
    work asks the one measured-run check, and nothing else reads the measured-run settings on its own."""
    host = ROOT / "port/ios/host"
    for name in ("host_texture_upscale.m", "host_texture_repair.m"):
        assert "host_texture_measured_run()" in (host / name).read_text(), f"{name} does not ask host_texture_measured_run"
    readers = [path.name for path in host.glob("host_texture_*.m")
               if "HALO_FIXED_TIMESTEP" in path.read_text() or "debug.input_replay" in path.read_text()]
    assert readers == ["host_texture_upscale.m"], f"measured-run settings read outside host_texture_measured_run: {readers}"
