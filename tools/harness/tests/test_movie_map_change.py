"""A map load or precache while a movie plays ends the movie first (game_precache_new_map,
cache_files_precache_map_begin), so the precache does not take the texture
cache's memory the movie holds and scenario_unload does not halt on !bink_playback_active(); the intro's return to the
main menu does not run in the middle of the load."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run

BINK = read("source/bink/bink_playback.c")
GAME = read("source/game/game.c")
SCENARIO = read("source/scenario/scenario.c")
CACHE = read("source/cache/cache_files_windows.c")
FLAGS = enum_with(read("source/bink/bink_playback.h"), "_bink_playback_return_to_main_menu_when_finished_bit")
GAME_CALL = "\n\tbink_playback_stop_for_map_change();\n"
BEGIN_CALL = "\t\t\tbink_playback_stop_for_map_change();\n"


def generated(fault=None):
    code = "\n".join([
        function(BINK, "bink_playback_active"),
        function(BINK, "bink_playback_stop"),
        function(BINK, "bink_playback_stop_for_map_change"),
        function(CACHE, "cache_files_precache_map_begin"),
        function(GAME, "game_precache_new_map"),
        function(SCENARIO, "scenario_unload"),
    ])
    for before, after in (fault if fault and isinstance(fault[0], tuple) else [fault] if fault else []):
        code = mutated(code, before, after)
    return (("under_test.inc", code), ("bink_flags.inc", FLAGS))


@pytest.mark.parametrize("case", ["intro", "intro-map-already-read", "lobby-precache", "no-movie", "skip"])
def test_movie_map_change(case):
    status, output = run(build("movie_map_change", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("fault,case", [
    ((GAME_CALL, "\n"), "intro-map-already-read"),
    ((BEGIN_CALL, ""), "lobby-precache"),
    (((GAME_CALL, "\n"), (BEGIN_CALL, "")), "intro"),
    (("\tSET_FLAG(bink_globals.flags, _bink_playback_return_to_main_menu_when_finished_bit, FALSE);\n", ""), "intro"),
])
def test_negative_control(fault, case):
    status, output = run(build("movie_map_change", generated(fault)), case)
    assert status == CHECK_FAILED, output
