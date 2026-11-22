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
static int solo = 1, profile = 1, coop, replay, scripted, lifted, setting = 1, has_directory = 1;
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
    assert(player == 0); strcpy(path, "u:\\UDATA\\C7201D882DAE\\"); return has_directory;
}
static void platform_translate_path(const char *xbox_path, char *host_path, unsigned long size) {
    snprintf(host_path, size, "host<%s>", xbox_path);
}
static int begins, ends, writes;
static char last_checkpoint[1024], last_target[1024];
static int last_has_target; static unsigned last_size; static const void *last_image;
static void host_save_checkpoint_begin(void) { assert(begins == ends); begins++; }
static void host_save_checkpoint_end(const char *checkpoint_path, const char *target_path, unsigned int size) {
    assert(begins == ends + 1); ends++;
    assert(checkpoint_path);
    last_has_target = target_path != NULL; last_size = size;
    strcpy(last_checkpoint, checkpoint_path); strcpy(last_target, target_path ? target_path : "");
}
static int host_save_write_now(const char *checkpoint_path, const char *target_path, const void *image, unsigned int size) {
    writes++; last_image = image; last_size = size;
    strcpy(last_checkpoint, checkpoint_path); strcpy(last_target, target_path);
    return 1;
}
'''

MAIN = r'''
int main(void) {
    int bits, written, persists = 0;
    for (bits = 0; bits < 512; bits++)
        for (written = 0; written < 2; written++) {
            int expected;
            solo = bits & 1; profile = (bits >> 1) & 1; coop = (bits >> 2) & 1; replay = (bits >> 3) & 1;
            setting = (bits >> 4) & 1; has_directory = (bits >> 5) & 1; player_spawn_count = (bits >> 6) & 1 ? 2 : 1;
            scripted = (bits >> 7) & 1; lifted = (bits >> 8) & 1;
            expected = solo && profile && !coop && player_spawn_count == 1 && (lifted || (!replay && !scripted));
            assert(saved_game_persist_allowed() == expected);
            saved_game_persist_checkpoint_begin();
            saved_game_persist_checkpoint_end(written, 4096);
            /* the host's count of the write always closes, whatever becomes of the checkpoint */
            assert(begins == ends && last_size == 4096);
            assert(last_has_target == (expected && setting && has_directory && written));
            if (last_has_target) {
                persists++;
                assert(!strcmp(last_target, "host<u:\\UDATA\\C7201D882DAE\\savegame.bin>"));
                assert(!strcmp(last_checkpoint, "host<z:\\savegame.bin>"));
            }
        }
    assert(persists == 5 && begins == 1024);
    /* Save and Quit hands the host the game's buffer and both paths; the setting is for checkpoints only */
    solo = profile = has_directory = 1; coop = replay = scripted = lifted = setting = 0; player_spawn_count = 1;
    assert(saved_game_persist_write_now(&writes, 77) == TRUE && writes == 1 && last_image == &writes && last_size == 77);
    assert(!strcmp(last_target, "host<u:\\UDATA\\C7201D882DAE\\savegame.bin>") && !strcmp(last_checkpoint, "host<z:\\savegame.bin>"));
    has_directory = 0;
    assert(saved_game_persist_write_now(&writes, 77) == FALSE && writes == 1);
    return 0;
}
'''


def test_only_the_players_own_campaign_game_writes_a_saved_game(tmp_path):
    source = (ROOT / "port/linux/game/saved_game_persist.c").read_text()
    functions = "\n".join(function(source, signature) for signature in (
        "boolean saved_game_persist_allowed(", "static boolean persist_paths(", "void saved_game_persist_checkpoint_begin(",
        "void saved_game_persist_checkpoint_end(", "boolean saved_game_persist_write_now("))
    subprocess.run([str(compile_probe(tmp_path, STUBS + functions + MAIN))], check=True)


def test_the_checkpoint_write_is_bracketed(tmp_path):
    """game_state_save tells the host before and after its write of the checkpoint file, and whether it succeeded."""
    actual = function((ROOT / "source/saved games/game_state.c").read_text(), "void game_state_save(")
    binary = compile_probe(tmp_path, r'''
#include <assert.h>
#include <string.h>
typedef int boolean;
#define FALSE 0
enum { GAME_STATE_SIZE = 12345 };
static struct { boolean saved_game_valid; } game_state_globals;
static char order[64]; static int write_ok = 1, ended_with = -1; static long ended_size;
static void game_state_call_before_save_procs(void) { strcat(order, "p"); }
static void main_stop_time(void) { strcat(order, "s"); }
static void main_start_time(void) { strcat(order, "S"); }
static void saved_game_persist_checkpoint_begin(void) { strcat(order, "b"); }
static boolean game_state_write_to_file(void) { strcat(order, "w"); return write_ok; }
static void saved_game_persist_checkpoint_end(boolean written, long size) { strcat(order, "e"); ended_with = written; ended_size = size; }
static void network_coop_checkpoint_saved(void) { strcat(order, "c"); }
static void game_state_note_event(const char *name) { (void)name; }
''' + actual + r'''
int main(void) {
    game_state_save();
    assert(!strcmp(order, "psbweSc") && ended_with == 1 && ended_size == GAME_STATE_SIZE);
    order[0] = 0; write_ok = 0;
    game_state_save();
    assert(!strcmp(order, "psbweS") && ended_with == 0);
    return 0;
}
''')
    subprocess.run([str(binary)], check=True)


def test_a_build_without_a_host_links_and_keeps_nothing(tmp_path):
    """The desktop builds compile the same unit and have no host imports: the class of bug is a guest call to a
    host function that only the Apple builds provide, which fails only at the desktop link."""
    source = (ROOT / "port/linux/game/saved_game_persist.c").read_text()
    functions = "\n".join(function(source, signature) for signature in (
        "boolean saved_game_persist_allowed(", "void saved_game_persist_checkpoint_begin(",
        "void saved_game_persist_checkpoint_end(", "boolean saved_game_persist_write_now("))
    stubs = STUBS.replace("#define HALO_IOS 1\n", "")
    for name in ("host_save_checkpoint_begin", "host_save_checkpoint_end", "host_save_write_now"):
        assert f"static void {name}(" in stubs or f"static int {name}(" in stubs
    # (no host function is defined here at all: a call to one would not link)
    start = stubs.index("static int begins, ends, writes;")
    stubs = stubs[:start] + "static int writes;\n"
    binary = compile_probe(tmp_path, stubs + "__attribute__((unused)) static void *unused_stubs[] = { platform_translate_path, "
        "player_ui_get_path_to_local_player_profile_directory };\n" + functions + r'''
int main(void) {
    saved_game_persist_checkpoint_begin();
    saved_game_persist_checkpoint_end(TRUE, 4096);
    assert(saved_game_persist_allowed());
    assert(saved_game_persist_write_now(&writes, 77) == FALSE);
    return 0;
}
''')
    subprocess.run([str(binary)], check=True)
