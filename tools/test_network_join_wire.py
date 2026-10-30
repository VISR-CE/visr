"""Join request padding and refusal messages for network versions 40 and 41."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_probe_of_join_padding_and_rejection_messages():
    with tempfile.TemporaryDirectory() as directory:
        executable = Path(directory) / "network_join_wire_probe"
        subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-o", str(executable),
                        str(ROOT / "port/ios/tests/network_join_wire_probe.c")], check=True)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
