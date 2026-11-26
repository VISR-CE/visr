"""A native probe never waits a short fixed time for another thread.

A loaded job host starves a worker thread for longer than any number picked on an idle machine: save_writer_probe
failed "save_writer_wait(10)" under a long build line, and the same short caps sat in a dozen other probes. Probes
wait with port/ios/tests/test_wait.h (a wall-clock wait on a monotonic clock, one generous cap, a semaphore timeout
constant and a watchdog constant). This test refuses, in every probe, a counted sleep-poll loop (whatever its body
holds), a dispatch wait with a short literal timeout in seconds or milliseconds (cast or not), a short
CFAbsoluteTimeGetCurrent() deadline, a TEST_WAIT_UNTIL_FOR with a short literal, and a whole-program alarm with a
literal count. A line that is a promptness check, an absence check or an event nothing reports carries
TEST_WAIT_SHORT_ON_PURPOSE and says why. A probe that uses the helper's macros must include the header.
A timeout that sits in a variable, and a loop bounded below ten, are not seen.
"""

import re
from pathlib import Path

import pytest
from probe_timeouts import PROBE_RUN_SECONDS

ROOT = Path(__file__).resolve().parents[1]
PROBES = sorted([*(ROOT / "port/ios/tests").glob("*.[cm]"), *(ROOT / "port/ios/tests").glob("*.cpp")])
MARK = "TEST_WAIT_SHORT_ON_PURPOSE"
SLEEP = re.compile(r"\b(?:usleep|sleepForTimeInterval|nanosleep)\b")
LONG_CAP_SECONDS = 100   # a network cap such as feeds_host_probe's 300 s is not a wait for another thread

NUMBER = r"(\d+(?:\.\d+)?)"
DISPATCH_WAIT = re.compile(r"dispatch_(?:semaphore|group)_wait\(")
DISPATCH_TIME = re.compile(r"dispatch_time\(\s*DISPATCH_TIME_NOW\s*,")
DEADLINE = re.compile(r"CFAbsoluteTimeGetCurrent\(\)\s*\+\s*" + NUMBER)
UNTIL_FOR = re.compile(r"TEST_WAIT_UNTIL_FOR\([^\n]*,\s*" + NUMBER + r"\s*\)")
ALARM = re.compile(r"(?<![\w.])alarm\(\s*[1-9]\d*\s*\)")
LOOP_HEAD = re.compile(r"\b(?:for|while)\s*\(")
BOUND = re.compile(r"<\s*(\d+)\b")


def matching(text, start, opener, closer):
    """Index just past the bracket that closes the one at text[start]."""
    depth = 0
    for index in range(start, len(text)):
        depth += (text[index] == opener) - (text[index] == closer)
        if depth == 0:
            return index + 1
    return len(text)


def dispatch_seconds(argument):
    """Seconds a dispatch_time offset spells with a literal, or None when it is not a literal."""
    unit = 1.0 if "NSEC_PER_SEC" in argument else 1e-3 if "NSEC_PER_MSEC" in argument else None
    if unit is None:
        return None
    factor = re.search(NUMBER, argument)
    return float(factor.group(1)) * unit if factor else unit


def loop_body(text, head_end):
    """The text a for or while loop repeats: its block, or the one statement after it."""
    rest = text[head_end:]
    stripped = rest.lstrip()
    skipped = len(rest) - len(stripped)
    if stripped.startswith("{"):
        return rest[skipped:matching(rest, skipped, "{", "}")]
    end = stripped.find(";")
    return stripped[:end + 1] if end >= 0 else stripped[:200]


def violations(text):
    """(line number, rule) for every wait in `text` that caps itself with a count or a short literal time."""
    found = []
    lines = text.split("\n")
    marked = {number for number, line in enumerate(lines, 1) if MARK in line}

    def line_of(offset):
        return text.count("\n", 0, offset) + 1

    def report(offset, rule):
        number = line_of(offset)
        if number not in marked:
            found.append((number, rule))

    for match in LOOP_HEAD.finditer(text):
        head_end = matching(text, match.end() - 1, "(", ")")
        head = text[match.end():head_end]
        bound = BOUND.search(head)
        if bound and int(bound.group(1)) >= 10 and SLEEP.search(loop_body(text, head_end)):
            report(match.start(), "a counted poll loop that sleeps (use TEST_WAIT_UNTIL)")
        if re.search(r"\b(?:tries|attempt)\s*(?:\+\+)?\s*<\s*\d{3,}", head):
            report(match.start(), "a try counter that gives up after hundreds of polls (use TEST_WAIT_UNTIL)")
    for match in re.finditer(r"\b(?:tries|attempt)\s*(?:\+\+)?\s*<\s*\d{3,}", text):
        report(match.start(), "a try counter that gives up after hundreds of polls (use TEST_WAIT_UNTIL)")
    for wait in DISPATCH_WAIT.finditer(text):
        call_end = matching(text, wait.end() - 1, "(", ")")
        timed = DISPATCH_TIME.search(text, wait.end(), call_end)
        if timed:
            argument = text[timed.end():matching(text, timed.start() + len("dispatch_time"), "(", ")") - 1]
            seconds = dispatch_seconds(argument)
            if seconds is not None and seconds < LONG_CAP_SECONDS:
                report(wait.start(), "a dispatch wait with a short literal timeout (use TEST_WAIT_SEMAPHORE_TIMEOUT)")
    for match in DEADLINE.finditer(text):
        if float(match.group(1)) < LONG_CAP_SECONDS:
            report(match.start(), "a short CFAbsoluteTimeGetCurrent deadline (use TEST_WAIT_UNTIL or TEST_WAIT_PUMPING_UNTIL)")
    for match in UNTIL_FOR.finditer(text):
        if float(match.group(1)) < LONG_CAP_SECONDS:
            report(match.start(), "a TEST_WAIT_UNTIL_FOR with a short literal cap (use TEST_WAIT_UNTIL)")
    for match in ALARM.finditer(text):
        report(match.start(), "a whole-program alarm with a literal count (use TEST_WAIT_WATCHDOG_SECONDS)")
    return sorted(set(found))


