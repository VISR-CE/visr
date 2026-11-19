"""Asset-free map picture metadata and image regressions, collected by ci_test.py."""
from pathlib import Path
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize('name,define,sources', [
    ('metadata', 'MAP_PICTURES_TEST', ['map_pictures_test.c', 'map_pictures.c']),
    ('image', 'MAP_PICTURES_IMAGE_TEST',
     ['map_pictures_image_test.c', 'map_pictures_image.c', 'bmp_files.c']),
])
def test_native_map_pictures(tmp_path, name, define, sources):
    game = ROOT / 'port/linux/game'
    executable = tmp_path / name
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-D' + define, '-I', str(game),
                    *[str(game / source) for source in sources],
                    '-lm', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
