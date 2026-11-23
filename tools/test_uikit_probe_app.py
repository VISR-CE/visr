#!/usr/bin/env python3
"""tools/uikit_probe_app.py: only a known scene-start failure is a skip; any other end before the probe starts fails."""
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import uikit_probe_app

NAME = 'benchmark menu probe'
# what GitHub's hosted runner and a Mac without a usable GUI session printed (Nov 22)
SCENE_ABORT = ("*** Terminating app due to uncaught exception 'NSInternalInconsistencyException', "
               "reason: '_mainSceneIdentifier should have been set by now!'\n")


def result(stdout, stderr='', returncode=0):
    return subprocess.CompletedProcess(['probe'], returncode, stdout, stderr)


def test_a_known_scene_start_failure_is_a_skip_with_uikits_reason(capsys):
    with pytest.raises(SystemExit) as ended:
        uikit_probe_app.require_start(result('', SCENE_ABORT, -6), NAME)
    assert ended.value.code == uikit_probe_app.SKIPPED
    out = capsys.readouterr().out
    assert uikit_probe_app.is_skip(ended.value.code, out)
    assert '_mainSceneIdentifier should have been set by now!' in uikit_probe_app.skip_reason(out)


@pytest.mark.parametrize('stderr', [
    '',                                                     # a bare abort
    'dyld[123]: Library not loaded: @rpath/Missing.framework\n',
    "*** Terminating app due to uncaught exception 'NSInvalidArgumentException', reason: 'crashed in didFinishLaunching'\n",
    'Killed: 9\n',                                           # a signing kill
])
def test_any_other_end_before_the_probe_starts_fails(stderr, capsys):
    with pytest.raises(SystemExit) as ended:
        uikit_probe_app.require_start(result('', stderr, -6), NAME)
    assert ended.value.code != uikit_probe_app.SKIPPED and isinstance(ended.value.code, str)
    assert 'not a known scene-start failure' in ended.value.code
    assert uikit_probe_app.skip_reason(capsys.readouterr().out) is None


def test_a_probe_that_started_passes_the_start_check_even_when_it_fails_later():
    uikit_probe_app.require_start(result(f'{NAME}: started\n', 'FAIL: something\n' + SCENE_ABORT, 1), NAME)
    assert uikit_probe_app.scene_start_failure(result(f'{NAME}: started\n', SCENE_ABORT, 1), NAME) is None


def test_a_skip_needs_both_the_status_and_the_line():
    assert uikit_probe_app.is_skip(uikit_probe_app.SKIPPED, 'SKIP: why\n')
    assert not uikit_probe_app.is_skip(uikit_probe_app.SKIPPED, 'no line\n')
    assert not uikit_probe_app.is_skip(1, 'SKIP: why\n')
    assert not uikit_probe_app.is_skip(uikit_probe_app.SKIPPED, None)
