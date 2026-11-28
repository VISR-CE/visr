"""How long a test waits for a probe program it started.

A probe waits on its own threads with port/ios/tests/test_wait.h (a wall-clock cap of TEST_WAIT_CAP_SECONDS, 120 s);
the process that runs it gets a minute longer, so on a hang the probe reaches its own cap first, says which wait
failed and cleans up its children, and the wrapper never kills it mid-report (tools/test_probe_waits.py checks the
order). A loaded job host starves a program for
far longer than an idle machine needs, so a short number here is a flake. tools/test_probe_waits.py refuses a literal
short timeout in a wrapper that runs a probe."""

PROBE_RUN_SECONDS = 180
