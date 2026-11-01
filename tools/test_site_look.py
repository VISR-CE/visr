"""Exercise panorama loading and keyboard lifecycle without a browser or network."""

from pathlib import Path
import shutil
import subprocess

import pytest


def test_panorama_loading_and_keyboard_lifecycle():
    node = shutil.which("node")
    if node is None:
        pytest.skip("Node.js is required for the panorama lifecycle checks")
    result = subprocess.run(
        [node, str(Path(__file__).with_name("site-look-test.cjs"))],
        capture_output=True,
        text=True,
        check=False,
        timeout=30,
    )
    assert result.returncode == 0, result.stdout + result.stderr
