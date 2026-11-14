#!/usr/bin/env python3
"""The lobby matches a game's map by scenario name, and says "1 machine" in the singular."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MENU = (ROOT / 'port/linux/game/menu_functions.c').read_text()


def function_body(signature):
    body = MENU[MENU.index(signature):]
    return body[:body.index('\n}\n')]


def test_lobby_map_show_matches_bare_names():
    body = function_body('static void lobby_map_show(')
    assert '_stricmp(scenario_name(names[index]), scenario_name(map_name))' in body, \
        'the map list holds scenario paths; a game advertising a bare map name must still match'


def test_machine_count_is_singular_for_one():
    assert 'on %d machines' not in MENU, 'a fixed plural prints "on 1 machines"'
    assert MENU.count('machine%s') >= 2


if __name__ == '__main__':
    test_lobby_map_show_matches_bare_names()
    test_machine_count_is_singular_for_one()
