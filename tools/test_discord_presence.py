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
    assert(!strcmp(value.details, "In the menus"));
    assert(!value.state[0] && !value.players && !value.capacity && !value.started);
    discord_presence_make(&value, "Blood Gulch", "Multiplayer", 4, 0, 123);
    assert(value.players == 4 && value.capacity == 0);
    assert(!strcmp(value.state, "4 players"));
    discord_presence_make(&value, "Blood Gulch", "Slayer", 20, 4, 0);
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


def test_live_snapshot_map_boundaries_and_public_labels(tmp_path):
    compiler = shutil.which('cc')
    if not compiler:
        pytest.skip('C compiler unavailable')
    probe = tmp_path / 'snapshot.c'
    probe.write_text('''
#include "host_discord_presence.h"
#include <assert.h>
#include <string.h>
int main(void) {
    struct discord_presence value;
    assert(!host_discord_presence_menu_ready());
    host_discord_presence_snapshot(&value);
    assert(!strcmp(value.details, "In the menus"));
    host_discord_presence_map("levels\\\\test\\\\bloodgulch\\\\bloodgulch");
    host_discord_presence_context(2, 2);
    host_discord_presence_context(4, 4);
    host_discord_presence_context(24, 2);
    host_discord_presence_context(25, 16);
    host_discord_presence_snapshot(&value);
    assert(!strcmp(value.details, "Halo: Combat Evolved — Blood Gulch, Slayer"));
    assert(!strcmp(value.state, "4/16 players"));
    assert(value.players == 4 && value.capacity == 16 && value.started > 0);
    host_discord_presence_context(24, 1);
    host_discord_presence_snapshot(&value);
    assert(strstr(value.details, "Capture the Flag"));
    host_discord_presence_context(24, 3);
    host_discord_presence_snapshot(&value);
    assert(strstr(value.details, "Oddball"));
    host_discord_presence_context(24, 4);
    host_discord_presence_snapshot(&value);
    assert(strstr(value.details, "King of the Hill"));
    host_discord_presence_context(24, 5);
    host_discord_presence_snapshot(&value);
    assert(strstr(value.details, "Race"));
    host_discord_presence_context(25, 500);
    host_discord_presence_snapshot(&value);
    assert(value.capacity == 0 && value.players == 4);
    int64_t started = value.started;
    host_discord_presence_context(4, 5);
    host_discord_presence_snapshot(&value);
    assert(value.started == started && value.players == 5);
    host_discord_presence_map("levels\\\\a10\\\\a10");
    host_discord_presence_context(2, 1);
    host_discord_presence_context(4, 1);
    host_discord_presence_snapshot(&value);
    assert(!strcmp(value.details, "Halo: Combat Evolved — The Pillar of Autumn, Campaign"));
    host_discord_presence_context(4, 2);
    host_discord_presence_snapshot(&value);
    assert(!strcmp(value.details, "Halo: Combat Evolved — The Pillar of Autumn, Co-op"));
    host_discord_presence_map("/private/path/unknown.map");
    host_discord_presence_snapshot(&value);
    assert(!strstr(value.details, "private") && strstr(value.details, "Custom map"));
    assert(!value.players && !value.capacity);
    host_discord_presence_loading(1);
    host_discord_presence_map("levels/ui/ui");
    assert(!host_discord_presence_menu_ready());
    host_discord_presence_loading(0);
    assert(host_discord_presence_menu_ready());
    host_discord_presence_movie(1);
    assert(!host_discord_presence_menu_ready());
    host_discord_presence_movie(0);
    assert(host_discord_presence_menu_ready());
    host_discord_presence_map("a10");
    assert(!host_discord_presence_menu_ready());
    host_discord_presence_map("levels/ui/ui");
    host_discord_presence_context(4, 4);
    host_discord_presence_snapshot(&value);
    assert(!strcmp(value.details, "In the menus"));
    assert(!value.players && !value.capacity && !value.started);
}
''')
    executable = tmp_path / 'snapshot'
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-pthread',
                    '-I', str(ROOT / 'port/runtime'), '-I', str(ROOT / 'port/ios/host'),
                    str(probe), str(ROOT / 'port/runtime/discord_presence.c'),
                    str(ROOT / 'port/ios/host/host_discord_presence.c'),
                    '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
