"""Which games write a saved game (port/linux/game/saved_game_persist.c), and how a checkpoint reaches the host.

The class of bug: a run that is not the player's own campaign game changing the player's saved game. A replay,
a benchmark's level, a co-op or network game, a level started with no profile and a multiplayer map played alone
all pass checkpoints; none may make one the profile's saved game. Every combination is tried."""
from pathlib import Path
import subprocess

from test_profile_game_hooks import compile_probe, function

ROOT = Path(__file__).resolve().parents[1]

STUBS = r'''
#define HALO_IOS 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int boolean;
#define NONE (-1)
#define FALSE 0
#define TRUE 1
enum { PERSIST_PATH_SIZE = 1024 };
static int solo = 1, profile = 1, coop, replay, scripted, lifted, setting = 1, has_directory = 1, other_profile, translations;
static short player_spawn_count = 1;
static short main_get_current_solo_level(void) { return solo ? 3 : NONE; }
static long player_ui_get_active_player_profile_index(short player) { assert(player == 0); return profile ? 2 : NONE; }
static boolean network_coop_active(void) { return coop; }
static unsigned char input_replay_playing(void) { return (unsigned char)replay; }
static int config_scripted_run(void) { return scripted; }
static int config_boolean(const char *name) {
    if (!strcmp(name, "debug.scripted_run_saves")) return lifted;
    assert(!strcmp(name, "saves.checkpoint_persist")); return setting;
}
static boolean player_ui_get_path_to_local_player_profile_directory(short player, char *path) {
    assert(player == 0); strcpy(path, other_profile ? "u:\\UDATA\\AAAAAAAAAAAA\\" : "u:\\UDATA\\C7201D882DAE\\"); return has_directory;
}
static void platform_translate_path(const char *xbox_path, char *host_path, unsigned long size) {
    translations++;
    snprintf(host_path, size, "host<%s>", xbox_path);
}
static int stores, loads, writes, host_keeps = 1;
static char last_checkpoint[1024], last_target[1024];
static int last_has_target; static unsigned last_size; static const void *last_image;
static int host_save_checkpoint_store(const void *image, unsigned int size, const char *session_path, const char *target_path) {
    stores++;
    assert(session_path);
    last_has_target = target_path != NULL; last_size = size; last_image = image;
    strcpy(last_checkpoint, session_path); strcpy(last_target, target_path ? target_path : "");
    return host_keeps;
}
static int host_save_checkpoint_load(void *image, unsigned int size) { loads++; last_image = image; last_size = size; return host_keeps; }
static int host_save_write_now(const char *checkpoint_path, const char *target_path, const void *image, unsigned int size) {
    writes++; last_image = image; last_size = size;
    strcpy(last_checkpoint, checkpoint_path); strcpy(last_target, target_path);
    return 1;
}
'''

