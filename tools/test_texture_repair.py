"""The texture repair filter's property tests (port/runtime/texture_repair.c) on synthetic gradients, with no map data.

Bug class: a repair that repaints (moves a texel farther than the stored rounding allows) or blurs detail (changes a
texel outside a flagged area). The probe checks every repaired texel against exact rational rounding (R5G6B5) and
against the block's original endpoints (DXT1). The same checks run on every stock map where the maps are, through
tools/texture_repair_sweep.py.
"""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_texture_repair_properties(tmp_path):
    probe = tmp_path / "texture-repair-probe"
    subprocess.run(["cc", "-std=c99", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=undefined", "-I", str(ROOT / "port/runtime"),
                    str(ROOT / "port/ios/tests/texture_repair_probe.c"), str(ROOT / "port/runtime/texture_repair.c"),
                    "-lm", "-o", str(probe)], check=True)
    result = subprocess.run([str(probe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "texture repair: ok"
