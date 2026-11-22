"""Builds and runs a standalone C probe for a unit test: one place for the compiler choice, the warning flags and the
include paths the host probes share."""
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
HOST_INCLUDES = ("port/ios/host", "port/linux/src")
FLAGS = ("-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-pthread")


def run_probe(test: unittest.TestCase, sources, includes=HOST_INCLUDES, extra=(), darwin_only=False):
    """Compiles `sources` (paths relative to the repository root) with `includes` and runs the result; skips the test
    when no compiler is found, or off macOS when `darwin_only` is set (probes of code that reads the Mach clock)."""
    if darwin_only and sys.platform != "darwin":
        test.skipTest("macOS is required")
    compiler = shutil.which("clang") or (None if darwin_only else shutil.which("cc"))
    if not compiler:
        test.skipTest("A C compiler is required")
    name = pathlib.Path(sources[0]).stem
    with tempfile.TemporaryDirectory(prefix=f"{name}-") as temp:
        executable = pathlib.Path(temp) / "probe"
        command = [compiler, *FLAGS]
        for include in includes:
            command += ["-I", str(ROOT / include)]
        command += [str(ROOT / source) for source in sources]
        command += [*extra, "-o", str(executable)]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)
