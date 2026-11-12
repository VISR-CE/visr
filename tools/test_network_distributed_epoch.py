"""Distributed wire framing: byte-identical below 42, an epoch after the header from 42."""
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


def test_wire_copy_is_identity_below_42_and_round_trips_the_epoch(tmp_path):
    source = (ROOT / "port/linux/game/network_distributed.c").read_text()
    header = (ROOT / "port/linux/game/network_distributed.h").read_text()
    structures = block(header, "struct distributed_message_header\n{").replace(
        "long game_time;", "int32_t game_time;") + ";\n"
    build = block((ROOT / "source/bungie_net/common/message_header.c").read_text(),
                  "void build_message_header(")
    epoch_size = block(source, "unsigned long network_distributed_wire_epoch_size(")
    wire_copy = block(source, "word network_distributed_wire_copy(")
    strip = block(source, "static boolean distributed_strip_epoch(")
    program = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "VERSION_HEADER"
typedef unsigned char byte;
typedef uint16_t word;
typedef uint16_t message_header;
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define csmemcpy memcpy
#define GET_MESSAGE_FLAGS(value) ((value) & 3)
#define GET_MESSAGE_TYPE(value) (((value) >> 2) & 3)
#define GET_MESSAGE_SIZE(value) ((value) >> 4)
#define match_assert(file, line, condition) assert(condition)
enum { MAXIMUM_MESSAGE_SIZE = 0xfff, NUMBER_OF_MESSAGE_TYPES = 4, MESSAGE_FLAG_BITS_MASK = 3 };
''' + structures + r'''
static unsigned host_version = 38;
static unsigned network_game_host_version(void) { return host_version; }
static unsigned int distributed_checkpoint_epoch;
''' + build + "\n" + epoch_size + "\n" + wire_copy + "\n" + strip + r'''
static word make_message(byte *message, word entries)
{
    struct distributed_message_header header = {0};
    word size = (word)(sizeof(header) + entries);
    header.type = 7;
    header.count = 2;
    header.game_time = 0x01020304;
    build_message_header(&header.header, size, 2, 0);
    memcpy(message, &header, sizeof(header));
    for (word i = 0; i < entries; i++)
        message[sizeof(header) + i] = (byte)(0xa0 + i);
    return size;
}
int main(void)
{
    byte message[64], wire[80];
    word stripped[48], stripped_size;
    unsigned epoch;
    word size = make_message(message, 20);

    /* 29-41: no epoch, the bytes on the wire are the message's own,
       whatever epoch this machine holds. */
    for (unsigned version = 29; version <= 41; version++) {
        host_version = version;
        distributed_checkpoint_epoch = version * 7;
        assert(network_distributed_wire_epoch_size() == 0);
        memset(wire, 0xee, sizeof(wire));
        assert(network_distributed_wire_copy(wire, sizeof(wire), message, size) == size);
        assert(memcmp(wire, message, size) == 0);
        assert(wire[size] == 0xee);
    }

    /* 42 and 43: the epoch, little-endian, after the 8-byte header; the
       transport header's size covers it. */
    for (unsigned version = 42; version <= 43; version++) {
        host_version = version;
        distributed_checkpoint_epoch = 0x0a0b0c0d;
        assert(network_distributed_wire_epoch_size() == 4);
        word wire_size = network_distributed_wire_copy(wire, sizeof(wire), message, size);
        assert(wire_size == size + 4);
        assert(GET_MESSAGE_SIZE(*(word *)wire) == wire_size);
        assert(memcmp(wire + 2, message + 2, 6) == 0);
        assert(wire[8] == 0x0d && wire[9] == 0x0c && wire[10] == 0x0b && wire[11] == 0x0a);
        assert(memcmp(wire + 12, message + 8, size - 8) == 0);

        /* and back: the same message, the same epoch */
        assert(distributed_strip_epoch((word *)wire, wire_size, stripped, &stripped_size, &epoch));
        assert(stripped_size == size && epoch == 0x0a0b0c0d);
        assert(memcmp(stripped, message, size) == 0);

        /* too short to carry an epoch, or no room for one */
        assert(!distributed_strip_epoch((word *)wire, 11, stripped, &stripped_size, &epoch));
        assert(distributed_strip_epoch((word *)wire, 12, stripped, &stripped_size, &epoch));
        assert(stripped_size == 8);
        assert(network_distributed_wire_copy(wire, (word)(size + 3), message, size) == 0);
        assert(network_distributed_wire_copy(wire, sizeof(wire), message, 7) == 0);
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
