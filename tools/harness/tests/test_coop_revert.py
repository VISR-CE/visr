"""Checkpoint restoration starts fresh unit lifetimes, even under unchanged IDs."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read


def generated(fault=None):
    source = read('port/linux/game/network_objects.c')
    update = function(source, 'distributed_host_update_objects')
    try:
        hook = function(source, 'network_objects_coop_reverted')
    except LookupError:
        hook = 'void network_objects_coop_reverted(void) {}'
    if fault == 'delete':
        hook = mutated(hook, 'message.changes[count].change = _object_change_delete;',
                       'message.changes[count].change = _object_change_create;')
    if fault == 'create':
        hook = mutated(hook, 'distributed_host_update_objects();', '')
    if fault == 'inventory':
        hook = mutated(hook, 'objects_host_inventories[absolute_index].time = now - INVENTORY_REFRESH_TICKS;',
                       'objects_host_inventories[absolute_index].time = now;')
    return (('under_test.inc', update + '\n' + hook),)


@pytest.mark.parametrize('case', ['same', 'unknown', 'changed', 'repeat', 'filter', 'empty', 'batch'])
def test_case(case):
    status, output = run_case(case)
    assert status == 0, output


def run_case(case, fault=None):
    from harness import run
    return run(build('coop_revert', generated(fault)), case)


@pytest.mark.parametrize('fault,case', [('delete', 'same'), ('create', 'same'), ('inventory', 'empty')])
def test_negative_control(fault, case):
    status, output = run_case(case, fault)
    assert status == CHECK_FAILED, output


def test_host_dispatch_after_clock_repair():
    from harness import run
    source = read('port/linux/game/network_coop.c')
    code = function(source, 'network_coop_reverted')
    status, output = run(build('coop_revert_dispatch', (('under_test.inc', code),)), 'dispatch')
    assert status == 0, output
