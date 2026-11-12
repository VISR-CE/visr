"""Checkpoint epochs (network version 42 and later): only the host's ordered
boundary changes worlds, and a client plays only after its snapshot ends."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def test_client_takes_only_newer_ordered_boundaries_and_holds_play_until_synchronized(tmp_path):
    source = (ROOT / "port/linux/game/network_distributed.c").read_text()
    header = (ROOT / "port/linux/game/network_distributed.h").read_text()
    kinds = block(header, "enum\n{") + ";\n"
    structures = block(header, "struct distributed_message_header\n{").replace(
        "long game_time;", "int32_t game_time;") + ";\n"
    valid = block(source, "static boolean distributed_message_epoch_valid(")
    program = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef unsigned char byte;
typedef uint16_t word;
typedef uint16_t message_header;
typedef int boolean;
#define NONE (-1)
#define TRUE 1
#define FALSE 0
#define HALO_PORT_MAXIMUM_NETWORK_MACHINES 128
enum { _game_connection_network_client, _game_connection_network_server, _game_connection_local };
''' + kinds + structures + r'''
static int connection = _game_connection_network_client, unreliable;
static boolean distributed_handling_batch, distributed_handling_stream_message = TRUE;
static unsigned int distributed_checkpoint_epoch;
static boolean distributed_checkpoint_synchronized, distributed_identity_sent;
static int restores, damage_resets, state_resets;
static int game_connection(void) { return connection; }
static boolean network_connection_last_read_was_unreliable(void) { return unreliable; }
static void distributed_restore_checkpoint(void) { restores++; }
static void network_damage_new_game(void) { damage_resets++; }
static void distributed_reset_checkpoint_state(boolean all) { (void)all; state_resets++; }
''' + valid + r'''
static boolean check(long machine, byte type, byte count, unsigned epoch, word size)
{
    struct distributed_message_header header = {0};
    header.type = type;
    header.count = count;
    return distributed_message_epoch_valid(machine, &header, epoch, size);
}
#define H ((word)sizeof(struct distributed_message_header))
int main(void)
{
    /* before any boundary, a client takes nothing but the host's 96 */
    assert(!check(NONE, _distributed_message_unit_states, 1, 0, 40));
    assert(!check(NONE, _distributed_message_object_changes, 1, 5, 40));

    /* a boundary must come ordered, alone, empty and nonzero, from the host */
    unreliable = 1;
    assert(!check(NONE, _distributed_message_checkpoint, 0, 5, H));
    unreliable = 0;
    distributed_handling_batch = 1;
    assert(!check(NONE, _distributed_message_checkpoint, 0, 5, H));
    distributed_handling_batch = 0;
    assert(!check(NONE, _distributed_message_checkpoint, 0, 0, H));
    assert(!check(NONE, _distributed_message_checkpoint, 1, 5, H));
    assert(!check(NONE, _distributed_message_checkpoint, 0, 5, H + 4));
    assert(!check(3, _distributed_message_checkpoint, 0, 5, H));
    assert(restores == 0 && distributed_checkpoint_epoch == 0);

    /* the boundary is taken, restored, and never handled further */
    distributed_identity_sent = TRUE;
    assert(!check(NONE, _distributed_message_checkpoint, 0, 5, H));
    assert(distributed_checkpoint_epoch == 5 && restores == 1);
    assert(!distributed_checkpoint_synchronized && !distributed_identity_sent);

    /* the same or an older boundary is ignored */
    assert(!check(NONE, _distributed_message_checkpoint, 0, 5, H));
    assert(!check(NONE, _distributed_message_checkpoint, 0, 4, H));
    assert(distributed_checkpoint_epoch == 5 && restores == 1);

    /* in transfer: only what rebuilds objects, and only in this epoch */
    assert(check(NONE, _distributed_message_object_changes, 1, 5, 40));
    assert(!check(NONE, _distributed_message_object_changes, 1, 4, 40));
    unreliable = 1;
    assert(!check(NONE, _distributed_message_object_changes, 1, 5, 40));
    assert(!check(NONE, _distributed_message_inventories, 1, 5, 40));
    unreliable = 0;
    assert(check(NONE, _distributed_message_inventories, 1, 5, 40));
    assert(!check(NONE, _distributed_message_unit_states, 1, 5, 40));
    assert(!check(NONE, _distributed_message_player_prediction, 1, 5, 40));
    assert(!check(3, _distributed_message_object_changes, 1, 5, 40));

    /* the snapshot's ordered end opens play and retires provisional hits */
    assert(!check(NONE, _distributed_message_objects_synchronized, 1, 5, H));
    unreliable = 1;
    assert(!check(NONE, _distributed_message_objects_synchronized, 0, 5, H));
    unreliable = 0;
    assert(check(NONE, _distributed_message_objects_synchronized, 0, 5, H));
    assert(distributed_checkpoint_synchronized && damage_resets == 1 && state_resets == 1);
    assert(check(NONE, _distributed_message_objects_synchronized, 0, 5, H));
    assert(damage_resets == 1);
    assert(check(NONE, _distributed_message_unit_states, 1, 5, 40));
    assert(!check(NONE, _distributed_message_unit_states, 1, 4, 40));
    assert(!check(NONE, _distributed_message_unit_states, 1, 6, 40));

    /* a revert: newer boundary, play held again until the next snapshot */
    assert(!check(NONE, _distributed_message_checkpoint, 0, 6, H));
    assert(distributed_checkpoint_epoch == 6 && restores == 2 && !distributed_checkpoint_synchronized);
    assert(!check(NONE, _distributed_message_unit_states, 1, 6, 40));
    assert(!check(NONE, _distributed_message_object_changes, 1, 5, 40));
    assert(check(NONE, _distributed_message_object_changes, 1, 6, 40));

    /* the epoch counter wraps past zero */
    distributed_checkpoint_epoch = 0xfffffffe;
    assert(!check(NONE, _distributed_message_checkpoint, 0, 1, H));
    assert(distributed_checkpoint_epoch == 1 && restores == 3);

    /* host: a late joiner's ready, in any epoch, ordered and alone */
    connection = _game_connection_network_server;
    distributed_checkpoint_epoch = 6;
    assert(check(2, _distributed_message_client_ready, 1, 0, H));
    assert(check(2, _distributed_message_client_ready, 0, 3, H));
    assert(!check(NONE, _distributed_message_client_ready, 1, 0, H));
    assert(!check(2, _distributed_message_client_ready, 2, 0, H));
    assert(!check(2, _distributed_message_client_ready, 1, 0, H + 1));
    distributed_handling_stream_message = FALSE;
    assert(!check(2, _distributed_message_client_ready, 1, 0, H));
    distributed_handling_stream_message = TRUE;
    /* and anything else from a client only in the host's epoch */
    assert(check(2, _distributed_message_player_prediction, 1, 6, 40));
    assert(!check(2, _distributed_message_player_prediction, 1, 5, 40));
    assert(!check(NONE, _distributed_message_checkpoint, 0, 9, H));
    assert(distributed_checkpoint_epoch == 6);
    return 0;
}
'''
    (tmp_path / "probe.c").write_text(program)
    executable = tmp_path / "probe"
    subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-Wno-unused-function",
                    "-Wno-unused-variable", "-fsanitize=address,undefined",
                    str(tmp_path / "probe.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
