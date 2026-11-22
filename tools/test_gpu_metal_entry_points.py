"""The check that every function gpu_metal.o exports is classed (tools/gpu_metal_entry_points.py) reads the real tables
and refuses an unclassed function and a backend operation exported by name."""
import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gpu_metal_entry_points  # noqa: E402


class EntryPointTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (shutil.which('clang') or shutil.which('cc')):
            raise unittest.SkipTest('A C compiler is required')
        cls.direct, cls.operations = gpu_metal_entry_points.tables()

    def test_tables_hold_the_direct_entry_points_and_the_side_operations(self):
        self.assertEqual(self.direct['host_picture_drawable'], 'SURFACE')
        self.assertEqual(self.direct['gpu_metal_frame_deadline_hold'], 'WORKGROUP')
        for name in ('gpu_metal_draw', 'gpu_metal_stream_frame_end', 'gpu_metal_soft_depth_prepare',
                     'gpu_metal_set_upscaler', 'gpu_metal_output', 'gpu_metal_initialize'):
            self.assertIn(name, self.operations)

    def test_classed_and_runtime_symbols_pass(self):
        nm = ('0000000000001000 T _gpu_metal_async_compiles\n'
              '0000000000002000 S _gpu_backend_metal\n'
              '0000000000003000 S _OBJC_CLASS_$_MetalTexture\n'
              '0000000000003800 S __OBJC_PROTOCOL_$_CAMetalDisplayLinkDelegate\n'
              '0000000000003900 S __OBJC_LABEL_PROTOCOL_$_CAMetalDisplayLinkDelegate\n'
              '0000000000004000 S ___block_descriptor_32_e5_v8?0l\n'
              '0000000000005000 T _host_picture_drawable\n')
        names = gpu_metal_entry_points.exported(nm)
        self.assertEqual(gpu_metal_entry_points.problems(names, self.direct, self.operations), [])

    def test_an_unclassed_function_and_an_exported_operation_fail(self):
        nm = ('0000000000001000 T _host_ssao_apply\n'
              '0000000000002000 T _gpu_metal_set_crt\n')
        found = gpu_metal_entry_points.problems(gpu_metal_entry_points.exported(nm), self.direct, self.operations)
        self.assertEqual(len(found), 2)
        self.assertIn('gpu_metal_set_crt is a backend operation exported by name', found[0])
        self.assertIn('host_ssao_apply is an exported symbol without a class', found[1])

    def test_an_exported_data_symbol_fails(self):
        nm = ('0000000000001000 D _gpu_metal_side_switch\n'
              '0000000000002000 S _gpu_metal_counters\n')
        found = gpu_metal_entry_points.problems(gpu_metal_entry_points.exported(nm), self.direct, self.operations)
        self.assertEqual(len(found), 2)

    def test_the_object_is_the_configuration_just_built(self):
        import tempfile
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for folder in ('VISR.build/Release-iphoneos/x/arm64', 'VISR.build/Release-iphonesimulator/x/arm64'):
                (root / folder).mkdir(parents=True)
                (root / folder / 'gpu_metal.o').write_bytes(b'')
            found = gpu_metal_entry_points.find_object(root, 'Release-iphoneos')
            self.assertIn('Release-iphoneos', found.parts)
            self.assertIsNone(gpu_metal_entry_points.find_object(root, 'Release-xros'))


if __name__ == '__main__':
    unittest.main()
