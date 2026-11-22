"""Adding a cross-port target is data: every port the adapters know or the snapshot holds needs a public name, a release page and a place in the chart order."""
import csv
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(Path(__file__).parent / 'perf_compare'))
import adapters  # noqa: E402
import results  # noqa: E402
import sheet_export  # noqa: E402

SNAPSHOT = ROOT / 'docs/benchmarks/results.csv'
CHARTS = ROOT / 'docs/benchmarks/charts'
ADAPTER_PORTS = set(adapters.OPENCE_CONFIG_PORTS) | set(adapters.WINDOW_SCALE_PORTS) | set(adapters.HOME_CONFIG_DIRS)

# Ports that predate this test and are charted after the ordered ones, sorted by id. Do not add to it: put a new port in `port_order`.
UNORDERED = {
    'iPhone/iPad': {'117', 'beeman', 'chupathingyce-ios', 'halopad', 'snailbbq'},
    'Apple TV': {'beeman', 'chupathingy-ios', 'chupathingyce-ios'},
    'Vision Pro': {'117', 'master-chef'},
}


def snapshot_rows():
    with SNAPSHOT.open(newline='', encoding='utf-8') as stream:
        return list(csv.DictReader(stream))


def mac_rows():
    """The Mac rows come from this registry; the iPhone, Apple TV and Vision Pro rows predate the checks (known-bugs.md, 'port registry')."""
    return [row for row in snapshot_rows() if row['platform'] == 'Mac']


def specs():
    return [tomllib.loads(path.read_text(encoding='utf-8')) | {'name': path.name} for path in sorted(CHARTS.glob('*.chart.toml'))]


def test_every_adapter_and_mac_snapshot_port_has_a_public_name_and_release_page():
    ports = ADAPTER_PORTS | {row['port'] for row in mac_rows()}
    missing = sorted(p for p in ports if p not in results.PORTS or (p not in sheet_export.RELEASE_PAGES and p not in sheet_export.SOURCE_COMMIT_PAGES))
    assert not missing, f'ports without a name or release page: {missing}'


def test_every_released_mac_row_links_the_page_its_port_template_gives():
    wrong = []
    for number, row in enumerate(snapshot_rows(), start=2):
        if row['platform'] != 'Mac' or row['release_kind'] != 'release':
            continue
        expected = sheet_export.release_url(row['port'], row['version'], row['build'])
        if not row['release_url'] or row['release_url'] != expected:
            wrong.append((number, row['label'], row['release_url'], expected))
    assert not wrong, f'rows whose release link is missing or differs from the port template: {wrong[:5]}'


def test_every_charted_port_is_in_each_chart_port_order():
    snapshot = snapshot_rows()
    problems = []
    for spec in specs():
        query = spec['query']
        platform_ports = {row['port'] for row in snapshot if row['platform'] == query['platform']}
        left_out = platform_ports - set(query.get('port_order', [])) - UNORDERED.get(query['platform'], set())
        if left_out:
            problems.append((spec['name'], sorted(left_out)))
    assert not problems, f'ports missing from port_order: {problems}'


def test_charts_of_one_platform_order_their_ports_alike():
    orders = {}
    for spec in specs():
        orders.setdefault(spec['query']['platform'], set()).add(tuple(spec['query'].get('port_order', [])))
    assert all(len(found) == 1 for found in orders.values()), orders


def test_window_scale_ports_ask_for_the_2x_drawable():
    for port in adapters.WINDOW_SCALE_PORTS:
        text = adapters.native_display(port, 'uncapped', 'defaults', '2560x1920')
        assert 'window_scale = 2' in text, port


def test_every_label_names_its_port_and_not_a_device_nickname():
    """A label is the chart series name and is public: the port's name, then the version, then an optional device in parentheses."""
    wrong = []
    for number, row in enumerate(snapshot_rows(), start=2):
        label = row['label']
        if not label.startswith(row['port_name']) or row['version'] not in label:
            wrong.append((number, label))
    assert not wrong, f'labels that are not "<port name> <version>[ (<device>)]": {wrong[:5]}'
