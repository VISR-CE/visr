#!/usr/bin/env python3
"""tools/uikit_probe_app.py: a probe whose UIKit scene never connected is a recorded skip, never a pass or a crash."""
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import uikit_probe_app

# what a Mac without a usable GUI session printed (ios_test job on Nov 22)
ABORT = ("*** Terminating app due to uncaught exception 'NSInternalInconsistencyException', "
         "reason: '_mainSceneIdentifier should have been set by now!'\n")


def result(stdout, stderr='', returncode=0):
    return subprocess.CompletedProcess(['probe'], returncode, stdout, stderr)


def test_a_scene_that_never_connected_is_a_start_failure_with_uikits_reason():
    failure = uikit_probe_app.start_failure(result('', ABORT, -6), 'benchmark menu probe')
    assert "_mainSceneIdentifier should have been set by now!" in failure


def test_a_start_failure_without_a_reason_names_the_exit_status():
    assert uikit_probe_app.start_failure(result('', '', -6), 'benchmark menu probe') == 'exit -6'


def test_a_probe_that_started_is_not_a_start_failure_even_when_it_fails_later():
    started = result('benchmark menu probe: started\n', 'FAIL: something\n', 1)
    assert uikit_probe_app.start_failure(started, 'benchmark menu probe') is None


def test_skip_exits_with_the_skip_status_and_a_reason_ios_test_reads(capsys):
    with pytest.raises(SystemExit) as ended:
        uikit_probe_app.skip('benchmark menu probe: UIKit could not start the probe here (exit -6)')
    assert ended.value.code == uikit_probe_app.SKIPPED
    assert uikit_probe_app.skip_reason(capsys.readouterr().out) == \
        'benchmark menu probe: UIKit could not start the probe here (exit -6)'
