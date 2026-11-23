"""Builds and runs a standalone C probe for a unit test: one place for the compiler choice, the warning flags and the
include paths the host probes share."""
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
HOST_INCLUDES = ("port/ios/host", "port/linux/src")
# the Linux-strict compiler wrapper `tools/ci_test.py` puts first on PATH on macOS: a probe run directly fails where the
# Linux Tooling job would, as it does under ci_test
STRICT = ROOT / "tools/probekit/strict"
FLAGS = ("-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-pthread")


def sanitizer_launcher():
    """What a ThreadSanitizer probe runs behind: on Linux, address randomization off, since the kernel's wider mmap
    randomization puts mappings where TSan's shadow memory goes."""
    if sys.platform == "linux" and shutil.which("setarch"):
        return ("setarch", platform.machine(), "--addr-no-randomize")
    return ()


def sanitizer_links(sanitizer: str) -> bool:
    """Whether the compiler run_probe uses can build and run a program with `-fsanitize=<sanitizer>` (a compiler
    without that runtime, or a kernel it cannot map its shadow memory on, skips the probe instead of failing it)."""
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        return False
    with tempfile.TemporaryDirectory(prefix="sanitizer-") as temp:
        source = pathlib.Path(temp) / "empty.c"
        executable = pathlib.Path(temp) / "empty"
        source.write_text("int main(void) { return 0; }\n")
        built = subprocess.run([compiler, f"-fsanitize={sanitizer}", str(source), "-o", str(executable)],
                               capture_output=True)
        if built.returncode:
            return False
        return not subprocess.run([*sanitizer_launcher(), str(executable)], capture_output=True).returncode


def run_probe(test: unittest.TestCase, sources, includes=HOST_INCLUDES, extra=(), darwin_only=False, launcher=()):
    """Compiles `sources` (paths relative to the repository root) with `includes` and runs the result, behind
    `launcher` when given; skips the test when no compiler is found, or off macOS when `darwin_only` is set (probes of
    code that reads the Mach clock)."""
    if darwin_only and sys.platform != "darwin":
        test.skipTest("macOS is required")
    compiler = shutil.which("clang") or (None if darwin_only else shutil.which("cc"))
    if not compiler:
        test.skipTest("A C compiler is required")
    environment = dict(os.environ)
    if darwin_only:
        # (Apple-only code never builds on Linux, so the Linux-strict checks do not apply)
        environment["HALO_LINUX_STRICT"] = "0"
    elif sys.platform == "darwin" and (STRICT / "clang").exists():
        compiler = str(STRICT / "clang")
    name = pathlib.Path(sources[0]).stem
    with tempfile.TemporaryDirectory(prefix=f"{name}-") as temp:
        executable = pathlib.Path(temp) / "probe"
        command = [compiler, *FLAGS]
        for include in includes:
            command += ["-I", str(ROOT / include)]
        command += [str(ROOT / source) for source in sources]
        command += [*extra, "-o", str(executable)]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([*launcher, str(executable)], check=True)
