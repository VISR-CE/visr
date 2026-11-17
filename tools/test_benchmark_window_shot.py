"""debug.benchmark_window_shot: one capture after a benchmark window's timer stops, never periodic."""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent


def test_key_is_defined_off_by_default():
    schema = json.loads((ROOT / 'port/settings/config.schema.json').read_text())
    entry = schema['properties']['debug']['properties']['benchmark_window_shot']
    assert entry['type'] == 'string'
    assert entry['default'] == ''
    assert 'benchmark_window_shot' in (ROOT / 'port/linux/README.md').read_text()


def test_request_follows_the_timer_stop_in_host_benchmark_end():
    source = (ROOT / 'port/ios/host/host_benchmark.m').read_text()
    body = source[source.index('void host_benchmark_end('):]
    closed = body.index('window_open = 0;')
    request = body.index('platform_benchmark_window_shot_request();')
    assert closed < request < closed + 400


def test_capture_is_one_shot_on_the_ready_path():
    source = (ROOT / 'port/linux/src/d3d8_device.c').read_text()
    request = source[source.index('void platform_benchmark_window_shot_request'):]
    request = request[:request.index('\n}\n')]
    assert 'config_string("debug.benchmark_window_shot")' in request
    assert 'platform_ready_screenshot_request_named("window-end")' in request
    assert 'screenshot_every' not in request
