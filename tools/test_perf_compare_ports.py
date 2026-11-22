"""Adding a cross-port target is data: every Mac target the adapters know needs a public name and a release page."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / 'perf_compare'))
import adapters  # noqa: E402
import results  # noqa: E402
import sheet_export  # noqa: E402

KNOWN = set(adapters.OPENCE_CONFIG_PORTS) | set(adapters.WINDOW_SCALE_PORTS) | set(adapters.HOME_CONFIG_DIRS)


def test_every_adapter_port_has_a_public_name_and_release_page():
    missing = sorted(p for p in KNOWN if p not in results.PORTS or p not in sheet_export.RELEASE_PAGES)
    assert not missing, f'ports without a name or release page: {missing}'


def test_window_scale_ports_ask_for_the_2x_drawable():
    for port in adapters.WINDOW_SCALE_PORTS:
        text = adapters.native_display(port, 'uncapped', 'defaults', '2560x1920')
        assert 'window_scale = 2' in text, port


def test_release_pages_resolve_the_tag_of_each_new_port():
    pages = sheet_export.RELEASE_PAGES
    assert pages['opence-bitmoda'].format(tag='v0.3.1', bare='0.3.1', build='1').endswith('/tag/opence-build-1')
    assert pages['meshce'].format(tag='v0.3.77', bare='0.3.77', build='77').endswith('/tag/build-77')
    assert pages['halo-content-evolved'].format(tag='v144', bare='144', build='144').endswith('/tag/build-144')
