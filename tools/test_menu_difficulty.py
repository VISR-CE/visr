"""Unknown difficulty frames must preserve the existing upscale."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def test_difficulty_source_guards():
    with tempfile.TemporaryDirectory() as directory:
        executable = Path(directory) / 'difficulty-probe'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '--include-directory=' + str(ROOT / 'port/ios/host'),
                        str(ROOT / 'port/ios/tests/menu_difficulty_probe.c'),
                        str(ROOT / 'port/ios/host/menu_difficulty.c'),
                        '-lm', '--output', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
