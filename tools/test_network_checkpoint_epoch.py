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


def test_epoch_path_is_off_below_42(tmp_path):
    """Below 42 the epoch never holds play, never gates what a client sends,
    and the revert hook sends no checkpoint; from 42 on all three engage."""
    source = (ROOT / "port/linux/game/network_distributed.c").read_text()
    header = (ROOT / "port/linux/game/network_distributed.h").read_text()
    kinds = block(header, "enum\n{") + ";\n"
    structures = block(header, "struct distributed_message_header\n{").replace(
        "long game_time;", "int32_t game_time;") + ";\n"
    functions = "\n".join(block(source, marker) for marker in (
        "unsigned long network_distributed_wire_epoch_size(",
        "static boolean distributed_fill_header(",
        "void network_distributed_reverted(",
        "void distributed_checkpoint_to_machine(",
        "boolean distributed_checkpoint_ready(",
    ))
    program = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "VERSION_HEADER"
typedef unsigned char byte;
typedef uint16_t word;
typedef uint16_t message_header;
typedef int boolean;
#define NONE (-1)
#define TRUE 1
#define FALSE 0
#define csmemset memset
enum { _game_connection_network_client, _game_connection_network_server, _game_connection_local };
''' + kinds + structures + r'''
static unsigned host_version;
static int connection = _game_connection_network_client, coop = 1, in_progress = 1;
static unsigned int distributed_checkpoint_epoch;
static boolean distributed_checkpoint_synchronized;
static struct { long sent; } distributed_statistics;
static int advances, restores, checkpoints_sent, host_reverts;
static unsigned network_game_host_version(void) { return host_version; }
static int game_connection(void) { return connection; }
static boolean network_coop_active(void) { return coop; }
static boolean game_in_progress(void) { return in_progress; }
static int32_t game_time_get(void) { return 7; }
static void build_message_header(message_header *h, word size, int a, int b) { (void)a; (void)b; *h = size; }
static void distributed_advance_checkpoint(void) { advances++; }
static void distributed_restore_checkpoint(void) { restores++; }
static short distributed_client_machines(long *machines, short capacity)
{ (void)capacity; machines[0] = 1; machines[1] = 2; return 2; }
static void distributed_send_to_machine_reliably(long machine, void *message, byte type, short count, word size)
{ (void)machine; (void)message; (void)count; (void)size; assert(type == _distributed_message_checkpoint); checkpoints_sent++; }
static void network_objects_host_reverted(void) { host_reverts++; }
void distributed_checkpoint_to_machine(long machine_index);
''' + functions + r'''
static boolean fill(byte type)
{
    struct distributed_message_header message;
    return distributed_fill_header(&message, type, 1, (word)sizeof(message));
}
int main(void)
{
    unsigned version;
    for (version = 38; version <= 43; version++)
    {
        int epoch = version >= 42;
        host_version = version;
        advances = restores = checkpoints_sent = host_reverts = 0;
        distributed_checkpoint_synchronized = FALSE;

        assert((network_distributed_wire_epoch_size() != 0) == epoch);

        /* a client not yet synchronized: play held, sends gated, only from 42 */
        connection = _game_connection_network_client;
        assert(distributed_checkpoint_ready() == !epoch);
        assert(fill(_distributed_message_unit_states) == !epoch);
        assert(fill(_distributed_message_client_ready));
        distributed_checkpoint_synchronized = TRUE;
        assert(distributed_checkpoint_ready() && fill(_distributed_message_unit_states));
        distributed_checkpoint_synchronized = FALSE;

        /* the host's co-op revert: inert below 42 */
        connection = _game_connection_network_server;
        network_distributed_reverted();
        assert(advances == epoch && restores == epoch && host_reverts == epoch);
        assert(checkpoints_sent == (epoch ? 2 : 0));
        distributed_checkpoint_to_machine(1);
        assert(checkpoints_sent == (epoch ? 3 : 0));
    }
    return 0;
}
'''
    program = program.replace("VERSION_HEADER", str(ROOT / "port/linux/include/halo_network_version.h"))
    (tmp_path / "probe.c").write_text(program)
    executable = tmp_path / "probe"
    subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-Wno-unused-function",
                    "-Wno-unused-variable", "-fsanitize=address,undefined",
                    str(tmp_path / "probe.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


def test_client_tick_stops_before_predictions_only_while_the_epoch_holds_play():
    """network_objects_client_tick's early return hangs on
    distributed_checkpoint_ready, which is always true below 42 (see above),
    and comes after the ready request but before every prediction."""
    tick = block((ROOT / "port/linux/game/network_objects.c").read_text(),
                 "void network_objects_client_tick(")
    ready = tick.index("_distributed_message_client_ready")
    gate = tick.index("if (!distributed_checkpoint_ready())\n\t\treturn;")
    first_prediction = tick.index("distributed_client_ready_picked_up_weapons();")
    assert ready < gate < first_prediction
    assert tick.count("return;") == 1
