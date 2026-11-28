#!/usr/bin/env python3
"""Exercise the actual network25 light sender/receiver code in a small world."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from probekit import source as probe_source  # noqa: E402

PATH = 'port/linux/game/network_objects.c'
source = probe_source.read(PATH)


flags = probe_source.enumeration(PATH, '_distributed_object_carried_bit')
driver = probe_source.function(PATH, 'distributed_unit_driven_here')
send = probe_source.block(PATH, 'if (TEST_FLAG(_object_mask_unit, object->object.type))\n\t{\n\t\tSET_FLAG(state->flags, _distributed_object_light_sent_bit')
receive = probe_source.block(PATH, 'if (TEST_FLAG(state->flags, _distributed_object_light_sent_bit) &&')
program = r'''
#include <assert.h>
#include <stdio.h>
typedef int boolean;
#define NONE (-1)
#define TRUE 1
#define TEST_FLAG(bits, bit) (((bits) >> (bit)) & 1)
#define SET_FLAG(bits, bit, value) ((bits) = ((bits) & ~(1u << (bit))) | ((!!(value)) << (bit)))
enum { _object_mask_unit = 3, _unit_integrated_light_on_bit = 7 };
struct unit_datum { struct { long player_index, driver_object_index; unsigned flags; } unit; };
struct object_datum { struct { int type; } object; };
struct distributed_object_state { long object_index; unsigned flags; };
static struct unit_datum units[2];
static struct unit_datum *unit_get(long index) { assert(index >= 0 && index < 2); return &units[index]; }
static boolean distributed_player_is_local(long player) { return player == 42; }
''' + flags + driver + '''
static void encode(struct distributed_object_state *state, struct object_datum *object, long object_index) {
''' + send + '''
}
static void decode(struct distributed_object_state *state, struct object_datum *object) {
''' + receive + r'''
}
int main(void) {
    struct object_datum object = { { 0 } };
    struct distributed_object_state state = { 0, 0 };
    units[0].unit.player_index = 8;
    units[0].unit.driver_object_index = NONE;
    units[1].unit.driver_object_index = NONE;
    for (int light = 0; light < 2; ++light) {
        state.flags = 0;
        units[0].unit.flags = light << _unit_integrated_light_on_bit;
        encode(&state, &object, 0);
        assert(TEST_FLAG(state.flags, _distributed_object_light_sent_bit));
        assert(TEST_FLAG(state.flags, _distributed_object_light_on_bit) == light);
        units[0].unit.flags = (!light) << _unit_integrated_light_on_bit;
        decode(&state, &object);
        assert(TEST_FLAG(units[0].unit.flags, _unit_integrated_light_on_bit) == light);
    }
    state.flags = 0; /* no light field: retain the existing state */
    units[0].unit.flags = 1 << _unit_integrated_light_on_bit;
    decode(&state, &object);
    assert(TEST_FLAG(units[0].unit.flags, _unit_integrated_light_on_bit));
    state.flags = 1 << _distributed_object_light_sent_bit; /* remote says off */
    units[0].unit.player_index = 42;
    decode(&state, &object);
    assert(TEST_FLAG(units[0].unit.flags, _unit_integrated_light_on_bit));
    units[0].unit.player_index = NONE;
    units[0].unit.driver_object_index = 1;
    units[1].unit.player_index = 42; /* locally driven vehicle retains prediction */
    decode(&state, &object);
    assert(TEST_FLAG(units[0].unit.flags, _unit_integrated_light_on_bit));
    units[1].unit.player_index = 8; /* remote driver accepts host state */
    decode(&state, &object);
    assert(!TEST_FLAG(units[0].unit.flags, _unit_integrated_light_on_bit));
    object.object.type = 2; /* nonunit: neither send nor apply light bits */
    state.flags = 0;
    encode(&state, &object, 0);
    assert(state.flags == 0);
    state.flags = (1 << _distributed_object_light_sent_bit) | (1 << _distributed_object_light_on_bit);
    decode(&state, &object);
    assert(units[0].unit.flags == 0);
    puts("PASS: network25 light payload, remote correction and local prediction");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'probe.c').write_text(program)
    subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-fsanitize=address,undefined',
                    str(path / 'probe.c'), '-o', str(path / 'probe')], check=True)
    subprocess.run([str(path / 'probe')], check=True)
