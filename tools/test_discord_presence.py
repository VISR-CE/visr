"""Exercise the presence value producer without game data or Discord."""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[1]


def test_presence_menu_party_bounds_and_labels(tmp_path):
    compiler = shutil.which('cc')
    if not compiler:
        pytest.skip('C compiler unavailable')
    probe = tmp_path / 'probe.c'
    probe.write_text('''
#include "discord_presence.h"
#include <assert.h>
#include <string.h>
int main(void) {
    struct discord_presence value;
    discord_presence_make(&value, "Blood Gulch", "Slayer", 4, 16, 123);
    assert(!strcmp(value.name, "VISR: CE"));
    assert(!strcmp(value.details, "Halo: Combat Evolved — Blood Gulch, Slayer"));
    assert(!strcmp(value.image, "visr"));
    assert(!strcmp(value.state, "4/16 players"));
    assert(value.players == 4 && value.capacity == 16 && value.started == 123);
    discord_presence_make(&value, 0, 0, -1, -1, -1);
    assert(!strcmp(value.details, "Halo: Combat Evolved — Main menu, Menu"));
    assert(!value.state[0] && !value.players && !value.capacity && !value.started);
    discord_presence_make(&value, "", "", 20, 4, 0);
    assert(value.players == 4);
    char label[1024]; memset(label, 'a', sizeof(label)); label[1023] = 0;
    discord_presence_make(&value, label, label, 1, 1, 0);
    assert(strlen(value.details) < sizeof(value.details));
}
''')
    executable = tmp_path / 'probe'
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I', str(ROOT / 'port/runtime'), str(probe),
                    str(ROOT / 'port/runtime/discord_presence.c'), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
