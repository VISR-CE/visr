"""A native probe never waits a short fixed time for another thread.

A loaded job host starves a worker thread for longer than any number picked on an idle machine: save_writer_probe
failed "save_writer_wait(10)" under a long build line, and the same short caps sat in a dozen other probes. Probes
wait with port/ios/tests/test_wait.h (a wall-clock wait on a monotonic clock, one generous cap, a semaphore timeout
constant and a watchdog constant). This test refuses, in every probe, a counted sleep-poll loop, a dispatch wait with a
short literal timeout, a whole-program alarm with a literal count, and a probe that waits on the clock without the
header. A line that is a deliberate promptness or absence check carries TEST_WAIT_SHORT_ON_PURPOSE and says why.
"""

import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
PROBES = sorted([*(ROOT / "port/ios/tests").glob("*.[cm]"), *(ROOT / "port/ios/tests").glob("*.cpp")])
MARK = "TEST_WAIT_SHORT_ON_PURPOSE"
SLEEP = r"(?:usleep|sleepForTimeInterval|nanosleep)"

# (name, pattern) for one source line
LINE_RULES = (
    ("a counted poll loop that sleeps (use TEST_WAIT_UNTIL)",
     re.compile(r"\bfor\s*\([^;]*;[^;]*<\s*\d+[^;]*;[^)]*\)[^\n]*" + SLEEP)),
    ("a counted poll loop whose body sleeps on the next line (use TEST_WAIT_UNTIL)", None),
    ("a dispatch wait with a short literal timeout (use TEST_WAIT_SEMAPHORE_TIMEOUT)",
     re.compile(r"dispatch_(?:semaphore|group)_wait\([^\n]*dispatch_time\(\s*DISPATCH_TIME_NOW\s*,\s*(?:(\d+)\s*\*\s*)?NSEC_PER_SEC\s*\)")),
    ("a whole-program alarm with a literal count (use TEST_WAIT_WATCHDOG_SECONDS)",
     re.compile(r"(?<![\w.])alarm\(\s*[1-9]\d*\s*\)")),
    ("a try counter that gives up after hundreds of polls (use TEST_WAIT_UNTIL)",
     re.compile(r"\b(?:tries|attempt)\s*(?:\+\+)?\s*<\s*\d{3,}")),
)
LOOP_HEAD = re.compile(r"\b(?:for|while)\s*\([^\n]*<\s*\d{3,}[^\n]*\)\s*\{?\s*$")


def violations(text):
    """(line number, rule) for every wait in `text` that caps itself with a count or a short literal time."""
    lines = text.split("\n")
    found = []
    for number, line in enumerate(lines, 1):
        if MARK in line:
            continue
        for name, pattern in LINE_RULES:
            if pattern is None:
                continue
            match = pattern.search(line)
            if not match:
                continue
            if name.startswith("a dispatch wait") and match.group(1) and int(match.group(1)) >= 100:
                continue   # a long network cap (feeds_host_probe), not a short thread wait
            found.append((number, name))
        if LOOP_HEAD.search(line) and re.search(SLEEP, "\n".join(lines[number:number + 6]).split("}")[0]):
            if MARK not in "\n".join(lines[number - 1:number + 6]):
                found.append((number, LINE_RULES[1][0]))
    return found


@pytest.mark.parametrize("path", PROBES, ids=lambda path: str(path.relative_to(ROOT)))
def test_probe_waits_on_the_clock_not_on_a_count(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    found = violations(text)
    assert not found, f"{path.name}: " + "; ".join(f"line {n}: {rule}" for n, rule in found) + " (port/ios/tests/test_wait.h)"


def test_negative_control_the_detector_sees_the_short_forms():
    assert violations("for (int i = 0; i < 3000 && !ready; i++) usleep(10000);")
    assert violations("for(int i=0; i<30000 && !ended; i++) { ended=take(); if(!ended) usleep(1000); }")
    assert violations("for (int attempt = 0; attempt < 200; attempt++) {\n    check();\n    [NSThread sleepForTimeInterval:0.01];\n}")
    assert violations("assert(dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC)) == 0);")
    assert violations("assert(dispatch_group_wait(g, dispatch_time(DISPATCH_TIME_NOW, 5*NSEC_PER_SEC)) == 0);")
    assert violations("    alarm(10);")
    assert violations("while (!gate && tries++ < 5000)\n\tusleep(1000);")


def test_negative_control_the_detector_passes_the_helper_and_the_marked_checks():
    assert not violations("TEST_WAIT_UNTIL(requests >= 2);")
    assert not violations("assert(!dispatch_semaphore_wait(done, TEST_WAIT_SEMAPHORE_TIMEOUT));")
    assert not violations("alarm(TEST_WAIT_WATCHDOG_SECONDS);")
    assert not violations("alarm(0);")
    assert not violations("if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_SEC))) return nil;")
    assert not violations("for (int a = 0; a < 200 && child.isRunning; a++) sleep_();   /* TEST_WAIT_SHORT_ON_PURPOSE: prompt */")
    assert not violations("usleep(60000);")


def test_a_probe_that_uses_the_helper_includes_it():
    for path in PROBES:
        text = path.read_text(encoding="utf-8", errors="replace")
        if re.search(r"TEST_WAIT_(?:UNTIL|SEMAPHORE_TIMEOUT|WATCHDOG_SECONDS|CAP_SECONDS)", text) and path.name != "test_wait.h":
            assert '"test_wait.h"' in text, f"{path.name} uses test_wait.h macros without including it"
