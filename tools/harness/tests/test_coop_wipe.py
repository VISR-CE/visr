"""Host visibility policy and checkpoint membership through actual save/death code."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, mutated, run
from harness.coop_wipe import generated


@pytest.mark.parametrize('case', ['start-visibility', 'policy', 'authority', 'roster', 'save', 'new-map', 'delay', 'failure'])
def test_team_wipe(case):
    status, output = run(build('coop_wipe', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('before,after,case', [
    ('p2p_cooperative_game_start();', '', 'start-visibility'),
    ('if (!coop_host())\n\t\treturn FALSE;', 'if (FALSE) return FALSE;', 'authority'),
    ('strcmp(mode, "revert") && p2p_hosting_listed()', 'strcmp(mode, "revert") && !p2p_hosting_listed()', 'policy'),
    ('if (!game_state_port_saved_game_valid() || !checkpoint_players_match())',
     'if (!game_state_port_saved_game_valid())', 'roster'),
    ('csmemset(&checkpoint_players, 0, sizeof(checkpoint_players));', '', 'new-map'),
    ('if (game_state_globals.saved_game_valid)\n\t\tnetwork_coop_checkpoint_saved();',
     'network_coop_checkpoint_saved();', 'save'),
])
def test_negative_control(before, after, case):
    includes = tuple((name, mutated(code, before, after) if name == 'under_test.inc' else code)
                     for name, code in generated())
    status, output = run(build('coop_wipe', includes), case)
    assert status == CHECK_FAILED, output
