import subprocess
import sys
from pathlib import Path

import pytest

import profile_check
import set_check
import settings_schema

TOOLS = Path(__file__).resolve().parent


def refusal(assignment, bare_strings=False):
    found = set_check.problems([assignment], bare_strings=bare_strings)
    assert len(found) == 1, (assignment, found)
    return found[0]


@pytest.mark.parametrize('assignment', [
    'debug.encode_thread="thread"', 'display.renderer="metal"', 'display.renderer=metal',
    'display.frame_rate="60"', 'display.frame_rate="unlimited"', 'display.vsync=false', 'debug.exit_after=25.0',
    'display.render_height=720', 'debug.benchmark_offscreen_mode="pipelined"', 'display.frame_source=""',
    'display.frames_in_flight="system"', 'debug.texture_astc="force"', 'display.graphics="hardcore"',
    'debug.menu_open="a_screen"'])
def test_accepted(assignment):
    assert set_check.problems([assignment], bare_strings=True) == []


# (assignment, a word the refusal must contain): each case is refused for the reason it names
@pytest.mark.parametrize('assignment, reason', [
    ('debug.encode_thread="on"', 'must be one of "off", "inline", "thread"'),
    ('debug.encode_thread=thread', 'needs quotes'),
    ('debug.encode_threads="thread"', 'not a setting'),
    ('display.vsync=yes', 'is not true or false'),
    ('display.render_height=high', 'is not an integer'),
    ('display.render_height=-99999', 'must be from 0 to 4320'),
    ('debug.exit_after=soon', 'is not a number'),
    ('display.renderer="vulkan"', 'must be one of "gl", "metal"'),
    ('display.frame_rate="fast"', 'a number from 30 to 1000'),
    ('display.frame_rate="5"', 'a number from 30 to 1000'),
    ('display.graphics="ultra"', 'must be one of'),
    ('debug.benchmark_offscreen_mode="piplined"', '"serialized", "pipelined"'),
    ('display.frame_source="sytem"', '"clock", "system"'),
    ('debug.shadow_blur="on"', '"footprint", "xbox"'),
    ('display.frames_in_flight="9"', '"system"'),
    ('debug.texture_astc="forced"', '"auto", "force", "off"'),
    ('display.mode="full"', '"fullscreen"'),
    ('debug.menu_open="x', 'not a valid quoted string'),
    ('noequals', 'expected SECTION.KEY=VALUE')])
def test_refused(assignment, reason):
    assert reason in refusal(assignment)


def test_bare_number_is_not_a_string():
    assert 'needs quotes' in refusal('display.frame_rate=60', bare_strings=True)


def test_unknown_key_suggests_near_names():
    assert 'debug.encode_thread' in refusal('display.encode_thread="thread"')


def test_every_string_setting_is_closed_or_reviewed_free_form():
    settings = settings_schema.config_settings()
    strings = {name for name, item in settings.items() if item['type'] == 'string'}
    closed = {name for name in strings if settings_schema.setting_choices(name, settings)}
    assert strings - closed - set_check.FREE_FORM == set(), 'give each a choices list in port_config.c, or review it into FREE_FORM'
    assert set_check.FREE_FORM & closed == set(), 'a closed setting is not free-form'
    assert set_check.FREE_FORM - strings == set(), 'FREE_FORM names a setting that is not a string setting'


def test_choices_have_one_source():
    settings = settings_schema.config_settings()
    own = {name for name, item in settings.items() if item.get('x-choices')}
    assert own & set(profile_check.profile_keys()) == set(), 'a profile setting takes its choices from port_profile_keys.h'
    for name in own:
        assert settings[name]['type'] == 'string'
        names = settings[name]['x-choices'].split('|')
        assert len(names) == len(set(names)) and any(names), name
        assert settings[name]['default'] in names, f'{name}: the default is not one of its choices'


def test_schema_carries_the_choices():
    config = settings_schema.config_schema(settings_schema.config_settings())
    assert config['properties']['debug']['properties']['encode_thread']['enum'] == ['off', 'inline', 'thread']


def test_command_line_exit_status():
    run = lambda *args: subprocess.run([sys.executable, str(TOOLS / 'set_check.py'), *args], capture_output=True, text=True)
    assert run('display.renderer=metal').returncode == 0
    refused = run('debug.encode_thread="on"')
    assert refused.returncode == 1 and 'must be one of' in refused.stderr


def test_mac_run_refuses_before_launching(tmp_path):
    result = subprocess.run([sys.executable, str(TOOLS / 'mac_run.py'), 'run', '--out', str(tmp_path / 'out'),
                             '--set', 'debug.encode_thread="on"'], capture_output=True, text=True)
    assert result.returncode != 0 and 'must be one of "off", "inline", "thread"' in result.stderr
    assert not (tmp_path / 'out').exists()
