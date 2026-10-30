"""The selected visibility survives password and listing transport changes."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, function, read, run


def test_chosen_visibility():
    source = read('port/linux/src/p2p_lobby.c')
    code = function(source, 'p2p_hosting_listed') + function(source, 'p2p_set_hosting_public')
    status, output = run(build('p2p_visibility', (('under_test.inc', code),)), '')
    assert status == 0, output
