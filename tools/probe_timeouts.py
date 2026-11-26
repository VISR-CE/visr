"""How long a test waits for a probe program it started.

A probe waits on its own threads with port/ios/tests/test_wait.h (a wall-clock cap of 120 s); the process that runs it
gets this much, so the wrapper never gives up before the probe's own caps do. A loaded job host starves a program for
far longer than an idle machine needs, so a short number here is a flake. tools/test_probe_waits.py refuses a literal
short timeout in a wrapper that runs a probe."""

PROBE_RUN_SECONDS = 120