MAIN = r'''
int main(void) {
    int bits, kept, persists = 0;
    for (bits = 0; bits < 512; bits++)
        for (kept = 0; kept < 2; kept++) {
            int expected;
            solo = bits & 1; profile = (bits >> 1) & 1; coop = (bits >> 2) & 1; replay = (bits >> 3) & 1;
            setting = (bits >> 4) & 1; has_directory = (bits >> 5) & 1; player_spawn_count = (bits >> 6) & 1 ? 2 : 1;
            scripted = (bits >> 7) & 1; lifted = (bits >> 8) & 1;
            expected = solo && profile && !coop && player_spawn_count == 1 && (lifted || (!replay && !scripted));
            assert(saved_game_persist_allowed() == expected);
            /* the host is always given the image (it is the checkpoint a revert goes back to), and whether it
               kept it is what the game is told */
            host_keeps = kept;
            assert(saved_game_persist_checkpoint_store(&stores, 4096) == kept);
            assert(last_image == &stores && last_size == 4096);
            assert(last_has_target == (expected && setting && has_directory));
            if (last_has_target) {
                persists++;
                assert(!strcmp(last_target, "host<u:\\UDATA\\C7201D882DAE\\savegame.bin>"));
                assert(!strcmp(last_checkpoint, "host<z:\\savegame.bin>"));
            }
        }
    assert(persists == 10 && stores == 1024);
    /* a revert asks the host for that copy */
    host_keeps = 1;
    assert(saved_game_persist_checkpoint_load(&loads, 4096) == TRUE && loads == 1 && last_image == &loads && last_size == 4096);
    host_keeps = 0;
    assert(saved_game_persist_checkpoint_load(&loads, 4096) == FALSE && loads == 2);
    host_keeps = 1;
    /* Save and Quit hands the host the game's buffer and both paths; the setting is for checkpoints only */
    solo = profile = has_directory = 1; coop = replay = scripted = lifted = setting = 0; player_spawn_count = 1;
    assert(saved_game_persist_write_now(&writes, 77) == TRUE && writes == 1 && last_image == &writes && last_size == 77);
    assert(!strcmp(last_target, "host<u:\\UDATA\\C7201D882DAE\\savegame.bin>") && !strcmp(last_checkpoint, "host<z:\\savegame.bin>"));
    has_directory = 0;
    assert(saved_game_persist_write_now(&writes, 77) == FALSE && writes == 1);
    /* the paths are translated once, not at every checkpoint (a translation walks folders on the disk,
       on the game's thread), and again when another profile is the active one */
    assert(translations == 2);
    has_directory = setting = 1; other_profile = 1;
    saved_game_persist_checkpoint_store(&stores, 4096);
    assert(translations == 3 && !strcmp(last_target, "host<u:\\UDATA\\AAAAAAAAAAAA\\savegame.bin>"));
    saved_game_persist_checkpoint_store(&stores, 4096);
    other_profile = 0;
    saved_game_persist_checkpoint_store(&stores, 4096);
    assert(translations == 4 && !strcmp(last_target, "host<u:\\UDATA\\C7201D882DAE\\savegame.bin>"));
    return 0;
}
'''


def test_only_the_players_own_campaign_game_writes_a_saved_game(tmp_path):
    source = (ROOT / "port/linux/game/saved_game_persist.c").read_text()
    functions = "\n".join(function(source, signature) for signature in (
        "boolean saved_game_persist_allowed(", "static boolean persist_paths(", "boolean saved_game_persist_checkpoint_store(",
        "boolean saved_game_persist_checkpoint_load(", "boolean saved_game_persist_write_now("))
    subprocess.run([str(compile_probe(tmp_path, STUBS + functions + MAIN))], check=True)


def test_a_checkpoint_is_valid_when_it_was_kept(tmp_path):
    """game_state_save's checkpoint is valid exactly when game_state_write_to_file says so, which on the Apple
    builds is whether the host kept the copy; co-op is told only of a valid one."""
    actual = function((ROOT / "source/saved games/game_state.c").read_text(), "void game_state_save(")
    binary = compile_probe(tmp_path, r"""
#include <assert.h>
#include <string.h>
typedef int boolean;
#define FALSE 0
static struct { boolean saved_game_valid; } game_state_globals;
static char order[64]; static int write_ok = 1;
static void game_state_call_before_save_procs(void) { strcat(order, "p"); }
static void main_stop_time(void) { strcat(order, "s"); }
static void main_start_time(void) { strcat(order, "S"); }
static boolean game_state_write_to_file(void) { strcat(order, "w"); return write_ok; }
static void network_coop_checkpoint_saved(void) { strcat(order, "c"); }
static void game_state_note_event(const char *name) { (void)name; }
""" + actual + r"""
int main(void) {
    game_state_save();
    assert(!strcmp(order, "pswSc") && game_state_globals.saved_game_valid);
    order[0] = 0; write_ok = 0;
    game_state_save();
    assert(!strcmp(order, "pswS") && !game_state_globals.saved_game_valid);
    return 0;
}
""")
    subprocess.run([str(binary)], check=True)


CHECKPOINT_FILE_FUNCTIONS = ("void game_state_create_or_open_file(", "void game_state_close_file(",
                             "boolean game_state_write_to_file(", "boolean game_state_read_from_file(")

