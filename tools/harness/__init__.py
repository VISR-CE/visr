"""Asset-free regression tests: the engine's real code compiled against a fake world (README.md)."""

import functools
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from probekit import source as probe_source  # noqa: E402  (the one implementation of the extractors)

ROOT = Path(__file__).resolve().parent.parent.parent
HARNESS = Path(__file__).resolve().parent


def read(relative):
    """A source file of the repository, as the compiler reads it (latin-1; tools/probekit/source.py)."""
    return probe_source.read(relative)


def function(text, name):
    """A function's definition (static, inline or not), found by name; LookupError if not found."""
    return probe_source.function_in(text, name)


def inline(source, name):
    """An __inline header function, as a static one."""
    return function(source, name).replace("__inline", "static", 1)


def enum_with(source, member):
    """The whole enum that declares a member."""
    at = source.index(member)
    start = source.rindex("enum", 0, at)
    return source[start:source.index("};", at) + 2]


def structure(text, name):
    """An actual named structure's definition, including nested unions/structures."""
    return probe_source.structure_in(text, name)


def constant(source, name):
    """An enum constant's or #define's integer value, following a name it is set to (as the port's capacities are)."""
    match = re.search(r"\b" + re.escape(name) + r"\s*=\s*(\w+)", source) or \
        re.search(r"#define\s+" + re.escape(name) + r"\s+(\w+)", source)
    if not match:
        raise LookupError(f"constant not found in the sources: {name}")
    value = match.group(1)
    if re.fullmatch(r"0x[0-9A-Fa-f]+|\d+", value):
        return int(value, 0)
    return constant(source + read("port/linux/include/halo_port_capacity.h"), value)


mutated = probe_source.mutated


_directory = tempfile.TemporaryDirectory(prefix="halo-harness-")


CHECK_FAILED = 1  # a case's exit status when a CHECK fails (harness.h)


@functools.lru_cache(maxsize=None)
def build(test, generated):
    """tests/<test>.c compiled with the generated includes ((name, text), ...) for the game's 32-bit ABI."""
    work = Path(_directory.name) / f"{test}-{abs(hash(generated)):x}"
    work.mkdir(parents=True, exist_ok=True)
    for name, text in generated:
        (work / name).write_text(text)
    executable = work / test
    command = [os.environ.get("CC", "clang"), "-m32", "-std=gnu99", "-O2", "-Wall", "-Werror", "-Wno-unused-function",
               "-Wno-unused-variable", "-I", str(HARNESS / "include"), "-I", str(work),
               str(HARNESS / "tests" / f"{test}.c"), "-o", str(executable), "-lm"]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{test}.c does not compile:\n{result.stderr}")
    return executable


def run(executable, case):
    """Run one case: (exit status, output)."""
    result = subprocess.run([str(executable), case], capture_output=True, text=True, timeout=60)
    return result.returncode, (result.stdout + result.stderr).strip()
