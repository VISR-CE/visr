"""Traced object shadows without Metal: the game's records, skinning and the caster cache
(port/ios/host/traced_shadows.h), compiled and run from port/ios/tests/traced_shadows_probe.c."""
import pathlib
import shutil
import subprocess
import tempfile

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def test_records_skinning_and_caster_cache():
    compiler = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
    if not compiler:
        pytest.skip("no C compiler")
    with tempfile.TemporaryDirectory() as temp:
        executable = pathlib.Path(temp) / "probe"
        subprocess.run([
            compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1",
            "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/linux/src"),
            str(ROOT / "port/ios/tests/traced_shadows_probe.c"), "-lm", "--output", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)
