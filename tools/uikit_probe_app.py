"""Shared by the runners that launch a probe as a small Mac Catalyst app with a UIKit window scene
(every tools/test_*.py that builds for -macabi).

Some hosts cannot make the scene: UIKit aborts before the probe's scene delegate runs, so the probe never prints
the start line it prints first thing in `scene:willConnectToSession:options:`. A run without that line is a skip
only when stderr carries one of the known scene-start failures below; anything else (a dyld failure, a signing kill,
a crash in `didFinishLaunching`) fails the runner. A skip exits SKIPPED with a "SKIP: " line, which tools/ios_test.py
records in its probe ledger and reports at the end, so it is never read as a pass.
"""
import sys

# the exit status of a runner whose probe could not be tested on this host
SKIPPED = 77
SKIP_PREFIX = 'SKIP: '
# UIKit's reasons for not making the scene. GitHub's hosted macOS runners and a Mac without a usable GUI session
# both raise this one (from -[UINSApplicationDelegate didRequestSceneWithOptions:sceneIdentifier:orError:]).
SCENE_START_FAILURES = (
    '_mainSceneIdentifier should have been set by now!',
)


def started_line(name):
    """The line a probe prints first thing in its scene delegate's `scene:willConnectToSession:options:`."""
    return f'{name}: started'


def scene_start_failure(result, name):
    """The known scene-start failure that kept the probe from starting, or None: the probe started, or it ended
    before starting for some other reason (which the caller must treat as a failure)."""
    if started_line(name) in result.stdout:
        return None
    return next((reason for reason in SCENE_START_FAILURES if reason in result.stderr), None)


def require_start(result, name):
    """Returns when the probe started. Exits as skipped when UIKit could not make its scene here, and fails the
    runner when it ended before starting for any other reason."""
    if started_line(name) in result.stdout:
        return
    reason = scene_start_failure(result, name)
    if reason:
        skip(f'{name}: UIKit could not make the window scene here ({reason})')
    sys.stderr.write(result.stderr[-4000:])
    sys.exit(f'{name}: ended before its scene connected (exit {result.returncode}), not a known scene-start failure')


def skip(message):
    """Ends the runner as skipped, with the reason on stdout."""
    print(SKIP_PREFIX + message, flush=True)
    sys.exit(SKIPPED)


def skip_reason(text):
    """The reason in a "SKIP: " line, or None when there is none."""
    return next((line[len(SKIP_PREFIX):] for line in text.splitlines() if line.startswith(SKIP_PREFIX)), None)


def is_skip(returncode, stdout):
    """A runner that skipped: exit SKIPPED and a "SKIP: " line, both."""
    return returncode == SKIPPED and skip_reason(stdout or '') is not None
