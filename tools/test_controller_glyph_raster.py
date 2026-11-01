"""Exercise Apple's real SF Symbol rasterizer without launching an app."""
from pathlib import Path
import subprocess
import tempfile
import shutil
ROOT = Path(__file__).resolve().parents[1]
def test_native_glyph_coverage(tmp_path):
    if not shutil.which('xcrun'):
        import pytest
        pytest.skip('Apple compiler and frameworks are required')
    source = ROOT / 'port/ios/host/host_controller_glyphs.m'
    binary = tmp_path / 'glyph-raster'
    subprocess.run(['xcrun', 'clang', '-fobjc-arc', '-DHALO_NATIVE_MAC=1', '-Wall', '-Werror',
                    '-framework', 'AppKit', '-framework', 'Foundation',
                    str(source), str(ROOT / 'port/ios/tests/controller_glyph_raster_probe.m'),
                    '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as directory:
        test_native_glyph_coverage(Path(directory))
