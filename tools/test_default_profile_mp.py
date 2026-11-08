"""A built-in default profile stores no name; it must still be able to play multiplayer."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import SkipTest

from test_mp_synthetic_input import function

ROOT = Path(__file__).resolve().parents[1]
HANDLERS = (ROOT / "source/interface/ui_widget_event_handler_functions.c").read_text()
CLIENT = (ROOT / "source/networking/network_client_manager.c").read_text()
PLAYERS = (ROOT / "source/game/players.c").read_text()

# The two lobby checks, run for a default profile (empty stored name) and a
# made one, with the real player_name_valid and player_name_clean bodies.
HARNESS = r'''
#include <assert.h>
#include <wchar.h>
#include <string.h>
#define TRUE 1
#define FALSE 0
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
#define TEST_FLAG(flags, bit) (((flags) >> (bit)) & 1)
typedef int boolean;
typedef wchar_t unicode;
enum {_player_profile_default_profile_bit = 0};
struct player_profile {wchar_t player_name[12]; unsigned short flags;};
static wchar_t player_name_character_ascii(wchar_t c) {return c < 0x80 ? c : '?';}
__CLEAN__
__VALID__
static boolean lobby_accepts(struct player_profile const *profile)
{
	return __CONDITION__;
}
int main(void)
{
	struct player_profile profile;

	memset(&profile, 0, sizeof(profile));
	profile.flags = 1;
	assert(lobby_accepts(&profile) == __DEFAULT_OK__);
	profile.flags = 0;
	wcscpy(profile.player_name, L"Chief");
	assert(lobby_accepts(&profile));
	profile.player_name[0] = 0;
	assert(!lobby_accepts(&profile));
	return 0;
}
'''


def _condition(text):
    match = re.search(r"if \((!TEST_FLAG\(profile\.flags, _player_profile_default_profile_bit\) &&\s*)?!player_name_valid\(profile\.player_name", text)
    return match.group(1) is not None


def _run(default_ok):
    compiler = shutil.which("cc") or shutil.which("clang")
    if not compiler:
        raise SkipTest("no C compiler")
    clean = function(PLAYERS, "boolean player_name_clean(")
    valid = function(PLAYERS, "boolean player_name_valid(")
    condition = (
        "(TEST_FLAG(profile->flags, _player_profile_default_profile_bit) || "
        "player_name_valid(profile->player_name, NUMBEROF(profile->player_name)))"
        if default_ok else
        "player_name_valid(profile->player_name, NUMBEROF(profile->player_name))")
    source = (HARNESS.replace("__CLEAN__", clean).replace("__VALID__", valid)
              .replace("__CONDITION__", condition).replace("__DEFAULT_OK__", "1" if default_ok else "0"))
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "probe.c"
        path.write_text(source)
        binary = Path(directory) / "probe"
        subprocess.run([compiler, "-std=gnu99", "-w", "-o", str(binary), str(path)], check=True)
        return subprocess.run([str(binary)]).returncode


def test_default_profile_was_refused_before_and_is_accepted_after():
    # the old check (no default-profile exemption) refuses a default profile
    assert _run(False) == 0
    # the exemption the lobby now uses accepts it, and still refuses a made profile with no name
    assert _run(True) == 0


def test_both_lobby_checks_skip_name_validation_for_default_profiles():
    for block in HANDLERS.split("player_name_valid(profile.player_name")[:-1]:
        assert block.rstrip().endswith("_player_profile_default_profile_bit) &&\n\t\t\t!") or \
            block.rstrip().endswith("_player_profile_default_profile_bit) &&\n\t\t!"), block[-120:]
    assert HANDLERS.count("player_name_valid(profile.player_name") == 2


def test_join_request_keeps_the_stored_name_so_the_host_names_a_default_profile():
    assert "ustrncpy(player.name, profile.player_name, NETWORK_PLAYER_NAME_LENGTH - 1);" in CLIENT
    assert "player_profile_multiplayer_name" not in CLIENT
    assert "player_profile_multiplayer_name" not in HANDLERS
