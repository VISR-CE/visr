"""Run the production menu pointer and layout probes without Apple frameworks."""

from pathlib import Path
import shutil
import subprocess
from unittest import SkipTest

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("probe", ["menu_pointer_probe", "menu_gaze_probe", "portrait_rule_probe"])
def test_menu_pointer_mapping(probe, tmp_path):
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        raise SkipTest("A C compiler is required for the menu pointer probes")
    binary = tmp_path / probe
    subprocess.run([
        compiler, "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-Iport/ios/host", "-Iport/linux/src", "-Iport/runtime/include",
        f"port/ios/tests/{probe}.c", "-o", str(binary), "-lm",
    ], cwd=ROOT, check=True)
    subprocess.run([str(binary)], check=True)
