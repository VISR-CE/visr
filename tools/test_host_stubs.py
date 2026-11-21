#!/usr/bin/env python3
"""The shared weak host stubs (port/ios/tests/host_stubs.c) cover every host header function.

Native Apple probe: it compiles the stub file with -Werror (default, native-Mac and strict C11
settings), fails when a header declares a host function without a weak default, and proves a strong
definition in a probe overrides the default while an unstubbed call links to it.
"""
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import host_stubs

ROOT = host_stubs.ROOT
STUBS = host_stubs.OUTPUT
INCLUDES = ['-Iport/ios/host', '-Iport/runtime', '-Iport/linux/src', '-Iport/runtime/include', '-Iport/ios']


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

        # A header function without a weak default fails the check.
        fake_host = scratch / 'host'
        fake_host.mkdir()
        (fake_host / 'host_fake.h').write_text('#pragma once\nint host_brand_new_call(int value);\n')
        found = host_stubs.collect(fake_host)
        assert list(found) == ['host_brand_new_call'], found
        assert host_stubs.missing_defaults(found, STUBS.read_text()) == ['host_brand_new_call']
        assert host_stubs.missing_defaults(host_stubs.collect(), STUBS.read_text()) == []

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
