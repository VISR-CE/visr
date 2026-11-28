"""The port's seam stubs (port/ios/tests/port_stubs.c, from tools/host_stubs.py SEAM_HEADERS) follow their headers
and compile as plain C with the compiler Linux-run probes use, so this runs in the Linux job as well as on Macs."""
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
