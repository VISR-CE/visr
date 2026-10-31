"""Fireteam policy and checkpoint membership through actual save/death code."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, mutated, read, run
from harness.coop_wipe import generated


@pytest.mark.parametrize('case', ['policy', 'authority', 'roster', 'save', 'new-map', 'delay', 'failure'])
def test_team_wipe(case):
    status, output = run(build('coop_wipe', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('before,after,case', [
    ('!coop_host() || config_boolean("network.coop_late_join")', 'config_boolean("network.coop_late_join")', 'authority'),
    ('config_boolean("network.coop_late_join")', '!config_boolean("network.coop_late_join")', 'policy'),
    ('if (!game_state_port_saved_game_valid() || !checkpoint_players_match())',
     'if (FALSE)', 'roster'),
    ('!(main_loss_scripted && network_coop_active())', 'TRUE', 'failure'),
    ('main_loss_scripted = FALSE;', '', 'failure'),
    ('remove_quitting_players_from_game();', '', 'roster'),
    ('if (!checkpoint_players_match())\n\t\treturn;', '', 'roster'),
    ('csmemset(&checkpoint_players, 0, sizeof(checkpoint_players));', '', 'new-map'),
    ('if (game_state_globals.saved_game_valid)\n\t\tnetwork_coop_checkpoint_saved();',
     'network_coop_checkpoint_saved();', 'save'),
])
def test_negative_control(before, after, case):
    includes = tuple((name, mutated(code, before, after) if name == 'under_test.inc' else code)
                     for name, code in generated())
    status, output = run(build('coop_wipe', includes), case)
    assert status == CHECK_FAILED, output


def test_scripted_loss_keeps_its_reason():
    assert 'HS_EVALUATE_NO_ARGUMENTS(main_lost_map_evaluate, main_lost_map_scripted)' in read('source/hs/hs.c')
