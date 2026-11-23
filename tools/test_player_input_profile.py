"""Input settings are the player profile's (port/linux/game/player_input_profile.c).

The classes these tests guard: an input setting read from the per-device config instead of the player's
profile; a profile write that drops the input extension; a menu row for an input setting the profile does not
hold.
"""
import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
MOVED_KEYS = re.compile(r'"(?:input\.(?:mouse_[a-z_]+|invert_mouse)|controls\.[a-z_]+)"')


def test_codec_probe_passes(tmp_path):
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required')
    binary = tmp_path / 'probe'
    subprocess.run([compiler, '-std=c99', '-Wall', '-Werror', '-fsanitize=address,undefined',
                    str(ROOT / 'port/ios/tests/player_input_profile_probe.c'), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_codec_is_plain_gnu89_for_the_guest_build(tmp_path):
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required')
    subprocess.run([compiler, '-std=gnu89', '-Wall', '-Wdeclaration-after-statement', '-Werror', '-c',
                    str(ROOT / 'port/linux/game/player_input_profile.c'), '--output=' + str(tmp_path / 'o')], check=True)


def test_no_input_code_reads_a_moved_setting_from_the_config():
    """Every reader of a moved input setting goes through the profile: no config_*() call names one."""
    sources = [path for folder in ('port', 'source') for pattern in ('*.c', '*.m', '*.h')
               for path in (ROOT / folder).rglob(pattern) if '/tests/' not in path.as_posix()]
    assert len(sources) > 100
    for path in sources:
        text = path.read_text(encoding='latin-1')
        for call in re.findall(r'config_(?:boolean|integer|real|string|text|default|write|write_boolean|reload_boolean)\(\s*("[^"]*")', text):
            assert not MOVED_KEYS.fullmatch(call), f'{path} reads {call} from the device config'
    config = (ROOT / 'port/linux/src/port_config.c').read_text()
    assert not re.search(r'^\t\{ ' + MOVED_KEYS.pattern, config, re.M)


def test_every_profile_input_setting_has_a_menu_row_and_every_row_a_setting():
    codec = (ROOT / 'port/linux/game/player_input_profile.c').read_text()
    fields = set(re.findall(r'\{ "(profile\.[a-z_]+)", [01], offsetof', codec))
    rows = set(re.findall(r'"(profile\.(?:mouse_[a-z_]+|invert_mouse))"', (ROOT / 'tools/port_settings.py').read_text()))
    assert fields and fields == rows


def test_the_write_thread_builds_its_block_with_the_merge_the_probe_tests():
    """The write thread's block comes from player_input_block_merge (behavior: port/ios/tests/player_input_profile_probe.c,
    which covers a base-only write, an extension-only write, an unreadable file and a newer record)."""
    source = (ROOT / 'source/saved games/player_profile.c').read_text(encoding='latin-1')
    thread = source[source.index('player_profile_write_thread_proc(\n\tvoid *input)\n{'):]
    thread = thread[:thread.index('player_profile_create_default_profiles_on_disk(\n\tvoid)\n{')]
    assert 'player_input_block_merge(' in thread and 'file_write(&file, sizeof(new_block), new_block)' in thread
    assert 'player_profile_input_request.write_input ? &player_profile_input_request.input : NULL' in thread
    # the default profiles, rewritten at every launch, keep their extension as well
    defaults = source[source.index('player_profile_create_default_profiles_on_disk(\n\tvoid)\n{'):]
    assert 'PLAYER_INPUT_REGION_OFFSET' in defaults[:defaults.index('player_profile_write(\n\tlong')]


def test_activating_a_profile_always_reads_its_input_settings():
    """A profile's index can be reused after a delete, so activation never skips the read; only the two level-save
    paths (same profile, record only) use the record-only update."""
    ui = (ROOT / 'source/interface/player_ui.c').read_text(encoding='latin-1')
    activate = ui[ui.index('void player_ui_set_active_player_profile('):]
    activate = activate[:activate.index('\n}')]
    assert 'player_profile_get_input(profile_index, &input);' in activate and 'if (' not in activate.split('csmemcpy')[1]
    update = ui[ui.index('void player_ui_update_active_player_profile_record('):]
    assert 'player_profile_get_input' not in update[:update.index('\n}')]
    saves = (ROOT / 'source/saved games/player_profile.c').read_text(encoding='latin-1')
    assert saves.count('player_ui_update_active_player_profile_record(') == 2
    assert 'player_ui_set_active_player_profile(' not in saves
