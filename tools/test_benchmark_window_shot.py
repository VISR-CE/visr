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


def test_request_is_made_by_the_guest_after_the_timer_stop():
    host = (ROOT / 'port/ios/host/host_benchmark.m').read_text()
    assert 'platform_benchmark_window_shot_request' not in host
    source = (ROOT / 'port/linux/src/benchmark_suite.c').read_text()
    body = source[source.index('static void finish('):]
    closed = body.index('benchmark_graphics_window(0,0,0)')
    ended = body.index('host_benchmark_end(status, note);')
    request = body.index('platform_benchmark_window_shot_request();')
    assert closed < ended < request < ended + 300


def test_capture_is_one_shot_on_the_ready_path():
    source = (ROOT / 'port/linux/src/d3d8_device.c').read_text()
    request = source[source.index('void platform_benchmark_window_shot_request'):]
    request = request[:request.index('\n}\n')]
    assert 'config_string("debug.benchmark_window_shot")' in request
    assert 'platform_ready_screenshot_request_named("window-end")' in request
    assert 'screenshot_every' not in request
