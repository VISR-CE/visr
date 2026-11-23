"""debug.test_input focus:LABEL walks land on a row of the list and fail on a label it does not have."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def test_focus_walk_probe():
    with tempfile.TemporaryDirectory() as directory:
        executable = Path(directory) / 'focus-walk-probe'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        str(ROOT / 'port/ios/tests/test_focus_walk_probe.c'),
                        '--output', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
