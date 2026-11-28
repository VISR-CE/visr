#!/usr/bin/env python3
"""tools/probekit/cc.py: the one probe build function's command, its errors, and a probe it builds running."""
import platform
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probekit import cc


def test_the_defaults_are_gnu11_wall_werror_and_lm_after_the_sources(monkeypatch):
    monkeypatch.setenv('CC', 'clang')
    assert cc.command(['a.c', 'b.c'], output='out') == ['clang', '-std=gnu11', '-Wall', '-Werror', 'a.c', 'b.c',
                                                       '-o', 'out', '-lm']
    assert cc.command(['a.c'], output='out', std='c11', extra_warnings=True, relaxed=('unused-function',),
                      sanitize=True, bits=32, defines=('X=1',), includes=('inc',), libs=('z',),
                      language='c') == [
        'clang', '-std=c11', '-Wall', '-Werror', '-Wextra', '-Wno-unused-function', '-m32',
        '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined', '-DX=1', '-Iinc',
        '-x', 'c', 'a.c', '-x', 'none', '-o', 'out', '-lz', '-lm']


def test_an_apple_build_uses_xcrun_and_a_mac_build_the_strict_wrapper(monkeypatch):
    monkeypatch.delenv('CC', raising=False)
    monkeypatch.delenv('HALO_LINUX_STRICT', raising=False)
    assert cc.command(['a.c'], output='out', apple=True)[:2] == ['xcrun', 'clang']
    monkeypatch.setattr(platform, 'system', lambda: 'Darwin')
    assert cc.command(['a.c'], output='out')[0] == str(cc.STRICT_CLANG)
    monkeypatch.setattr(platform, 'system', lambda: 'Linux')
    assert cc.command(['a.c'], output='out')[0] == 'clang'


def test_flags_replace_every_default_and_keep_their_order():
    flags = ['-O2', '-framework', 'Foundation', '-Iport/ios/host']
    assert cc.command(['port/ios/tests/a_probe.c'], output='out', flags=flags, apple=True) == [
        'xcrun', 'clang', '-O2', '-framework', 'Foundation', '-Iport/ios/host', 'port/ios/tests/a_probe.c',
        '-o', 'out']
    with pytest.raises(ValueError, match='replaces every default'):
        cc.command(['a.c'], output='out', flags=['-O2'], sanitize=True)


def test_host_stubs_match_the_probes_defines_and_standard():
    assert cc.stub_flags(['xcrun', 'clang', '-std=c11', '-O2', '-DB=1', '-DA', 'a.c']) == ['-DA', '-DB=1', '-std=c11']
    with pytest.raises(ValueError, match='needs apple=True'):
        cc.build(['a.c'], output='out', host_stubs=True)


@pytest.mark.skipif(not shutil.which('clang'), reason='needs clang')
def test_a_probe_builds_and_runs_and_a_failure_names_the_command(tmp_path):
    source = tmp_path / 'sqrt_probe.c'
    source.write_text('#include <math.h>\n#include <stdio.h>\nint main(int c, char **v) {\n'
                      '    (void)v; printf("%.1f\\n", sqrt((double)c + 3.0)); return 0;\n}\n')
    probe = cc.build([source], output=tmp_path / 'sqrt-probe', cwd=tmp_path)
    assert subprocess.run([str(probe)], capture_output=True, text=True, check=True).stdout == '2.0\n'
    broken = tmp_path / 'broken_probe.c'
    broken.write_text('int main(void) { int unused; return 0; }\n')
    with pytest.raises(cc.BuildError) as failure:
        cc.build([broken], output=tmp_path / 'broken-probe', cwd=tmp_path)
    message = str(failure.value)
    assert str(tmp_path / 'broken-probe') in message and '-Werror' in message and 'unused' in message


def test_port_stubs_match_the_probes_defines_standard_and_target():
    assert cc_stubs().port_stub_flags(['-std=c11', '-O2', '-DB=1', '-m32', '-arch', 'arm64', 'a.c']) == [
        '-std=c11', '-DB=1', '-m32', '-arch', 'arm64']


def cc_stubs():
    import host_stubs
    return host_stubs


@pytest.mark.skipif(not shutil.which('clang'), reason='needs clang')
def test_port_stubs_give_a_plain_c_probe_the_seams_and_its_own_definition_wins(tmp_path):
    source = tmp_path / 'seam_probe.c'
    source.write_text('#include <stdio.h>\n#include "port_config.h"\nvoid platform_log(const char *format, ...);\n'
                      'long config_integer(const char *name) { (void)name; return 7; }\n'
                      'int main(void) {\n    platform_log("quiet %d", 1);\n'
                      '    printf("%d %ld [%s]\\n", config_boolean("a"), config_integer("b"), config_string("c"));\n'
                      '    return 0;\n}\n')
    probe = cc.build([source], output=tmp_path / 'seam-probe', includes=[cc.ROOT / 'port/linux/src'],
                     port_stubs=True)
    assert subprocess.run([str(probe)], capture_output=True, text=True, check=True).stdout == '0 7 []\n'
