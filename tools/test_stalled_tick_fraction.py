"""A frame whose due ticks did not run draws the latest tick.

A host's update queue stays empty while a network pregame waits at the menu
backdrop, so its clock comes due without ticking. If the render fraction kept
sweeping from 0 to 1 between the same two snapshots on every stalled tick, the
backdrop would jitter back and forth in time behind the pregame screens.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "source/game/game_time.c").read_text()


def _function(name):
    start = SOURCE.index(f"\n{name}(")
    return SOURCE[start:SOURCE.index("\n}\n", start)]


def test_stalled_ticks_hold_the_fraction_at_the_latest_tick():
    fraction = _function("real game_time_get_tick_fraction")
    assert "if (game_time_ticks_stalled)\n\t\treturn 1.0f;" in fraction


def test_the_stall_is_decided_by_whether_any_tick_ran():
    update = _function("void game_time_update")
    before = update.index("long server_time_before = game_time_globals->server_time;")
    loop = update.index("game_time_globals->server_time++;")
    stall = update.index("game_time_ticks_stalled = (game_time_globals->server_time == server_time_before);")
    assert before < loop < stall


def test_a_new_game_clock_starts_unstalled():
    assert "game_time_ticks_stalled = FALSE;" in _function("void game_time_start")
