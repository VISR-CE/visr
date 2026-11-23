"""Shared by the runners that launch a probe as a small Mac Catalyst app with a UIKit window scene
(tools/test_frame_workgroup_uikit.py, tools/test_benchmark_menu.py).

Some hosts cannot make the scene: GitHub's macOS runners raise while connecting it, and a Mac without a usable
GUI session aborts with "_mainSceneIdentifier should have been set by now!". UIKit then ends the app before the
probe's scene delegate runs, so the probe never prints its start line. A runner tells that apart from a probe
failure by that line, and exits with SKIPPED and a "SKIP: " line: tools/ios_test.py records it in its probe ledger
and reports it at the end, so a skip is never read as a pass.
"""
import sys

# the exit status of a runner whose probe could not be tested on this host
SKIPPED = 77
SKIP_PREFIX = 'SKIP: '


def started_line(name):
    """The line a probe prints first thing in its scene delegate's `scene:willConnectToSession:options:`."""
    return f'{name}: started'


def start_failure(result, name):
    """None when the probe's scene connected (its start line is in stdout); otherwise UIKit's reason, from the
    uncaught exception's `reason:` line, or the exit status."""
    if started_line(name) in result.stdout:
        return None
    return next((line.strip() for line in result.stderr.splitlines() if 'reason:' in line), f'exit {result.returncode}')


def skip(message):
    """Ends the runner as skipped, with the reason on stdout."""
    print(SKIP_PREFIX + message, flush=True)
    sys.exit(SKIPPED)


def skip_reason(stdout):
    """The reason a skipped runner printed."""
    return next((line[len(SKIP_PREFIX):] for line in stdout.splitlines() if line.startswith(SKIP_PREFIX)),
                'no reason given')
