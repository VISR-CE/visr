"""Run the production benchmark state machine and typed match handoff without game data."""
from pathlib import Path
import shutil
import subprocess
from unittest import SkipTest

from probekit import cc

ROOT = Path(__file__).resolve().parents[1]


def test_benchmark_startup_and_typed_multiplayer_handoff(tmp_path):
    if not shutil.which("clang"):
        raise SkipTest("A C compiler is required for the benchmark startup probe")
    binary = tmp_path / "benchmark-startup-probe"
    cc.build([ROOT / "port/ios/tests/benchmark_startup_probe.c"], output=binary, sanitize=True,
             includes=[ROOT / "port/linux/src"], options=["-O2", "-iquote", ROOT / "port/linux/include"],
             port_stubs=True)
    subprocess.run([str(binary)], check=True)
