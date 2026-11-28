"""The port's seam stubs (port/ios/tests/port_stubs.c, from tools/host_stubs.py SEAM_HEADERS) follow their headers
and compile as plain C with the compiler Linux-run probes use, and so does the portable part of the host stubs
(port/ios/tests/host_stubs.c outside its __OBJC__ and __APPLE__ parts). Runs in the Linux job as well as on Macs."""
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import host_stubs  # noqa: E402
from probekit import cc  # noqa: E402


def test_port_stubs_are_current():
    found, gone = host_stubs.collect_seams()
    assert not gone, f'SEAM_HEADERS names functions their headers no longer declare: {gone}'
    assert host_stubs.PORT_OUTPUT.read_text() == host_stubs.render_seams(found), 'run tools/host_stubs.py --write'


def test_port_stubs_compile_as_plain_c(tmp_path):
    host_stubs.port_stub_object(tmp_path, cc.compiler(False), ['-std=gnu11'])


def test_host_stubs_portable_part_compiles_as_plain_c(tmp_path):
    """HOST_STUBS_PORTABLE_ONLY leaves the unguarded part on every host, as Linux does (on a Mac the strict wrapper
    refuses headers Ubuntu lacks)."""
    subprocess.run([*cc.compiler(False), '-c', '-x', 'c', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-DHOST_STUBS_PORTABLE_ONLY',
                    *host_stubs.INCLUDES, str(host_stubs.OUTPUT), '-o', str(tmp_path / 'host-stubs.o')],
                   cwd=host_stubs.ROOT, check=True)


def test_every_host_header_is_in_one_part():
    found = host_stubs.collect()
    assert not host_stubs.unlisted_headers(found)
    parts = (host_stubs.OBJC_HEADERS, host_stubs.APPLE_C_HEADERS, host_stubs.PORTABLE_HEADERS)
    assert sum(map(len, parts)) == len(set().union(*parts)), 'a header is in two parts'
    stale = set().union(*parts) - {entry[0] for entry in found.values()}
    assert not stale, f'listed headers that declare no host function (or are gone): {sorted(stale)}'


def test_seam_stubs_follow_their_headers(tmp_path):
    """The class: a hand copy of a production declaration drifts silently. A seam header's declaration (included or
    copied) is regenerated from the header's text, so a changed parameter type or a new function makes the
    checked-in port_stubs.c stale, and a listed function the header lost is reported."""
    (tmp_path / 'included.h').write_text('int seam_value(const char *name);\n')
    (tmp_path / 'copied.h').write_text('#include <xtl.h>\nvoid seam_log(const char *format, ...);\nvoid other(void);\n')
    seams = [{'header': 'included.h', 'includable': True},
             {'header': 'copied.h', 'includable': False, 'functions': ('seam_log', 'seam_gone')}]
    found, gone = host_stubs.collect_seams(tmp_path, seams)
    assert sorted(found) == ['seam_log', 'seam_value'] and gone == ['seam_gone (copied.h)']
    before = host_stubs.render_seams(found)
    assert '#include "included.h"' in before and '#include "copied.h"' not in before
    assert 'void seam_log(const char *format, ...);  /* copied.h */' in before
    (tmp_path / 'copied.h').write_text('#include <xtl.h>\nvoid seam_log(const wchar_t *format, ...);\n')
    assert host_stubs.render_seams(host_stubs.collect_seams(tmp_path, seams)[0]) != before
    (tmp_path / 'included.h').write_text('int seam_value(const char *name);\nint seam_new(void);\n')
    assert 'seam_new' in host_stubs.missing_defaults(host_stubs.collect_seams(tmp_path, seams)[0], before)


def test_stand_ins_follow_production(tmp_path):
    """The class: the menu probes' stand-in headers were hand copies of production declarations, so a production
    change compiled against the old copy. Each item is copied from production text: a changed declaration makes the
    checked-in stand-in stale, and a removed one is reported by name."""
    assert host_stubs.stand_in_problems() == []
    header = tmp_path / 'port/linux/src/port_config.h'
    header.parent.mkdir(parents=True)
    header.write_text('const char *config_string(const char *name);\nlong config_integer(const char *name);\n'
                      'void config_folder(char *path, size_t size);\n')
    spec = host_stubs.STAND_INS['port_config.h']
    before = host_stubs.render_stand_in('port_config.h', spec, tmp_path)
    header.write_text(header.read_text().replace('const char *name);\nlong', 'const wchar_t *name);\nlong'))
    assert host_stubs.render_stand_in('port_config.h', spec, tmp_path) != before
    header.write_text('const char *config_string(const char *name);\n')
    try:
        host_stubs.render_stand_in('port_config.h', spec, tmp_path)
    except LookupError as error:
        assert '`config_integer` is not declared' in str(error)
    else:
        raise AssertionError('a removed declaration was not reported')
