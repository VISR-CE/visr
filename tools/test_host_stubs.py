#!/usr/bin/env python3
"""The shared weak host stubs (port/ios/tests/host_stubs.c) cover every host header function.

Native Apple probe: it compiles the stub file with -Werror (default, native-Mac and strict C11
settings), fails when a header declares a host function without a weak default, and proves a strong
definition in a probe overrides the default while an unstubbed call links to it.
"""
import json
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import host_stubs

ROOT = host_stubs.ROOT
STUBS = host_stubs.OUTPUT


INCLUDES = host_stubs.INCLUDES


def ast_functions(header: Path) -> set[str]:
    """Exported (non-static, non-inline, body-less) functions clang sees declared in this header itself."""
    dump = subprocess.run(['xcrun', 'clang', '-fsyntax-only', '-x', 'objective-c', '-fblocks', '-Xclang',
                           '-ast-dump=json', *INCLUDES, '-include', str(header), '/dev/null'],
                          cwd=ROOT, check=True, stdout=subprocess.PIPE, text=False).stdout
    tree = json.loads(dump)
    current = ''
    found = set()
    for node in tree['inner']:
        loc = node.get('loc', {})
        place = loc.get('expansionLoc', loc)
        if 'file' in place:
            current = place['file']
        if (node.get('kind') == 'FunctionDecl' and current.endswith('/' + header.name)
                and not node['name'].startswith('__') and node.get('storageClass') != 'static' and not node.get('inline')
                and not any(child.get('kind') == 'CompoundStmt' for child in node.get('inner', []))):
            found.add(node['name'])
    return found


def compile_stubs(output: Path, *flags: str) -> None:
    subprocess.run(['xcrun', 'clang', '-c', '-x', 'objective-c', '-fno-objc-arc', '-fblocks', '-Wall', '-Wextra',
                    '-Werror', *INCLUDES, *flags, str(STUBS), '-o', str(output)], cwd=ROOT, check=True)


def main() -> None:
    subprocess.run([sys.executable, 'tools/host_stubs.py', '--check'], cwd=ROOT, check=True)
    with tempfile.TemporaryDirectory(prefix='host-stubs-') as scratch:
        scratch = Path(scratch)
        # Every define set a probe is built with must still compile against the real headers.
        for index, flags in enumerate(([], ['-DHALO_NATIVE_MAC=1'], ['-DHALO_IOS=1'], ['-DHALO_TESTER_BUILD=1'])):
            compile_stubs(scratch / f'stubs{index}.o', *flags)

        # Ground truth from clang, not from the scanner under test: every exported function any host
        # header declares has a weak default (headers that need UIKit are the documented exclusions).
        defined = host_stubs.defined_names(STUBS.read_text())
        for header in sorted(host_stubs.HOST.glob('*.h')):
            if header.name in host_stubs.SKIP_HEADERS:
                continue
            missing = sorted(ast_functions(header) - defined)
            assert not missing, f'{header.name}: no weak default for {missing}'

        # A header function without a weak default fails the check.
        fake_host = scratch / 'host'
        fake_host.mkdir()
        (fake_host / 'host_fake.h').write_text('#pragma once\nint host_brand_new_call(int value);\n')
        found = host_stubs.collect(fake_host)
        assert list(found) == ['host_brand_new_call'], found
        assert host_stubs.missing_defaults(found, STUBS.read_text()) == ['host_brand_new_call']
        assert host_stubs.missing_defaults(host_stubs.collect(), STUBS.read_text()) == []

        # Scanner fixtures: extern "C", #if/#else/#elif, attributes, multi-line, block and function
        # pointer parameters, unnamed parameters, noreturn, a non-void default that must abort.
        (fake_host / 'host_fake.h').write_text(textwrap.dedent('''
            #pragma once
            #include <stddef.h>
            #ifdef __cplusplus
            extern "C" {
            #endif
            int host_fix_plain(int value);
            void host_fix_multi(const char *name,
                                unsigned long size);
            #if defined(HALO_NATIVE_MAC)
            int host_fix_mac(void);
            #elif defined(HALO_IOS)
            int host_fix_ios(void);
            #else
            int host_fix_other(void);
            #endif
            void host_fix_callback(int (*handler)(int), void (^done)(int), int *);
            unsigned host_fix_unnamed(unsigned, const char *);
            void host_fix_fatal(const char *format, ...) __attribute__((format(printf, 1, 2), noreturn));
            static inline int host_fix_inline(void) { return 1; }
            #ifdef __cplusplus
            }
            #endif
            '''))
        found = host_stubs.collect(fake_host)
        expected = {'host_fix_plain', 'host_fix_multi', 'host_fix_mac', 'host_fix_ios', 'host_fix_other',
                    'host_fix_callback', 'host_fix_unnamed', 'host_fix_fatal'}
        assert set(found) == expected, sorted(set(found) ^ expected)
        fixture_stub = scratch / 'fixture_stubs.c'
        fixture_stub.write_text(host_stubs.render(found))
        for flags in ([], ['-DHALO_NATIVE_MAC=1'], ['-DHALO_IOS=1']):
            subprocess.run(['xcrun', 'clang', '-c', '-x', 'objective-c', '-fno-objc-arc', '-fblocks', '-Wall',
                            '-Wextra', '-Werror', f'-I{fake_host}', *flags, str(fixture_stub),
                            '-o', str(scratch / 'fixture.o')], cwd=ROOT, check=True)

        # A strong definition in the probe wins; a call it does not define gets the weak default.
        probe = scratch / 'override.c'
        probe.write_text(textwrap.dedent('''
            #include <stdio.h>
            #include <string.h>
            #include "host.h"
            #include "host_config.h"
            #include "host_external.h"
            #include "ios_host.h"
            int host_external_active(void) { return 7; }
            int main(void) {
                char text[16] = "x";
                host_config_string("any.key", "fallback", text, sizeof text);
                if (host_external_active() != 7) return 1;
                if (strcmp(text, "fallback")) return 2;
                if (host_window_up() != 0) return 3;
                host_logf(0, "ignored %d", 1);
                return 0;
            }
        '''))
        executable = scratch / 'override'
        subprocess.run(['xcrun', 'clang', '-std=c11', '-Wall', '-Wextra', '-Werror', *INCLUDES, str(probe),
                        str(scratch / 'stubs0.o'), '-o', str(executable)], cwd=ROOT, check=True)
        subprocess.run([str(executable)], check=True)
    print('host stubs ok')


if __name__ == '__main__':
    main()
