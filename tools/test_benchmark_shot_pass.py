"""debug.benchmark_shot_pass: replay only up to the shot tick, capture once, exit."""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent


def test_keys_are_defined_off_by_default():
    schema = json.loads((ROOT / 'port/settings/config.schema.json').read_text())
    debug = schema['properties']['debug']['properties']
    assert debug['benchmark_shot_pass']['default'] is False
    assert debug['benchmark_shot_tick']['default'] == 0
    readme = (ROOT / 'port/linux/README.md').read_text()
    assert 'benchmark_shot_pass' in readme and 'benchmark_shot_tick' in readme


def test_suite_stops_at_the_shot_tick_and_exits_after_the_capture():
    source = (ROOT / 'port/linux/src/benchmark_suite.c').read_text()
    assert 'config_boolean("debug.benchmark_shot_pass")' in source
    assert 'config_integer("debug.benchmark_shot_tick")' in source
    block = source[source.index('shot pass asks for its picture'):]
    block = block[:block.index('break;')]
    assert 'platform_benchmark_window_shot_request();' in block
    assert 'platform_ready_screenshot_taken()' in source
    assert 'shot pass saved' in source and 'exit(EXIT_SUCCESS)' in source
    skips = source[source.index('benchmark_suite_skips_drawing'):]
    assert 'shot_pass' in skips[:400]
