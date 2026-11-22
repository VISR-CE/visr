#!/usr/bin/env python3
"""tools/host_syntax_check.py must catch a call placed above its declaration inside one platform's `#if`.

The class: a host source only one platform compiles (visionOS, tvOS, an iPhone-only block) that breaks
that platform's build while every other check passes. The fixture below is that mistake in miniature.
"""
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

FIXTURE = '''\
#include <TargetConditionals.h>
#if TARGET_OS_VISION
void use_before_declaration(void) { frame_repeat_apply(); }
static void frame_repeat_apply(void);
#endif
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
void iphone_use_before_declaration(void) { snapshot_output(); }
static void snapshot_output(void);
#endif
void always_fine(void) {}
'''

CLEAN = FIXTURE.replace('void use_before', 'static void frame_repeat_apply(void);\nvoid use_before') \
                .replace('void iphone_use', 'static void snapshot_output(void);\nvoid iphone_use')


def sdk_path(sdk):
    result = subprocess.run(['xcrun', '--sdk', sdk, '--show-sdk-path'], stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, text=True)
    return result.stdout.strip() if result.returncode == 0 else None


class SyntaxCommandTransform(unittest.TestCase):
    def test_compile_to_object_with_strict_flags(self):
        import host_syntax_check
        command = host_syntax_check.syntax_command(
            '/usr/bin/clang -DX=1 -I/a -x objective-c -MD -MT t -MF d.d -o obj.o -c /src/host_a.m', '/tmp/new.o')
        self.assertEqual(command[0], '/usr/bin/clang')
        self.assertEqual(command.count('-o'), 1)
        self.assertEqual(command[command.index('-o') + 1], '/tmp/new.o')
        self.assertIn('-c', command)
        self.assertNotIn('obj.o', command)
        self.assertNotIn('-MD', command)
        self.assertIn('-O0', command)
        self.assertIn('-Werror=implicit-function-declaration', command)
        self.assertEqual(command[-1], '/src/host_a.m')
        self.assertIn('-DX=1', command)


def write_database(folder, files):
    import json
    path = Path(folder) / 'compile_commands.json'
    path.write_text(json.dumps([{'file': str(file), 'command': 'clang -c x', 'directory': '/'} for file in files]))
    return path


class DefinerEntries(unittest.TestCase):
    def setUp(self):
        import host_syntax_check
        self.module = host_syntax_check
        self.root = host_syntax_check.ROOT

    def test_selects_host_runtime_and_bridges_but_not_third_party_or_assembly(self):
        with tempfile.TemporaryDirectory() as folder:
            database = write_database(folder, [
                self.root / 'port/ios/host/host_a.m', self.root / 'port/ios/host/guest_call.S',
                self.root / 'port/runtime/other.c', self.root / 'port/third_party/kcp/ikcp.c',
                self.root / 'build/ios/host/bridges.c'])
            entries = self.module.definer_entries(database)
            self.assertEqual(sorted(Path(path).relative_to(self.root).as_posix() for path in entries),
                             ['build/ios/host/bridges.c', 'port/ios/host/host_a.m', 'port/runtime/other.c'])
            plain = self.module.definer_entries(database, validation=False)
            self.assertNotIn(str(self.root / 'build/ios/host/bridges.c'), plain)

    def test_a_database_with_no_host_sources_fails_instead_of_passing(self):
        with tempfile.TemporaryDirectory() as folder:
            database = write_database(folder, [self.root / 'port/runtime/other.c'])
            with self.assertRaises(SystemExit):
                self.module.definer_entries(database)

    def test_paths_through_a_symlinked_checkout_still_match(self):
        """CMake names a symlinked checkout by its link path; the sweep once compiled nothing and passed"""
        with tempfile.TemporaryDirectory() as folder:
            link = Path(folder) / 'link'
            link.symlink_to(self.root)
            database = write_database(folder, [link / 'port/ios/host/host_a.m'])
            self.assertEqual(len(self.module.definer_entries(database)), 1)


class ImportNames(unittest.TestCase):
    def test_lists_are_found_and_validation_ones_follow_the_flag(self):
        import host_syntax_check
        with_validation = host_syntax_check.import_names(True)
        without = host_syntax_check.import_names(False)
        self.assertTrue(without)
        self.assertLess(without, with_validation)

    def test_an_empty_import_list_fails_instead_of_passing(self):
        import host_syntax_check
        original = host_syntax_check.ROOT
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / 'port/runtime').mkdir(parents=True)
            host_syntax_check.ROOT = Path(folder)
            try:
                with self.assertRaises(SystemExit):
                    host_syntax_check.import_names(True)
            finally:
                host_syntax_check.ROOT = original