CHECKPOINT_FILE_STUBS = r"""
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef int boolean;
#define FALSE 0
#define TRUE 1
#define match_assert(file, line, condition) assert(condition)
#define match_vassert(file, line, condition, message) (failures += !(condition))
#define csprintf(buffer, format, code) ((void)(code), "")
static int failures;
__attribute__((unused)) static char temporary[8];
__attribute__((unused)) static boolean recover_saved_games_hack;
static struct {
    void *buffer; unsigned long buffer_size; boolean buffer_allocated, file_open, file_valid_for_read; void *handle;
} xbox_game_state_globals;
static unsigned char state[64], accepted[64];
static int accepts = 1;
static boolean game_state_image_accept(void *image, unsigned long size) {
    assert(size == sizeof(state));
    if (accepts) memcpy(accepted, image, size);
    return accepts;
}
"""


def test_a_checkpoint_and_a_revert_touch_no_file_on_the_apple_builds(tmp_path):
    """The class of bug: file system work on the game's thread at a checkpoint (one 21 MB write held a frame for
    about 180 ms on the slowest device). On the Apple builds the four checkpoint file functions are compiled here
    with no file function defined at all, so a call to one would not link: a checkpoint hands the game state to
    saved_game_persist_checkpoint_store, a revert takes it from saved_game_persist_checkpoint_load, and the flags
    the game asserts on behave as they did with the file."""
    source = (ROOT / "source/saved games/game_state_xbox.c").read_text()
    functions = "\n".join(function(source, signature) for signature in CHECKPOINT_FILE_FUNCTIONS)
    binary = compile_probe(tmp_path, "#define HALO_IOS 1\n" + CHECKPOINT_FILE_STUBS + r"""
static unsigned char kept[64]; static int host_keeps = 1, has_kept, stores, loads;
static boolean saved_game_persist_checkpoint_store(const void *image, long size) {
    stores++; assert(size == sizeof(kept));
    if (host_keeps) { memcpy(kept, image, sizeof(kept)); has_kept = 1; }
    return host_keeps;
}
static boolean saved_game_persist_checkpoint_load(void *image, long size) {
    loads++; assert(size == sizeof(kept));
    if (has_kept) memcpy(image, kept, sizeof(kept));
    return has_kept;
}
""" + functions + r"""
int main(void) {
    xbox_game_state_globals.buffer = state; xbox_game_state_globals.buffer_size = sizeof(state);
    xbox_game_state_globals.buffer_allocated = TRUE;
    game_state_create_or_open_file();
    assert(xbox_game_state_globals.file_open && !xbox_game_state_globals.file_valid_for_read);
    /* a copy the host could not keep is a checkpoint that failed, and there is none to read */
    host_keeps = 0; memset(state, 1, sizeof(state));
    assert(!game_state_write_to_file() && failures == 1 && !xbox_game_state_globals.file_valid_for_read);
    /* a checkpoint, the game goes on, a revert: the checkpoint's bytes come back through the image check */
    host_keeps = 1; memset(state, 2, sizeof(state));
    assert(game_state_write_to_file() && stores == 2 && xbox_game_state_globals.file_valid_for_read);
    memset(state, 3, sizeof(state));
    assert(game_state_read_from_file() && loads == 1 && accepted[0] == 2 && accepted[63] == 2 && failures == 1);
    /* (the game state itself is replaced by game_state_image_accept, never by the load) */
    assert(state[0] == 3);
    /* an image the check declines is not taken */
    accepts = 0;
    assert(!game_state_read_from_file() && failures == 2);
    /* a revert with no checkpoint kept is declined quietly (the caller starts the map over), not an assertion */
    has_kept = 0;
    assert(!game_state_read_from_file() && failures == 2);
    game_state_close_file();
    assert(!xbox_game_state_globals.file_open);
    game_state_create_or_open_file();
    assert(xbox_game_state_globals.file_open);
    return 0;
}
""")
    subprocess.run([str(binary)], check=True)


