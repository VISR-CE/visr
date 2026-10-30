"""Effective visibility respects browser enablement, independent of passwords and transport."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run


def test_chosen_visibility():
    source = read('port/linux/src/p2p_lobby.c')
    code = function(source, 'p2p_hosting_listed') + function(source, 'p2p_set_hosting_public')
    status, output = run(build('p2p_visibility', (('under_test.inc', code),)), '')
    assert status == 0, output


def test_visibility_negative_control():
    source = read('port/linux/src/p2p_lobby.c')
    code = function(source, 'p2p_hosting_listed') + function(source, 'p2p_set_hosting_public')
    code = mutated(code, 'return public;', 'return !public;')
    status, output = run(build('p2p_visibility', (('under_test.inc', code),)), '')
    assert status == CHECK_FAILED, output
