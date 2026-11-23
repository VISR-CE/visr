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


PROGRAM = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../port/linux/game/player_input_profile.c"
#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4
#define csmemcpy memcpy
#define match_assert(...) ((void)0)
typedef int boolean;
#define TRUE 1
#define FALSE 0
struct player_profile { int record; };
static struct
{
	struct { struct player_profile profile; long active_profile_index; } local_players[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
} player_ui_globals;
static long last_index = -2;
static void set_local_player_controls_from_player_profile(short local_player_index) { (void)local_player_index; }

/* the saved profiles as the mapfile lists them: an index is a position, and a delete moves the later ones up */
static struct player_input_profile files[16];
static int count;
static long profile_create(const struct player_input_profile *input)
{
	files[count] = *input;
	return count++;
}
static void profile_delete(long index)
{
	int i;
	for (i = (int)index; i + 1 < count; i++)
		files[i] = files[i + 1];
	count--;
}
boolean player_profile_get_input(long index, struct player_input_profile *input)
{
	if (index < 0 || index >= count)
		return 0;
	*input = files[index];
	return 1;
}
/* FUNCTION */
int main(void)
{
	struct player_profile record = { 0 };
	struct player_input_profile a, b, defaults;
	long index_a, index_b;

	player_input_profile_defaults(&defaults);
	a = defaults; a.mouse_sensitivity = 300; a.invert_mouse = 1; a.bindings[6][0] = 33;
	b = defaults; b.mouse_sensitivity = 175; b.mouse_aim_assist = 1;

	/* a profile deleted while active, then a new one created at its index */
	index_a = profile_create(&a);
	player_ui_set_active_player_profile(0, index_a, &record);
	assert(player_input_profile_for_local_player(0)->mouse_sensitivity == 300);
	profile_delete(index_a);
	index_b = profile_create(&defaults);
	assert(index_b == index_a);
	player_ui_set_active_player_profile(0, index_b, &record);
	assert(!memcmp(player_input_profile_for_local_player(0), &defaults, sizeof(defaults)));

	/* the one after it moves up into the deleted profile's index */
	count = 0;
	index_a = profile_create(&a);
	index_b = profile_create(&b);
	player_ui_set_active_player_profile(0, index_a, &record);
	profile_delete(index_a);
	player_ui_set_active_player_profile(0, index_a, &record);
	assert(!memcmp(player_input_profile_for_local_player(0), &b, sizeof(b)));
	/* another local player keeps their own */
	player_ui_set_active_player_profile(1, index_a, &record);
	assert(player_input_profile_for_local_player(1)->mouse_sensitivity == 175);
	assert(player_input_profile_for_local_player(2)->mouse_sensitivity == 100);
	return 0;
}
'''


def _function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def _activation_status(tmp_path, ui_source, mutate=None):
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required')
    function = _function(ui_source.replace('\r\n', '\n'), 'void player_ui_set_active_player_profile(')
    if mutate:
        function = mutate(function)
    folder = tmp_path / 'probe'
    folder.mkdir()
    program = folder / 'probe.c'
    program.write_text(PROGRAM.replace('/* FUNCTION */', function).replace('"../../port/', '"' + str(ROOT) + '/port/'))
    binary = folder / 'probe'
    subprocess.run([compiler, '-std=gnu99', '-Wall', '-Wno-unused-function', '-fsanitize=address,undefined',
                    str(program), '--output=' + str(binary)], check=True)
    return subprocess.run([str(binary)], capture_output=True).returncode


def test_a_reused_profile_index_gets_its_own_input_settings(tmp_path):
    """Behavior of the real activation function: delete the active profile, create another at the reused index (or
    let the next one move up into it) and activate it: it plays with its own settings, not the deleted one's."""
    ui = (ROOT / 'source/interface/player_ui.c').read_text(encoding='latin-1')
    assert _activation_status(tmp_path, ui) == 0


def test_the_reused_index_probe_catches_a_skipped_read(tmp_path):
    ui = (ROOT / 'source/interface/player_ui.c').read_text(encoding='latin-1')

    def skip_unchanged_index(function):
        read = 'player_profile_get_input(profile_index, &input);'
        assert read in function
        return function.replace(read, 'if (profile_index != last_index) { ' + read + ' last_index = profile_index; }')

    assert _activation_status(tmp_path, ui, skip_unchanged_index) != 0
