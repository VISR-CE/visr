"""Input settings are the player profile's (port/linux/game/player_input_profile.c).

The classes these tests guard: an input setting read from the per-device config instead of the player's
profile; a profile write that drops the input extension; a menu row for an input setting the profile does not
hold.
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from probekit import source as probe_source  # noqa: E402
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
    thread = probe_source.function_in(source, 'player_profile_write_thread_proc')
    assert 'player_input_block_merge(' in thread and 'file_write(&file, sizeof(new_block), new_block)' in thread
    assert 'player_profile_input_request.write_input ? &player_profile_input_request.input : NULL' in thread
    # the default profiles, rewritten at every launch, keep their extension as well
    defaults = probe_source.function_in(source, 'player_profile_create_default_profiles_on_disk')
    assert 'PLAYER_INPUT_REGION_OFFSET' in defaults


def test_activating_a_profile_always_reads_its_input_settings():
    """A profile's index can be reused after a delete, so activation never skips the read; only the two level-save
    paths (same profile, record only) use the record-only update."""
    ui = (ROOT / 'source/interface/player_ui.c').read_text(encoding='latin-1')
    activate = probe_source.function_in(ui, 'player_ui_set_active_player_profile')
    assert 'player_profile_get_input(profile_index, &input);' in activate and 'if (' not in activate.split('csmemcpy')[1]
    update = probe_source.function_in(ui, 'player_ui_update_active_player_profile_record')
    assert 'player_profile_get_input' not in update
    saves = (ROOT / 'source/saved games/player_profile.c').read_text(encoding='latin-1')
    assert saves.count('player_ui_update_active_player_profile_record(') == 2
    assert 'player_ui_set_active_player_profile(' not in saves


PROGRAM = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "../../port/linux/game/player_input_profile.c"
#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4
#define NONE (-1)
#define csmemcpy memcpy
#define match_assert(...) ((void)0)
typedef int boolean;
#define TRUE 1
#define FALSE 0
struct player_profile { int record; };
static struct
{
	struct { struct player_profile profile; long active_profile_index; } local_players[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	long edit_profile_index;
} player_ui_globals;
static long player1_last_used_profile_index = NONE;
static char player1_profile_path[0x100];
#define _saved_game_file_type_player_profile 1
#define SAVED_GAME_FILE_INDEX_MEMORY_UNIT(profile_index) (((profile_index)>>8)&0xFF)
#define SAVED_GAME_FILE_INDEX_FILE_INDEX(profile_index) (((profile_index)>>16)&0xFFF)
#define SAVED_GAME_FILE_INDEX_FILE_INDEX_SHIFT 16
static long last_index = -2;
static void set_local_player_controls_from_player_profile(short local_player_index) { (void)local_player_index; }

/* the saved profiles as the mapfile lists them: an index holds the position (saved_game_files.c's
SAVED_GAME_FILE_INDEX_BUILD: position << 16, memory unit << 8, type in the low bits, the valid bit above), and a
delete closes the list up */
#define INDEX_OF(n) ((((long)(n)) << 16) | 0x40000000L)
static struct player_input_profile files[16];
static int count;
static int file_id[16], next_id;
static long profile_create(const struct player_input_profile *input)
{
	files[count] = *input;
	file_id[count] = ++next_id;
	return INDEX_OF(count++);
}
boolean player_profile_get_input(long index, struct player_input_profile *input)
{
	int n = (int)((index >> 16) & 0xFFF);
	if (index == NONE || n >= count)
		return 0;
	*input = files[n];
	return 1;
}
/* the profile's directory path names it across a re-listing (the id stands for the directory) */
static void path_of(int n, char *path) { snprintf(path, 32, "p%d", file_id[n]); }
long saved_game_file_get_type(long index) { (void)index; return _saved_game_file_type_player_profile; }
boolean player_profile_get_enclosing_directory_path(long index, char *path)
{
	int n = (int)((index >> 16) & 0xFFF);
	if (index == NONE || n >= count)
		return 0;
	path_of(n, path);
	return 1;
}
boolean saved_game_file_get_path_to_enclosing_directory(long index, char *path)
{
	return player_profile_get_enclosing_directory_path(index, path);
}
long saved_game_file_find_profile_index_for_directory_path(const char *path, short type)
{
	int n;
	char other[32];
	(void)type;
	for (n = 0; n < count; n++)
	{
		path_of(n, other);
		if (!strcmp(path, other))
			return INDEX_OF(n);
	}
	return NONE;
}
boolean saved_game_file_retrieve_player1_last_used_profile_directory(char *path) { (void)path; return 0; }
/* FUNCTIONS */
/* delete_enumerated_saved_game_file: the entry goes and the game's thread is told (it applies the post at its next
tick, or before it reads or sets a profile) */
static void profile_remove(long index)
{
	int i, n = (int)((index >> 16) & 0xFFF);
	for (i = n; i + 1 < count; i++)
	{
		files[i] = files[i + 1];
		file_id[i] = file_id[i + 1];
	}
	count--;
}
/* enumerate_memory_units: the listing comes back in another order (here: reversed) and says so */
static void profile_relist_reversed(void)
{
	int i;
	for (i = 0; i < count / 2; i++)
	{
		struct player_input_profile swap = files[i];
		int swap_id = file_id[i];
		files[i] = files[count - 1 - i]; file_id[i] = file_id[count - 1 - i];
		files[count - 1 - i] = swap; file_id[count - 1 - i] = swap_id;
	}
	player_ui_post_saved_game_files_relisted();
}
static void reset_players(void)
{
	int p;
	count = 0;
	memset(&player_ui_globals, 0, sizeof(player_ui_globals));
	for (p = 0; p < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; p++)
		player_ui_globals.local_players[p].active_profile_index = NONE;
	player_ui_globals.edit_profile_index = NONE;
	player1_last_used_profile_index = NONE;
	player1_profile_path[0] = 0;
}
static void profile_delete(long index)
{
	profile_remove(index);
	player_ui_post_saved_game_file_deleted(index);
}
static void *delete_on_another_thread(void *argument)
{
	profile_delete(*(long *)argument);
	return NULL;
}
static long active_of(int player) { return player_ui_globals.local_players[player].active_profile_index; }
int main(void)
{
	struct player_profile record = { 0 };
	struct player_input_profile a, b, c, defaults;
	long index_a, index_b, index_c;

	player_input_profile_defaults(&defaults);
	a = defaults; a.mouse_sensitivity = 300; a.invert_mouse = 1; a.bindings[6][0] = 33;
	b = defaults; b.mouse_sensitivity = 175; b.mouse_aim_assist = 1;
	c = defaults; c.mouse_sensitivity = 60;

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
	player_ui_globals.local_players[0].active_profile_index = NONE;
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

	/* delete a profile two local players do not play with, save another: nobody holds the deleted profile's
	input, and a player's active index still names their own profile after the list closes up */
	count = 0;
	memset(&player_ui_globals, 0, sizeof(player_ui_globals));
	player_ui_globals.local_players[0].active_profile_index = NONE;
	player_ui_globals.local_players[1].active_profile_index = NONE;
	player_ui_globals.local_players[2].active_profile_index = NONE;
	player_ui_globals.local_players[3].active_profile_index = NONE;
	index_a = profile_create(&a);
	index_b = profile_create(&b);
	index_c = profile_create(&c);
	player_ui_set_active_player_profile(0, index_a, &record);
	player_ui_set_active_player_profile(1, index_b, &record);
	player_ui_set_active_player_profile(2, index_c, &record);
	profile_delete(index_a);
	player_ui_apply_pending_saved_game_file_deletions();
	/* the player on the deleted profile has none and the defaults */
	assert(active_of(0) == NONE);
	assert(!memcmp(player_input_profile_for_local_player(0), &defaults, sizeof(defaults)));
	/* the others' indices moved down with their profiles */
	assert(active_of(1) == INDEX_OF(0) && active_of(2) == INDEX_OF(1));
	assert(!memcmp(player_input_profile_for_local_player(1), &b, sizeof(b)));
	assert(!memcmp(player_input_profile_for_local_player(2), &c, sizeof(c)));
	/* saving the profile now at index 0 (B) reaches only the player on it: the deleted profile's old index is
	nobody's */
	{
		int player, matches = 0;
		for (player = 0; player < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; player++)
			if (active_of(player) == INDEX_OF(0))
				matches |= 1 << player;
		assert(matches == (1 << 1));
		/* (a profile created now takes the freed last position, which nobody holds either) */
		index_a = profile_create(&a);
		matches = 0;
		for (player = 0; player < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; player++)
			if (active_of(player) == index_a)
				matches |= 1 << player;
		assert(matches == 0);
	}

	/* a delete on another thread (a failed profile write): the post is applied by the game thread, and not before */
	{
		pthread_t thread;
		count = 0;
		memset(&player_ui_globals, 0, sizeof(player_ui_globals));
		player_ui_globals.local_players[1].active_profile_index = NONE;
		player_ui_globals.local_players[2].active_profile_index = NONE;
		player_ui_globals.local_players[3].active_profile_index = NONE;
		index_a = profile_create(&a);
		index_b = profile_create(&b);
		player_ui_set_active_player_profile(0, index_a, &record);
		player_ui_set_active_player_profile(1, index_b, &record);
		pthread_create(&thread, NULL, delete_on_another_thread, &index_a);
		pthread_join(thread, NULL);
		assert(player_ui_globals.local_players[0].active_profile_index == index_a);	/* (nothing applied off the game thread) */
		player_ui_apply_pending_saved_game_file_deletions();
		assert(active_of(0) == NONE && active_of(1) == INDEX_OF(0));
		assert(!memcmp(player_input_profile_for_local_player(0), &defaults, sizeof(defaults)));
		assert(!memcmp(player_input_profile_for_local_player(1), &b, sizeof(b)));
	}

	/* two deletes before the tick apply in order */
	count = 0;
	memset(&player_ui_globals, 0, sizeof(player_ui_globals));
	player_ui_globals.local_players[0].active_profile_index = NONE;
	player_ui_globals.local_players[1].active_profile_index = NONE;
	player_ui_globals.local_players[2].active_profile_index = NONE;
	player_ui_globals.local_players[3].active_profile_index = NONE;
	index_a = profile_create(&a);
	index_b = profile_create(&b);
	index_c = profile_create(&c);
	player_ui_set_active_player_profile(1, index_b, &record);
	player_ui_set_active_player_profile(2, index_c, &record);
	profile_delete(index_a);
	profile_delete(INDEX_OF(0));	/* B, now first */
	player_ui_apply_pending_saved_game_file_deletions();
	assert(active_of(1) == NONE && active_of(2) == INDEX_OF(0));
	assert(!memcmp(player_input_profile_for_local_player(2), &c, sizeof(c)));

	/* more deletes between ticks than the mailbox holds: nobody can be trusted to hold a profile */
	{
		int i;
		count = 0;
		index_a = profile_create(&a);
		player_ui_set_active_player_profile(0, index_a, &record);
		for (i = 0; i < 40; i++)
			player_ui_post_saved_game_file_deleted(INDEX_OF(9));
		player_ui_apply_pending_saved_game_file_deletions();
		assert(active_of(0) == NONE && active_of(2) == NONE);
		assert(!memcmp(player_input_profile_for_local_player(0), &defaults, sizeof(defaults)));

		/* after an overflow the next ordinary delete is still seen (the ring keeps its place) */
		count = 0;
		memset(&player_ui_globals, 0, sizeof(player_ui_globals));
		player_ui_globals.local_players[1].active_profile_index = NONE;
		player_ui_globals.local_players[2].active_profile_index = NONE;
		player_ui_globals.local_players[3].active_profile_index = NONE;
		index_a = profile_create(&a);
		index_b = profile_create(&b);
		player_ui_set_active_player_profile(0, index_b, &record);
		profile_delete(index_a);
		player_ui_apply_pending_saved_game_file_deletions();
		assert(active_of(0) == INDEX_OF(0));
	}

	/* every cached index follows a delete: the last-used profile of player 1 and the profile being edited, as well as
	the local players' */
	reset_players();
	index_a = profile_create(&a);
	index_b = profile_create(&b);
	index_c = profile_create(&c);
	player1_last_used_profile_index = index_c;
	player_ui_set_edit_profile_index(index_c);
	profile_delete(index_a);
	player_ui_apply_pending_saved_game_file_deletions();
	assert(player1_last_used_profile_index == INDEX_OF(1) && player_ui_globals.edit_profile_index == INDEX_OF(1));
	/* the deleted profile itself: nothing keeps its index */
	profile_delete(INDEX_OF(1));
	player_ui_apply_pending_saved_game_file_deletions();
	assert(player1_last_used_profile_index == NONE && player_ui_globals.edit_profile_index == NONE);

	/* the saved games listed again in another order: the paths decide, for the local players (with their own input
	settings), the profile being edited and player 1's last-used profile (found again when it is next read) */
	reset_players();
	index_a = profile_create(&a);
	index_b = profile_create(&b);
	index_c = profile_create(&c);
	player_ui_set_active_player_profile(0, index_a, &record);
	player_ui_set_active_player_profile(1, index_b, &record);
	player_ui_set_active_player_profile(2, index_c, &record);
	player_ui_set_edit_profile_index(index_a);
	player1_last_used_profile_index = index_c;
	snprintf(player1_profile_path, 32, "p%d", file_id[2]);
	profile_relist_reversed();
	player_ui_apply_pending_saved_game_file_deletions();
	assert(active_of(0) == INDEX_OF(2) && active_of(1) == INDEX_OF(1) && active_of(2) == INDEX_OF(0));
	assert(!memcmp(player_input_profile_for_local_player(0), &a, sizeof(a)));
	assert(!memcmp(player_input_profile_for_local_player(2), &c, sizeof(c)));
	assert(player_ui_globals.edit_profile_index == INDEX_OF(2));
	assert(player_ui_get_player1_last_used_profile_index() == INDEX_OF(0));

	/* a profile that is gone when the list is built again is dropped, not left on another profile's index */
	reset_players();
	index_a = profile_create(&a);
	index_b = profile_create(&b);
	player_ui_set_active_player_profile(0, index_a, &record);
	player_ui_set_active_player_profile(1, index_b, &record);
	player_ui_set_edit_profile_index(index_a);
	profile_remove(index_a);	/* (the listing no longer has it, and nothing was posted) */
	player_ui_post_saved_game_files_relisted();
	player_ui_apply_pending_saved_game_file_deletions();
	assert(active_of(0) == NONE && active_of(1) == INDEX_OF(0) && player_ui_globals.edit_profile_index == NONE);
	assert(!memcmp(player_input_profile_for_local_player(0), &defaults, sizeof(defaults)));
	assert(!memcmp(player_input_profile_for_local_player(1), &b, sizeof(b)));

	/* a compaction that failed partway: nothing is trusted */
	reset_players();
	index_a = profile_create(&a);
	index_b = profile_create(&b);
	player_ui_set_active_player_profile(0, index_a, &record);
	player_ui_set_active_player_profile(1, index_b, &record);
	player_ui_set_edit_profile_index(index_b);
	player1_last_used_profile_index = index_b;
	player_ui_post_saved_game_files_unknown_change();
	player_ui_apply_pending_saved_game_file_deletions();
	assert(active_of(0) == NONE && active_of(1) == NONE && player_ui_globals.edit_profile_index == NONE && player1_last_used_profile_index == NONE);
	assert(!memcmp(player_input_profile_for_local_player(1), &defaults, sizeof(defaults)));
	return 0;
}
'''


def _activation_status(tmp_path, ui_source, mutate=None):
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required')
    ui_source = ui_source.replace('\r\n', '\n')
    functions = probe_source.function_in(ui_source, 'player_ui_set_active_player_profile')
    # (the deletion mailbox is new in the branch: a source without it gets empty posts, which the probe must reject)
    if '/* port: saved game file deletion mailbox' in ui_source:
        start = ui_source.index('/* port: saved game file deletion mailbox')
        functions = ui_source[start:ui_source.index('/* end of the deletion mailbox */')] + '\n' + \
            probe_source.function_in(ui_source, 'player_ui_get_player1_last_used_profile_index') + '\n' + functions
    else:
        functions = ('void player_ui_post_saved_game_file_deleted(long deleted_index) { (void)deleted_index; }\n'
                     'void player_ui_apply_pending_saved_game_file_deletions(void) { }\n') + functions
    if mutate:
        functions = mutate(functions)
    folder = tmp_path / ('probe%d' % len(list(tmp_path.iterdir())))
    folder.mkdir()
    program = folder / 'probe.c'
    program.write_text(PROGRAM.replace('/* FUNCTIONS */', functions).replace('"../../port/', '"' + str(ROOT) + '/port/'))
    binary = folder / 'probe'
    subprocess.run([compiler, '-std=gnu99', '-Wall', '-Wno-unused-function', '-Wno-unused-variable',
                    '-fsanitize=address,undefined', '-pthread', str(program), '--output=' + str(binary)], check=True)
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


def test_the_probe_catches_a_delete_that_leaves_active_indices_stale(tmp_path):
    ui = (ROOT / 'source/interface/player_ui.c').read_text(encoding='latin-1')

    def stale_indices(functions):
        # (the hook that moves the players does nothing: the stale-index bug)
        return _replace_function(functions, 'player_ui_saved_game_file_deleted',
                                 'static void player_ui_saved_game_file_deleted(long deleted_index) { (void)deleted_index; }')

    assert _activation_status(tmp_path, ui, stale_indices) != 0


def test_deletes_are_posted_under_the_mapfile_mutex_and_drained_before_index_reads():
    files = (ROOT / 'source' / 'saved games' / 'saved_game_files.c').read_text(encoding='latin-1')
    body = probe_source.function_in(files, 'remove_nth_entry_in_mapfile')
    body = body[:body.index('saved_game_files_release_mapfile_mutex();')]
    assert 'player_ui_post_saved_game_file_deleted(deleted_index)' in body
    assert 'player_ui_apply_pending_saved_game_file_deletions' in (ROOT / 'source' / 'main' / 'main.c').read_text(encoding='latin-1')
    # every function in player_ui.c that reads an active index drains first
    ui = (ROOT / 'source' / 'interface' / 'player_ui.c').read_text(encoding='latin-1').replace('\r\n', '\n')
    start = ui.index('/* end of the deletion mailbox */')
    for chunk in re.split(r'\n(?=[a-z][a-z_ ]* \*?player_ui_\w+\()', ui[:start] + '\n' + ui[start:]):
        if '.active_profile_index' in chunk and not re.match(r'static void player_ui_(saved_game_file_deleted|find_cached|clear_active|clear_edit|remember_profile_path|forget_cached)', chunk) \
                and 'player_ui_apply_pending_saved_game_file_deletions' not in chunk[:60]:
            if re.search(r'\.active_profile_index\s*(!=|==|,|\))', chunk) or 'result = ' in chunk:
                assert 'player_ui_apply_pending_saved_game_file_deletions();' in chunk, chunk[:80]


def _replace_function(functions, name, replacement):
    return functions.replace(probe_source.function_in(functions, name), replacement, 1)


def test_the_probe_catches_a_post_that_does_nothing(tmp_path):
    ui = (ROOT / 'source/interface/player_ui.c').read_text(encoding='latin-1')

    def no_op_post(functions):
        return _replace_function(functions, 'player_ui_post_saved_game_file_deleted',
                                 'void player_ui_post_saved_game_file_deleted(long deleted_index) { (void)deleted_index; }')

    assert _activation_status(tmp_path, ui, no_op_post) != 0


def test_the_probe_catches_a_relisting_that_finds_nothing_again(tmp_path):
    ui = (ROOT / 'source/interface/player_ui.c').read_text(encoding='latin-1')

    def no_op_relisting(functions):
        return _replace_function(functions, 'player_ui_find_cached_profiles_by_path',
                                 'static void player_ui_find_cached_profiles_by_path(void) { }')

    def no_op_relisted_post(functions):
        return _replace_function(functions, 'player_ui_post_saved_game_files_relisted',
                                 'void player_ui_post_saved_game_files_relisted(void) { }')

    assert _activation_status(tmp_path, ui, no_op_relisting) != 0
    assert _activation_status(tmp_path, ui, no_op_relisted_post) != 0


def test_a_relisting_and_a_failed_compaction_are_posted_by_the_saved_game_files():
    files = (ROOT / 'source' / 'saved games' / 'saved_game_files.c').read_text(encoding='latin-1').replace('\r\n', '\n')
    listing = probe_source.body_in(files, 'enumerate_memory_units')
    assert listing.rstrip().endswith('player_ui_post_saved_game_files_relisted();\n\treturn;')
    remove = probe_source.function_in(files, 'remove_nth_entry_in_mapfile')
    remove = remove[:remove.index('saved_game_files_release_mapfile_mutex();')]
    assert 'player_ui_post_saved_game_files_unknown_change();' in remove