@pytest.mark.parametrize("path", PROBES, ids=lambda path: str(path.relative_to(ROOT)))
def test_probe_waits_on_the_clock_not_on_a_count(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    found = violations(text)
    assert not found, f"{path.name}: " + "; ".join(f"line {n}: {rule}" for n, rule in found) + " (port/ios/tests/test_wait.h)"


# the shapes this class took before the helper: each is a line or block that was in a probe
OLD_SHAPES = {
    "a flat counted loop": "for (int i = 0; i < 3000 && !ready; i++) usleep(10000);",
    "a counted loop on one line": "for(int i=0; i<30000 && !ended; i++) { ended=take(); if(!ended) usleep(1000); }",
    "a sleep after a statement": "for (int attempt = 0; attempt < 200; attempt++) {\n    check();\n    [NSThread sleepForTimeInterval:0.01];\n}",
    "a block before the sleep (data_updates_probe waitForMenuChecks)": (
        "    for (int i = 0; i < 3000; i++) {\n        __block BOOL busy;\n"
        "        dispatch_sync(updatesQueue(), ^{ busy = profileChecking || recordingChecking; });\n"
        "        if (!busy) return;\n        usleep(10000);\n    }"),
    "a block before the sleep (profile_feed_probe)": (
        "        for (int i = 0; i < 3000; i++) {\n            __block BOOL busy; dispatch_sync(updatesQueue(), ^{ busy = profileChecking; });\n"
        "            if (!busy) break;\n            usleep(10000);\n        }"),
    "a while loop on a try counter": "while (!gate && tries++ < 5000)\n\tusleep(1000);",
    "a semaphore timeout in seconds": "assert(dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC)) == 0);",
    "a group timeout": "assert(dispatch_group_wait(g, dispatch_time(DISPATCH_TIME_NOW, 5*NSEC_PER_SEC)) == 0);",
    "a cast timeout": "assert(!dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC))));",
    "a bare second": "assert(dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC)) == 0);",
    "a millisecond timeout": "dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC));",
    "a run-loop deadline (main_thread_probe)": (
        "    CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + 5;\n"
        "    while (!atomic_load(&done) && CFAbsoluteTimeGetCurrent() < deadline)\n        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);"),
    "a deadline passed to a wait (benchmark_summary_probe)": "assert(wait_for(done, CFAbsoluteTimeGetCurrent() + 1));",
    "a short cap on the helper": "TEST_WAIT_UNTIL_FOR(ready, 2);",
    "a whole-program alarm": "    alarm(10);",
}


@pytest.mark.parametrize("name", sorted(OLD_SHAPES))
def test_negative_control_the_detector_sees_the_old_shape(name):
    assert violations(OLD_SHAPES[name]), name


NEW_SHAPES = {
    "the helper": "TEST_WAIT_UNTIL(requests >= 2);",
    "the pumping helper": "assert(TEST_WAIT_PUMPING_UNTIL(atomic_load(&done)));",
    "the semaphore constant": "assert(!dispatch_semaphore_wait(done, TEST_WAIT_SEMAPHORE_TIMEOUT));",
    "the watchdog": "alarm(TEST_WAIT_WATCHDOG_SECONDS);",
    "alarm off": "alarm(0);",
    "a long network cap": "if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_SEC))) return nil;",
    "a delayed signal, not a wait": "dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), queue, ^{ signal(); });",
    "the cap as a deadline": "assert(wait_for(done, CFAbsoluteTimeGetCurrent() + TEST_WAIT_CAP_SECONDS));",
    "a marked loop": "for (int a = 0; a < 200 && child.isRunning; a++) sleep_();   /* TEST_WAIT_SHORT_ON_PURPOSE: prompt */",
    "a marked deadline": "CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + 2.1;   /* TEST_WAIT_SHORT_ON_PURPOSE: the product's timeout */",
    "a one-off pause": "usleep(60000);",
    "a short loop that is not a poll": "for (frame = 0; frame < 5; frame++) {\n    work();\n    usleep(2000);\n}",
}


@pytest.mark.parametrize("name", sorted(NEW_SHAPES))
def test_negative_control_the_detector_passes_the_helper_and_the_marked_checks(name):
    assert not violations(NEW_SHAPES[name]), name


def test_a_probe_that_uses_the_helper_includes_it():
    for path in PROBES:
        text = path.read_text(encoding="utf-8", errors="replace")
        if re.search(r"TEST_WAIT_(?:UNTIL|UNTIL_FOR|PUMPING_UNTIL|SEMAPHORE_TIMEOUT|WATCHDOG_SECONDS|CAP_SECONDS)", text):
            assert '"test_wait.h"' in text, f"{path.name} uses test_wait.h macros without including it"


def test_a_probe_that_uses_the_helper_includes_it_first():
    """_POSIX_C_SOURCE (what hides nanosleep from glibc under -std=c11) only counts before the first system header."""
    for path in PROBES:
        text = path.read_text(encoding="utf-8", errors="replace")
        if '"test_wait.h"' in text:
            first = re.search(r"^#(?:include|import) .*$", text, re.M).group(0)
            assert first == '#include "test_wait.h"', f"{path.name}: test_wait.h must be the first include, not {first}"


FEATURE_MACRO = re.compile(r"^\s*#\s*define\s+(?:_POSIX_C_SOURCE|_XOPEN_SOURCE|_GNU_SOURCE|_DEFAULT_SOURCE|_BSD_SOURCE|_ISOC\d+_SOURCE)\b")


def unguarded_feature_macros(text):
    """Lines that define a feature-test macro outside an #if that names __linux__ (or __GLIBC__)."""
    found, stack = [], []
    for number, line in enumerate(text.split("\n"), 1):
        directive = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", line)
        if directive:
            word, rest = directive.groups()
            if word in ("if", "ifdef", "ifndef"):
                stack.append(("__linux__" in rest or "__GLIBC__" in rest) and word != "ifndef" or ("__linux__" in rest and "!defined(__linux__)" not in rest))
            elif word == "elif":
                stack[-1:] = [("__linux__" in rest or "__GLIBC__" in rest)] if stack else []
            elif word == "else":
                stack[-1:] = [False] if stack else []
            elif stack:
                stack.pop()
        elif FEATURE_MACRO.match(line) and not any(stack):
            found.append(number)
    return found


@pytest.mark.parametrize("path", sorted((ROOT / "port/ios/tests").glob("*.h")), ids=lambda path: path.name)
def test_a_shared_test_header_guards_its_feature_test_macro_per_platform(path):
    """A feature-test macro changes what a libc declares: _POSIX_C_SOURCE fixes a strict -std=c11 build on Linux and hides
    mkdtemp, pthread_setname_np and pthread_set_qos_class_self_np on Apple (the iOS workflow failed on it)."""
    found = unguarded_feature_macros(path.read_text(encoding="utf-8", errors="replace"))
    assert not found, f"{path.name}: line {found[0]} defines a feature-test macro for every platform (guard it with #if defined(__linux__))"


def test_negative_control_the_feature_macro_detector():
    assert unguarded_feature_macros("#define _POSIX_C_SOURCE 200809L")
    assert unguarded_feature_macros("#if !defined(_POSIX_C_SOURCE)\n#define _POSIX_C_SOURCE 200809L\n#endif")
    assert unguarded_feature_macros("#ifdef __APPLE__\n#define _GNU_SOURCE\n#endif")
    assert not unguarded_feature_macros("#if defined(__linux__) && !defined(_POSIX_C_SOURCE)\n#define _POSIX_C_SOURCE 200809L\n#endif")
    assert not unguarded_feature_macros("#ifdef __linux__\n#define _GNU_SOURCE\n#endif")
    assert not unguarded_feature_macros("#include <stdio.h>")


def test_a_probe_that_includes_the_helper_uses_it():
    for path in PROBES:
        text = path.read_text(encoding="utf-8", errors="replace")
        if '"test_wait.h"' in text:
            assert re.search(r"TEST_WAIT_[A-Z_]+", text.replace('"test_wait.h"', "")), f"{path.name} includes test_wait.h and uses nothing from it"


def test_the_python_wrappers_share_one_probe_timeout():
    for path in sorted((ROOT / "tools").glob("test_*.py")):
        if path.name == Path(__file__).name:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in re.finditer(r"subprocess\.run\(\[str\((?:program|executable|binary|transport|worker)\)[^\n]*timeout=(\d+)", text):
            assert int(match.group(1)) >= PROBE_RUN_SECONDS, (
                f"{path.name}: a probe runs with timeout={match.group(1)}, under probe_timeouts.PROBE_RUN_SECONDS")
