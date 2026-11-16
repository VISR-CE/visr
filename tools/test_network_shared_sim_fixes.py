"""Check the version gate for network version 42's shared simulation fixes."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(path, name):
    source = (ROOT / path).read_text()
    match = re.search(r"(?m)^(?:static )?[^;\n]+\b" + re.escape(name) + r"\([^;]*?\)\s*\{", source)
    assert match, f"Production definition missing: {name}"
    start = source.index("{", match.start())
    end, depth = start + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


PROBE = r"""
#include <stdio.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#include "VERSION_HEADER"
enum { _game_connection_local, _game_connection_network_server, _game_connection_network_client };
static short connection;
static unsigned int host_version;
static boolean replaying;
static short game_connection(void) { return connection; }
static unsigned int network_game_host_version(void) { return host_version; }
static unsigned char input_replay_playing(void) { return replaying; }
GATE
int main(void)
{
    static struct { short connection; unsigned int version; boolean replay; boolean expected; } cases[] = {
        { _game_connection_local, 43, FALSE, FALSE },
        { _game_connection_local, 38, FALSE, FALSE },
        { _game_connection_network_client, 41, FALSE, FALSE },
        { _game_connection_network_client, 38, FALSE, FALSE },
        { _game_connection_network_server, 38, FALSE, FALSE },
        { _game_connection_network_client, 42, FALSE, TRUE },
        { _game_connection_network_client, 43, FALSE, TRUE },
        { _game_connection_network_server, 43, FALSE, TRUE },
        { _game_connection_network_client, 43, TRUE, FALSE },
    };
    int failures = 0;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        connection = cases[i].connection;
        host_version = cases[i].version;
        replaying = cases[i].replay;
        if (!network_game_shared_sim_fixes() != !cases[i].expected)
        {
            printf("case %u wrong\n", i);
            failures++;
        }
    }
    return failures;
}
"""


def test_shared_sim_fix_gate():
    gate = function("source/networking/network_client_manager.c", "network_game_shared_sim_fixes")
    probe = PROBE.replace("VERSION_HEADER", str(ROOT / "port/linux/include/halo_network_version.h")).replace("GATE", gate)
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory) / "probe.c"
        source.write_text(probe)
        executable = Path(directory) / "probe"
        subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-Wno-unused-function", "-o", str(executable), str(source)], check=True)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout


def test_boat_controls_start_from_zero_only_under_the_gate():
    boat = function("source/units/vehicles.c", "update_human_boat_physics")
    gated = "if (network_game_shared_sim_fixes())\n\t\t\tcsmemset(state, 0, 3 * sizeof(*state));"
    assert boat.count(gated) == 1
    assert boat.index(gated) < boat.index("speed = (real)fabs(")
    assert boat.count("csmemset(state") == 1
