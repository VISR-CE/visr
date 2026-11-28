"""build(): the one place a test compiles and links a probe.

Every probe a test builds goes through `build`, so the compiler, the warnings, `-lm` and Phase 1's Linux checks are
the same everywhere (tools/test_probe_conventions.py keeps new tests on it). The defaults:

- the compiler is `clang` (`$CC` overrides it), or `xcrun clang` with `apple=True`;
- `-std=gnu11 -Wall -Werror`, with `-Wextra` only on request: the Linux runner's clang is older than the Macs', so a
  larger default warning set would fail on one and pass on the other;
- `-lm` after the sources, always;
- on a Mac, a build that is not an Apple build runs through `tools/probekit/strict/clang`, which refuses what
  links here and not on Linux (glibc's strict ISO macros, headers Ubuntu lacks, libm without `-lm`).

`flags` replaces every default with the caller's own options, in the caller's order: the command is the compiler,
`flags`, the sources and `-o OUTPUT`. tools/ios_test.py's probes use it, so moving them onto `build` changed no
option; they can move to the structured parameters one at a time.

`host_stubs=True` links the shared weak host stubs (tools/host_stubs.py) built for the probe's `-D` and `-std=`
options, so a probe never keeps its own copy of a host function only because production code started calling it.
`port_stubs=True` links the port's seams (`port/ios/tests/port_stubs.c`: `port_config.h`'s functions and
`platform_log`), compiled as plain C with the probe's own compiler, so it works on Linux too; `host_stubs=True`
links them as well. A probe's own definition of a seam wins; one that returns state the probe sets up carries
`/* fake: <why> */` (tools/test_probe_conventions.py, rule 2).
"""
from __future__ import annotations

import os
import platform
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STRICT_CLANG = ROOT / "tools/probekit/strict/clang"
OUTPUT_LINES = 40


class BuildError(RuntimeError):
    """A probe that did not build: the command, the first lines of the compiler's output and the probe's path."""


def stub_flags(options) -> list[str]:
    """The options the shared host stub object has to match: the probe's -D and -std= options, sorted."""
    return sorted(option for option in map(str, options) if option.startswith("-D") or option.startswith("-std="))


def compiler(apple: bool) -> list[str]:
    if apple:
        return ["xcrun", "clang"]
    if os.environ.get("CC"):
        return [os.environ["CC"]]
    if platform.system() == "Darwin" and os.environ.get("HALO_LINUX_STRICT") != "0":
        return [str(STRICT_CLANG)]
    return ["clang"]


def command(sources, *, output, std="gnu11", includes=(), defines=(), libs=(), language=None, extra_warnings=False,
            relaxed=(), sanitize=False, bits=64, apple=False, flags=None) -> list[str]:
    """The compiler command `build` runs, without the host stub object."""
    sources = [str(source) for source in sources]
    if not sources:
        raise ValueError("build() needs at least one source")
    if flags is not None:
        structured = (std, tuple(includes), tuple(defines), tuple(libs), language, extra_warnings, tuple(relaxed),
                      sanitize, bits)
        if structured != ("gnu11", (), (), (), None, False, (), False, 64):
            raise ValueError("build(flags=...) replaces every default; pass the other options inside flags")
        return [*compiler(apple), *map(str, flags), *sources, "-o", str(output)]
    options = [f"-std={std}", "-Wall", "-Werror"]
    if extra_warnings:
        options.append("-Wextra")
    options += [f"-Wno-{warning}" for warning in relaxed]
    if bits == 32:
        options.append("-m32")
    elif bits != 64:
        raise ValueError(f"bits must be 32 or 64, not {bits}")
    if sanitize:
        options += ["-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]
    options += [f"-D{define}" for define in defines]
    options += [f"-I{include}" for include in includes]
    inputs = ["-x", language, *sources, "-x", "none"] if language else sources
    return [*compiler(apple), *options, *inputs, "-o", str(output), *(f"-l{lib}" for lib in libs), "-lm"]


def build(sources, *, output, host_stubs=False, port_stubs=False, stub_directory=None, cwd=None, **options) -> Path:
    """Compile and link `sources` to `output` and return its path. `options` are `command`'s. A failed build raises
    BuildError. `cwd` is where relative sources and options are read from (the repository root by default);
    `stub_directory` holds the host stub objects (the output's folder by default)."""
    output = Path(output)
    arguments = command(sources, output=output, **options)
    if host_stubs and not options.get("apple"):
        raise ValueError("host_stubs=True needs apple=True: the stubs are Objective-C (port/ios/tests/host_stubs.c)")
    if host_stubs or port_stubs:
        if str(ROOT / "tools") not in sys.path:
            sys.path.insert(0, str(ROOT / "tools"))
        import host_stubs as stubs
        flags = options.get("flags")
        given = arguments if flags is None else flags
        directory = Path(stub_directory or output.parent)
        objects = [stubs.stub_object(directory, stub_flags(given))] if host_stubs else []
        objects.append(stubs.port_stub_object(directory, compiler(bool(options.get("apple"))),
                                              stubs.port_stub_flags(given)))
        at = next(i for i in range(len(arguments) - 1) if arguments[i] == "-o" and arguments[i + 1] == str(output))
        arguments[at:at] = ["-x", "none", *map(str, objects)]   # with the inputs: libraries (-lm) come after them
    result = subprocess.run(arguments, cwd=cwd or ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            errors="replace")
    if result.returncode != 0:
        shown = result.stdout.splitlines()
        more = f"\n... {len(shown) - OUTPUT_LINES} more lines" if len(shown) > OUTPUT_LINES else ""
        raise BuildError(f"probe {output} did not build (exit {result.returncode}): {' '.join(arguments)}\n"
                         + "\n".join(shown[:OUTPUT_LINES]) + more)
    if result.stdout:
        print(result.stdout, end="", flush=True)
    return output