def test_a_build_without_a_host_keeps_its_checkpoint_file(tmp_path):
    """The desktop builds have no host to keep the copy: the same four functions use the file z:\\savegame.bin as
    they always did, and name no saved_game_persist function (none is defined here, so one would not link)."""
    source = (ROOT / "source/saved games/game_state_xbox.c").read_text()
    functions = "\n".join(function(source, signature) for signature in CHECKPOINT_FILE_FUNCTIONS)
    binary = compile_probe(tmp_path, CHECKPOINT_FILE_STUBS + r"""
typedef void *HANDLE; typedef unsigned long DWORD;
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define OPEN_ALWAYS 4
#define FILE_FLAG_NO_BUFFERING 8
#define FILE_FLAG_SEQUENTIAL_SCAN 16
#define FILE_BEGIN 0
#define GAME_STATE_FILE_SIZE 64
static char calls[64]; static unsigned char file[64]; static char opened[32];
static HANDLE CreateFileA(const char *name, DWORD a, DWORD b, void *c, DWORD d, DWORD e, void *f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; strcat(calls, "o"); strcpy(opened, name); return file;
}
static DWORD SetFilePointer(HANDLE h, long to, void *high, DWORD from) { (void)h; (void)to; (void)high; (void)from; strcat(calls, "s"); return 0; }
static int SetEndOfFile(HANDLE h) { (void)h; strcat(calls, "e"); return 1; }
static int CloseHandle(HANDLE h) { (void)h; strcat(calls, "c"); return 1; }
static int WriteFile(HANDLE h, const void *data, DWORD size, unsigned long *written, void *o) {
    (void)h; (void)o; strcat(calls, "w"); memcpy(file, data, size); *written = size; return 1;
}
static int ReadFile(HANDLE h, void *data, DWORD size, unsigned long *read, void *o) {
    (void)h; (void)o; strcat(calls, "r"); memcpy(data, file, size); *read = size; return 1;
}
static DWORD GetLastError(void) { return 0; }
""" + functions + r"""
int main(void) {
    (void)GetLastError;
    xbox_game_state_globals.buffer = state; xbox_game_state_globals.buffer_size = sizeof(state);
    xbox_game_state_globals.buffer_allocated = TRUE;
    game_state_create_or_open_file();
    assert(!strcmp(opened, "z:\\savegame.bin") && xbox_game_state_globals.file_open);
    memset(state, 2, sizeof(state));
    assert(game_state_write_to_file() && xbox_game_state_globals.file_valid_for_read);
    memset(state, 3, sizeof(state));
    assert(game_state_read_from_file() && accepted[0] == 2);
    game_state_close_file();
    assert(!strcmp(calls, "oseswsrc") && failures == 0);
    return 0;
}
""")
    subprocess.run([str(binary)], check=True)


def test_a_build_without_a_host_links_and_keeps_nothing(tmp_path):
    """The desktop builds compile the same unit and have no host imports: the class of bug is a guest call to a
    host function that only the Apple builds provide, which fails only at the desktop link."""
    source = (ROOT / "port/linux/game/saved_game_persist.c").read_text()
    functions = "\n".join(function(source, signature) for signature in (
        "boolean saved_game_persist_allowed(", "boolean saved_game_persist_checkpoint_store(",
        "boolean saved_game_persist_checkpoint_load(", "boolean saved_game_persist_write_now("))
    stubs = STUBS.replace("#define HALO_IOS 1\n", "")
    for name in ("host_save_checkpoint_store", "host_save_checkpoint_load", "host_save_write_now"):
        assert f"static int {name}(" in stubs
    # (no host function is defined here at all: a call to one would not link)
    start = stubs.index("static int stores, loads, writes, host_keeps = 1;")
    stubs = stubs[:start] + "static int writes;\n"
    binary = compile_probe(tmp_path, stubs + "__attribute__((unused)) static void *unused_stubs[] = { platform_translate_path, "
        "player_ui_get_path_to_local_player_profile_directory };\n" + functions + r'''
int main(void) {
    assert(saved_game_persist_checkpoint_store(&writes, 4096) == FALSE);
    assert(saved_game_persist_checkpoint_load(&writes, 4096) == FALSE);
    assert(saved_game_persist_allowed());
    assert(saved_game_persist_write_now(&writes, 77) == FALSE);
    return 0;
}
''')
    subprocess.run([str(binary)], check=True)


