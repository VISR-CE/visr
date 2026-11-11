"""A host at the main menu ticks only as far as its update queue.

Hosting a game from the multiplayer menus makes the connection a network
server while the menu backdrop is still loaded. The host's update queue stays
empty until the game starts, so a tick taken on local time finds no update and
halts in `players_update_before_game` (players.c #779).
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def _tick_bound(source):
    start = source.index("if (game_connection() == _game_connection_network_client)\n\t\t\t\t{\n\t\t\t\t\tmaximum_possible_server_time")
    return source[start:source.index("if (maximum_possible_server_time > game_time_globals->server_time)", start)]


def test_only_a_client_ticks_without_queued_updates():
    bound = _tick_bound((ROOT / "source/game/game_time.c").read_text())
    code = "\n".join(line for line in bound.splitlines() if not line.strip().startswith(("/*", "(", "or not", "with no")))
    # every connection other than a network client takes its bound from the update queue
    assert "else\n\t\t\t\t\tmaximum_possible_server_time = update_client_get_maximum_possible_server_time();" in bound
    assert "main_menu_is_active" not in code
    assert code.count("maximum_possible_server_time =") == 2
