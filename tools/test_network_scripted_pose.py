"""Scripted player poses on the wire: a 42/43 extension, ignored below 42."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from probekit import source as probe_source  # noqa: E402

DISTRIBUTED = "port/linux/game/network_distributed.c"
HEADER = "port/linux/game/network_distributed.h"


def test_scripted_pose_round_trips_from_42_and_is_ignored_below(tmp_path):
    source = (ROOT / "port/linux/game/network_distributed.c").read_text()
    start = source.index("/* struct distributed_unit_state flags */")
    enums = source[start:source.index("/* world units: how far", start)]
    state = probe_source.structure(DISTRIBUTED, "distributed_unit_state") + "\n"
    pieces = [
        probe_source.structure(HEADER, "distributed_vector") + "\n",
        enums,
        state,
        "enum { DISTRIBUTED_UNIT_STATE_MINIMUM_SIZE = 3 };\n",
        probe_source.function(DISTRIBUTED, "distributed_vector_pack"),
        probe_source.function(DISTRIBUTED, "distributed_unit_vector_pack"),
        probe_source.function(DISTRIBUTED, "distributed_vector_unpack"),
        probe_source.function(DISTRIBUTED, "distributed_put"),
        probe_source.function(DISTRIBUTED, "distributed_take"),
        probe_source.function(DISTRIBUTED, "distributed_scripted_poses"),
        probe_source.function(DISTRIBUTED, "distributed_scripted_pose_valid"),
        probe_source.function(DISTRIBUTED, "distributed_unit_state_write"),
        probe_source.function(DISTRIBUTED, "distributed_unit_state_read"),
    ]
    program = r'''
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "VERSION_HEADER"
typedef unsigned char byte;
typedef uint16_t word;
typedef int boolean;
typedef float real;
typedef struct { real x, y, z; } real_point3d;
typedef struct { real i, j, k; } real_vector3d;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define NO_PLAYER 0xff
#define csmemcpy memcpy
#define csmemset memset
#define TEST_FLAG(flags, bit) (((flags) >> (bit)) & 1)
#define SET_FLAG(flags, bit, value) ((value) ? ((flags) |= (1u << (bit))) : ((flags) &= ~(1u << (bit))))
#define DISTRIBUTED_UNIT_SCALE 32767.0f
enum { NUMBER_OF_PLAYER_POWERUPS = 2, TICKS_PER_SECOND = 30, MAXIMUM_ROUND_TRIP_TICKS = 30 };
static unsigned host_version = 38;
static unsigned network_game_host_version(void) { return host_version; }
''' + "\n".join(pieces) + r'''
static void alive(struct distributed_unit_state *state)
{
    memset(state, 0, sizeof(*state));
    state->player_index = 3;
    state->unit_index = 0x12340005;
    state->vehicle_index = NONE;
    state->seat_index = NONE;
    state->killing_player_index = NO_PLAYER;
    state->up.k = (short)DISTRIBUTED_UNIT_SCALE;
    state->body_vitality = 16384;
    SET_FLAG(state->flags, _distributed_unit_alive_bit, TRUE);
}
static void posed(struct distributed_unit_state *state)
{
    static const real_vector3d vectors[NUMBER_OF_DISTRIBUTED_POSE_VECTORS] = {
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0.6f, 0.8f, 0}, {0, -0.6f, 0.8f}};
    alive(state);
    state->scripted_pose = TRUE;
    SET_FLAG(state->unit_flags, _distributed_unit_suspended_bit, TRUE);
    for (int index = 0; index < NUMBER_OF_DISTRIBUTED_POSE_VECTORS; index++)
        distributed_vector_pack(&vectors[index], DISTRIBUTED_UNIT_SCALE, &state->pose[index]);
}
int main(void)
{
    struct distributed_unit_state state, back;
    byte buffer[256];
    word size, plain_size;

    assert(sizeof(((struct distributed_unit_state *)0)->pose) == 30);

    /* 42 and 43: the pose rides after the camouflage part, 30 bytes, and
       comes back as it went, suspended bit and all */
    for (unsigned version = 42; version <= 43; version++) {
        host_version = version;
        alive(&state);
        plain_size = distributed_unit_state_write(&state, FALSE, buffer);
        assert(!TEST_FLAG(buffer[2], _distributed_unit_part_scripted_pose_bit));
        assert(distributed_unit_state_read(buffer, buffer + plain_size, &back) == plain_size);
        assert(!back.scripted_pose);

        posed(&state);
        size = distributed_unit_state_write(&state, FALSE, buffer);
        assert(TEST_FLAG(buffer[2], _distributed_unit_part_scripted_pose_bit));
        assert(size == plain_size + 2 + 30); /* camouflage part, then the pose */
        assert(distributed_unit_state_read(buffer, buffer + size, &back) == size);
        assert(back.scripted_pose);
        assert(TEST_FLAG(back.unit_flags, _distributed_unit_suspended_bit));
        assert(memcmp(back.pose, state.pose, sizeof(state.pose)) == 0);

        /* short by a byte */
        assert(distributed_unit_state_read(buffer, buffer + size - 1, &back) == 0);

        /* a zero vector, or one far from unit length, drops only the pose:
           the state keeps its size and the rest of its parts */
        state.pose[_distributed_pose_looking].i = 0;
        state.pose[_distributed_pose_looking].j = 0;
        state.pose[_distributed_pose_looking].k = 0;
        size = distributed_unit_state_write(&state, FALSE, buffer);
        assert(distributed_unit_state_read(buffer, buffer + size, &back) == size);
        assert(!back.scripted_pose && back.scripted_pose_sent);
        assert(TEST_FLAG(back.unit_flags, _distributed_unit_suspended_bit));
        assert(back.body_vitality == 16384);
        posed(&state);
        state.pose[_distributed_pose_facing].i = 16000;
        size = distributed_unit_state_write(&state, FALSE, buffer);
        assert(distributed_unit_state_read(buffer, buffer + size, &back) == size);
        assert(!back.scripted_pose && back.scripted_pose_sent);

        /* two states in one message: a bad pose in the first leaves the
           second intact */
        {
            struct distributed_unit_state second;
            word first_size, second_size;
            alive(&second);
            second.player_index = 5;
            second.body_vitality = 1000;
            first_size = distributed_unit_state_write(&state, FALSE, buffer);
            second_size = distributed_unit_state_write(&second, FALSE, buffer + first_size);
            assert(distributed_unit_state_read(buffer, buffer + first_size + second_size, &back) == first_size);
            assert(distributed_unit_state_read(buffer + first_size, buffer + first_size + second_size, &back) == second_size);
            assert(back.player_index == 5 && back.body_vitality == 1000 && !back.scripted_pose_sent);
        }

        /* a pose on a dead unit is refused */
        memset(buffer, 0, sizeof(buffer));
        buffer[0] = 3;
        buffer[1] = 0;
        buffer[2] = 1u << _distributed_unit_part_scripted_pose_bit;
        assert(distributed_unit_state_read(buffer, buffer + 3, &back) == 0);
    }

    /* the sender normalizes each direction and refuses a zero one */
    {
        real_vector3d long_vector = {3, 4, 0}, zero = {0, 0, 0}, back_vector;
        struct distributed_vector packed;
        assert(distributed_unit_vector_pack(&long_vector, &packed));
        distributed_vector_unpack(&packed, DISTRIBUTED_UNIT_SCALE, &back_vector);
        assert(fabsf(back_vector.i - 0.6f) < 0.001f && fabsf(back_vector.j - 0.8f) < 0.001f);
        assert(!distributed_unit_vector_pack(&zero, &packed));
    }

    /* 29-41: the part bit means nothing, nothing past the old parts is
       read or written, and a state with no pose writes the same bytes on every version */
    for (unsigned version = 29; version <= 41; version++) {
        host_version = version;
        posed(&state);
        size = distributed_unit_state_write(&state, FALSE, buffer);
        assert(!TEST_FLAG(buffer[2], _distributed_unit_part_scripted_pose_bit));
        assert(distributed_unit_state_read(buffer, buffer + size, &back) == size);
        assert(!back.scripted_pose);

        byte old[256], new_[256];
        alive(&state);
        plain_size = distributed_unit_state_write(&state, FALSE, old);
        host_version = 43;
        assert(distributed_unit_state_write(&state, FALSE, new_) == plain_size);
        assert(memcmp(old, new_, plain_size) == 0);
        host_version = version;
        memset(buffer, 0, sizeof(buffer));
        buffer[0] = 3;
        buffer[2] = 1u << _distributed_unit_part_scripted_pose_bit;
        assert(distributed_unit_state_read(buffer, buffer + 3, &back) == 3);
    }
    return 0;
}
'''
    program = program.replace("VERSION_HEADER", str(ROOT / "port/linux/include/halo_network_version.h"))
    (tmp_path / "probe.c").write_text(program)
    executable = tmp_path / "probe"
    subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-Wno-unused-function",
                    "-fsanitize=address,undefined", str(tmp_path / "probe.c"), "-o", str(executable)],
                   check=True)
    subprocess.run([str(executable)], check=True)


def test_scripted_pose_apply_and_prediction_are_gated():
    source = (ROOT / "port/linux/game/network_distributed.c").read_text()
    apply = probe_source.function(DISTRIBUTED, "distributed_scripted_pose_apply")
    assert "if (!distributed_scripted_poses())" in apply
    assert "if (!state->scripted_pose && " in probe_source.function(DISTRIBUTED, "distributed_apply_state")
    handle = probe_source.function(DISTRIBUTED, "distributed_handle_unit_state")
    assert "distributed_scripted_pose_apply(" in handle
    from_unit = probe_source.function(DISTRIBUTED, "distributed_scripted_pose_from_unit")
    assert "_game_connection_network_server" in from_unit and "distributed_scripted_poses()" in from_unit
    # a host refuses a client's state that carried a pose, valid or not
    assert "state.scripted_pose_sent ||" in source