DELETE_SAVE_GAME_PROBE = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define WINAPI
#define MAX_PATH 260
#define ERROR_SUCCESS 0
#define ERROR_PATH_NOT_FOUND 3
typedef unsigned long DWORD; typedef const char *LPCSTR; typedef const unsigned short *LPCWSTR;
static char order[64], withdrawn[1024];
static int present = 1, deletes_that_leave_the_folder;
static void save_directory_for(LPCSTR root, LPCWSTR name, char *directory, unsigned long size) {
    (void)root; (void)name; snprintf(directory, size, "u:\\UDATA\\C7201D882DAE\\");
}
static DWORD GetFileAttributesA(const char *path) { (void)path; return present ? 16 : (DWORD)-1; }
static void platform_translate_path(const char *xbox_path, char *host_path, unsigned long size) {
    snprintf(host_path, size, "host<%s>", xbox_path);
}
void host_save_withdraw_folder(const char *folder_path) { strcat(order, "w"); strcpy(withdrawn, folder_path); }
static void delete_tree(const char *directory) {
    (void)directory; strcat(order, "d");
    if (deletes_that_leave_the_folder > 0) deletes_that_leave_the_folder--; else present = 0;
}
static void again(int leaving) { order[0] = 0; present = 1; deletes_that_leave_the_folder = leaving; }
"""


def test_a_profile_being_deleted_withdraws_its_checkpoint_first(tmp_path):
    """XDeleteSaveGame tells the host's writer before it lists the profile's folder. The class of bug: the
    writer's thread putting a file into a folder the game is emptying, which then cannot be removed. The profile
    write thread deletes profiles while the game plays, so a checkpoint posted between the withdrawal and the
    listing gets a second withdrawal and listing, and no more than that."""
    source = (ROOT / "port/linux/src/xbox_xapi.c").read_text()
    binary = compile_probe(tmp_path, "#define HALO_IOS 1\n" + DELETE_SAVE_GAME_PROBE
                           + function(source, "DWORD WINAPI XDeleteSaveGame(") + r"""
int main(void) {
    assert(XDeleteSaveGame("u:\\", 0) == ERROR_SUCCESS);
    assert(!strcmp(order, "wd") && !strcmp(withdrawn, "host<u:\\UDATA\\C7201D882DAE\\>"));
    again(1);
    assert(XDeleteSaveGame("u:\\", 0) == ERROR_SUCCESS && !strcmp(order, "wdwd"));
    again(5);
    assert(XDeleteSaveGame("u:\\", 0) == ERROR_SUCCESS && !strcmp(order, "wdwd"));
    order[0] = 0; present = 0;
    assert(XDeleteSaveGame("u:\\", 0) == ERROR_PATH_NOT_FOUND && !order[0]);
    return 0;
}
""")
    subprocess.run([str(binary)], check=True)


def test_a_build_without_a_host_deletes_a_profile_without_asking_one(tmp_path):
    """The desktop builds compile the same file and have no host: XDeleteSaveGame must not name a host function
    there (the probe's is left out, so naming it would not link), and deletes once."""
    source = (ROOT / "port/linux/src/xbox_xapi.c").read_text()
    stubs = "\n".join(line for line in DELETE_SAVE_GAME_PROBE.split("\n") if "host_save_withdraw_folder" not in line)
    binary = compile_probe(tmp_path, stubs + function(source, "DWORD WINAPI XDeleteSaveGame(") + r"""
int main(void) {
    (void)withdrawn; (void)platform_translate_path;
    again(1);
    assert(XDeleteSaveGame("u:\\", 0) == ERROR_SUCCESS && !strcmp(order, "d"));
    return 0;
}
""")
    subprocess.run([str(binary)], check=True)