@unittest.skipUnless(platform.system() == 'Darwin' and shutil.which('xcrun'), 'needs Xcode')
class CatchesUseBeforeDeclarationPerPlatform(unittest.TestCase):
    def compile(self, source, sdk, target):
        import host_syntax_check
        path = sdk_path(sdk)
        if not path:
            self.skipTest(f'no {sdk} SDK')
        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder) / 'fixture.c'
            file.write_text(source)
            command = f'clang -x c -std=gnu17 -arch arm64 -isysroot {shlex.quote(path)} -target {target} -o fixture.o -c {file}'
            return host_syntax_check.check_one({'file': str(file), 'command': command, 'directory': folder},
                                               Path(folder) / 'fixture.o')

    def test_visionos_only_mistake_is_caught_on_visionos_not_iphone(self):
        _, ok, output = self.compile(FIXTURE, 'xros', 'arm64-apple-xros2.0')
        self.assertFalse(ok)
        self.assertIn('frame_repeat_apply', output)
        _, ok, output = self.compile(FIXTURE, 'iphoneos', 'arm64-apple-ios16.0')
        self.assertFalse(ok)
        self.assertIn('snapshot_output', output)
        self.assertNotIn('frame_repeat_apply', output)

    def test_declared_first_passes_everywhere(self):
        for sdk, target in (('xros', 'arm64-apple-xros2.0'), ('iphoneos', 'arm64-apple-ios16.0'),
                            ('appletvos', 'arm64-apple-tvos16.0')):
            _, ok, output = self.compile(CLEAN, sdk, target)
            self.assertTrue(ok, output)

    def test_tvos_compiles_neither_block(self):
        _, ok, output = self.compile(FIXTURE, 'appletvos', 'arm64-apple-tvos16.0')
        self.assertTrue(ok, output)


class GuestImageHeader(unittest.TestCase):
    def test_header_declares_every_guest_image_name_the_host_uses(self):
        """the syntax-check stub is made by the generator of the real header, so a name a host source
        starts using (an `IOS_GUEST_*` macro or a `halo_guest_*` blob) must be in what it writes"""
        import re
        import host_syntax_check
        import ios_embed_guest
        header = ios_embed_guest.image_header('0' * 64, 0x4000, 0x88004000, 0x4000, 0x4000)
        used = set()
        for path in (host_syntax_check.ROOT / 'port/ios/host').iterdir():
            if path.suffix in ('.c', '.m', '.h'):
                used.update(re.findall(r'\b(?:IOS_GUEST_[A-Z_]+|HALO_GUEST_SHA256|halo_guest_(?:code|data|symbols))\b', path.read_text()))
        self.assertTrue(used)
        for name in sorted(used):
            self.assertIn(name, header)


# Mach-O symbols carry a leading underscore and ELF ones do not, so this runs only where the sweep does.
@unittest.skipUnless(platform.system() == 'Darwin' and shutil.which('clang') and shutil.which('nm'), 'needs a Mac')
class ImportDefinitionCheck(unittest.TestCase):
    """the class: a guest import defined only inside a conditionally compiled file compiles everywhere and
    fails the link where the condition is false"""

    def objects(self, folder, sources):
        import host_syntax_check
        result = []
        for name, text in sources.items():
            source = Path(folder) / f'{name}.c'
            source.write_text(text)
            obj = Path(folder) / f'0-{name}.o'
            subprocess.run(['clang', '-c', '-o', str(obj), str(source)], check=True)
            result.append(obj)
        return host_syntax_check.missing_definitions(result, listed=())

    BRIDGES = 'void host_resolve_import(void) {}\nextern void host_frames_in_flight_scene(void);\nvoid use(void) { host_frames_in_flight_scene(); }\n'

    def test_import_defined_only_under_a_disabled_condition_is_reported(self):
        conditional = '#if HALO_FRAME_PACING\nvoid host_frames_in_flight_scene(void) {}\n#endif\n'
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(self.objects(folder, {'bridges': self.BRIDGES, 'host_x': conditional}),
                             ['host_frames_in_flight_scene'])

    def test_import_defined_unconditionally_passes(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(self.objects(folder, {'bridges': self.BRIDGES,
                                                   'host_x': 'void host_frames_in_flight_scene(void) {}\n'}), [])


if __name__ == '__main__':
    unittest.main()
