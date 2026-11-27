#!/usr/bin/env python3
"""tools/probe_ledger.py's record of a run (HALO_PROBE_LEDGER) and its comparison: the proof that a change to how
tools/ios_test.py builds its probes built and ran the same ones the same way."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probe_ledger import ProbeLedger, compare, link_parts, main


def test_the_record_leaves_out_the_checkout_path(tmp_path, monkeypatch):
    record = tmp_path / 'ledger.json'
    monkeypatch.setenv('HALO_PROBE_LEDGER', str(record))
    probes = ProbeLedger(root=tmp_path)
    probes.note_built(tmp_path / 'build/a-probe', ('xcrun', 'clang', f'-I{tmp_path}/build', 'port/ios/tests/a.c',
                                                    '-o', tmp_path / 'build/a-probe'))
    probes.note_ran(tmp_path / 'build/a-probe', ('true',))
    probes.check()
    assert json.loads(record.read_text()) == [
        ['built', 'build/a-probe', ['xcrun', 'clang', '-Ibuild', 'port/ios/tests/a.c', '-o', 'build/a-probe']],
        ['ran', 'build/a-probe', ['true']]]


def test_inputs_may_move_among_the_options_but_not_change_order():
    before = ['xcrun', 'clang', '-O2', 'port/ios/tests/a.c', '-o', 'out', '-framework', 'Foundation', 'b.c']
    moved = ['xcrun', 'clang', '-O2', '-framework', 'Foundation', 'port/ios/tests/a.c', 'b.c', '--output=out']
    assert link_parts(before) == link_parts(moved)
    assert link_parts(before)['inputs'] == ['port/ios/tests/a.c', 'b.c']
    assert link_parts(before)['options'] == ['-O2', '-framework Foundation']
    swapped = ['xcrun', 'clang', '-O2', '-framework', 'Foundation', 'b.c', 'port/ios/tests/a.c', '-o', 'out']
    assert link_parts(before) != link_parts(swapped)
    reordered = ['xcrun', 'clang', '-framework', 'Foundation', '-O2', 'port/ios/tests/a.c', 'b.c', '-o', 'out']
    assert link_parts(before) != link_parts(reordered)


def test_a_changed_flag_or_a_missing_run_is_a_difference(tmp_path, capsys):
    built = ['built', 'build/a', ['xcrun', 'clang', '-O2', 'port/ios/tests/a.c', '-o', 'build/a']]
    ran = ['ran', 'build/a', []]
    assert compare([built, ran], [built, ran]) == []
    assert compare([built, ran], [built])
    changed = ['built', 'build/a', ['xcrun', 'clang', '-O0', 'port/ios/tests/a.c', '-o', 'build/a']]
    assert compare([built, ran], [changed, ran])
    first, second = tmp_path / 'first.json', tmp_path / 'second.json'
    first.write_text(json.dumps([built, ran]))
    second.write_text(json.dumps([changed, ran]))
    assert main(['--compare', str(first), str(first)]) == 0
    assert main(['--compare', str(first), str(second)]) == 1
    assert 'probe ledgers differ' in capsys.readouterr().out
