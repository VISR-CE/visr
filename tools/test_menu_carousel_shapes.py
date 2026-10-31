"""Asset-free guards for the source-fitted carousel redraw."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CarouselShapesTests(unittest.TestCase):
    def test_unknown_source_and_invalid_output_preserve_upscale(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / 'shapes.dylib'
            subprocess.run(['cc', '-shared', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            str(ROOT / 'port/ios/host/menu_picture_shapes.c'),
                            '-lm', '--output', str(library)], check=True)
            dll = ctypes.CDLL(str(library))
            identify = dll.menu_carousel_shapes_source
            redraw = dll.menu_carousel_shapes
            source = (ctypes.c_uint8 * (32 * 256 * 4))()
            output = (ctypes.c_uint8 * (32 * 256 * 4 * 16))(*([73] * (32 * 256 * 4 * 16)))
            saved = bytes(output)
            self.assertEqual(identify(None, 32, 256), -1)
            self.assertEqual(identify(source, 32, 256), -1)
            self.assertEqual(identify(source, 16, 256), -1)
            for scale in (0, 1, 2, 4, 8):
                self.assertEqual(redraw(source, 32, 256, output, scale, None), 0)
                self.assertEqual(bytes(output), saved)


if __name__ == '__main__':
    unittest.main()
