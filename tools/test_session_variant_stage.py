"""The playlist stage follows the gametype the menus give a running server.

Rounds after the first are set up from the playlist stage
(`network_game_server_reset_to_pregame` -> `setup_game_from_playlist`), and the
server keeps player_ui's session options (Server Setup's vehicle set and the
like) only when the stage's variant matches player_ui's. Every menu path that
hands the server a gametype therefore overrides the stage's variant too.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EVENTS = ROOT / 'source/interface/ui_widget_event_handler_functions.c'


def server_blocks(text):
    """Each `if (server)` block that changes the server's game variant."""
    blocks = []
    for match in re.finditer(r'if \(server\)\s*\{', text):
        depth, index = 1, match.end()
        while depth:
            depth += {'{': 1, '}': -1}.get(text[index], 0)
            index += 1
        body = text[match.end():index]
        if 'network_game_server_change_game_variant(' in body:
            blocks.append(body)
    return blocks


def test_menu_variant_changes_override_the_stage():
    text = EVENTS.read_text()
    # (co-op's gametype has no game engine; cooperative_round sets its rounds up)
    blocks = server_blocks(text)
    assert len(blocks) == 3, 'a gametype change outside an `if (server) { ... }` block'
    for body in blocks:
        override = body.find('game_engine_override_game_variant(')
        change = body.find('network_game_server_change_game_variant(')
        assert 0 <= override < change, body


if __name__ == '__main__':
    test_menu_variant_changes_override_the_stage()
