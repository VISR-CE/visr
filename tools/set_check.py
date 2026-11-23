#!/usr/bin/env python3
"""Check `--set SECTION.KEY=VALUE` assignments against the settings the game defines.

The game quietly falls back to a default for a value it does not accept (debug.encode_thread = "on" runs as
"off"), so a run launched with one measures something other than what was asked. Every runner that takes --set
calls `problems()` before it launches and refuses on any answer. Keys and types come from the C setting table
(settings_schema.config_settings), a string's accepted names from the table's choices or the profile table, and a
profile number's range from the profile table; the value rules are profile_check.value_problem's.

    python3 tools/set_check.py display.renderer=metal debug.encode_thread='"on"'
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import profile_check
import settings_schema

# string settings that take any text (paths, lists, free-form specs): reviewed, not forgotten
# (tools/test_set_check.py fails for a string setting that is neither closed nor listed here)
FREE_FORM = frozenset("""
display.resolution display.window_size debug.orientation debug.test_controller debug.controller_record
debug.controller_replay debug.controller_aim_axes display.ui_redraw_pictures display.scoreboard_background_color
network.address network.broadcast network.delta_url network.brokers_file network.stun_servers discord.application_id
debug.external_mode debug.knob_cycle_start debug.benchmark_staging debug.input_record debug.language
debug.benchmark_suite debug.input_replay debug.network_test debug.network_test_pickup_weapon debug.network_test_weapon
debug.controller_glyph_family debug.test_input debug.side_by_side_tangents debug.side_by_side_head_yaw
debug.head_sweep debug.test_text_answer debug.render_capture debug.render_capture_frames
debug.render_validation_allow debug.shader_list_record debug.menu_open debug.menu_sequence debug.menu_error
debug.menu_text_seed debug.gpu_skip_vertex_shaders debug.gpu_dump_shaders debug.gpu_shader_replay
debug.gpu_debug_expression debug.benchmark_window_shot debug.screenshot_directory debug.screenshot_frames
debug.texture_dump_directory debug.texture_override_directory debug.texture_compare_directory
debug.texture_override_name debug.texture_compare_name debug.device_profile
paths.data paths.saves paths.custom_edition
""".split())
INTEGER = re.compile(r'^[+-]?\d+$')
NUMBER = re.compile(r'^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$')


def parse(value, kind, bare_strings):
    """(the value as Python, None) or (None, why not); a string is quoted, or a bare word where bare_strings"""
    if kind == 'boolean':
        return (value == 'true', None) if value in ('true', 'false') else (None, 'is not true or false')
    if kind == 'integer':
        return (int(value), None) if INTEGER.match(value) else (None, 'is not an integer')
    if kind == 'number':
        return (float(value), None) if NUMBER.match(value) else (None, 'is not a number')
    if value[:1] in ('"', "'"):
        try:
            text = json.loads(value) if value[0] == '"' else value[1:-1]
        except ValueError:
            return None, 'is not a valid quoted string'
        if len(value) < 2 or value[-1] != value[0]:
            return None, 'is not a valid quoted string'
        return text, None
    # (mac_run.py writes a bare number as a TOML number, which is not a string: it needs quotes too)
    if bare_strings and value[:1] not in ('[', '{') and not NUMBER.match(value):
        return value, None
    return None, 'needs quotes, as "' + value + '"'


def problem(assignment, settings=None, bare_strings=False):
    """why one KEY=VALUE assignment is not accepted, else None. bare_strings: an unquoted word is a string
    (mac_run.py quotes it); a runner that writes the value as given must have it quoted."""
    settings = settings_schema.config_settings() if settings is None else settings
    if '=' not in assignment:
        return f'{assignment!r}: expected SECTION.KEY=VALUE'
    name, value = (part.strip() for part in assignment.split('=', 1))
    if name not in settings:
        near = [other for other in settings if other.split('.')[-1] == name.split('.')[-1] or other.startswith(name[:8])]
        return f'{name}: not a setting' + (f' (did you mean {", ".join(sorted(near)[:4])}?)' if near else '')
    kind = settings[name]['type']
    parsed, why = parse(value, kind, bare_strings)
    if why:
        return f'{name}: {value} {why}'
    names = settings_schema.setting_choices(name, settings) if kind == 'string' else None
    profile = profile_check.profile_keys().get(name)
    if names:
        key = ('string', None, None, names)
    elif profile and kind in ('integer', 'number'):
        key = profile
    else:
        return None
    why = profile_check.value_problem(name, parsed, key)
    return f'{name}: {value} {why}' if why else None


def problems(assignments, bare_strings=False):
    """every refusal for a list of KEY=VALUE assignments (empty when all are accepted)"""
    settings = settings_schema.config_settings()
    found = (problem(item, settings, bare_strings) for item in assignments)
    return [text for text in found if text]


def main(argv):
    found = problems(argv, bare_strings=True)
    for text in found:
        print(f'--set: {text}', file=sys.stderr)
    return 1 if found else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
