"""Asset-free map import progress math, collected by ci_test.py."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def test_map_pictures_progress(tmp_path):
    executable = tmp_path / "map-pictures-progress"
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "port/ios/host"),
                    str(ROOT / "port/ios/tests/map_pictures_progress_probe.c"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
