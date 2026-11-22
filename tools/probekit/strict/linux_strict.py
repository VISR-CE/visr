"""Compiler wrapper that makes a Mac probe build fail where the Linux Tooling job would.

`tools/ci_test.py` puts this folder first on PATH on macOS, so tests that run
`cc` or `clang` reach the `cc` and `clang` scripts here, which run this file.
It runs the real compiler (the next one on PATH) and adds three checks that
imitate Ubuntu 24.04 with glibc:

1. A command with an ISO `-std=` (c99, c11, ...) and no feature-test macro of
   its own gets `-D_ANSI_SOURCE -D_C99_SOURCE`, which hides `M_PI`, `strdup`
   and `clock_gettime` the way glibc's strict mode does.
2. Every system header that a file outside the system directories includes
   directly must exist on Ubuntu 24.04 (`linux_headers.txt`). An include that
   only Apple builds reach carries an `/* apple-only */` comment on its line.
3. A link without `-lm` must not leave a libm function undefined
   (`linux_libm.txt`): libSystem provides them on macOS, glibc needs `-lm`.
   For clang the check reads an x86-64 build of the same sources, since arm64
   inlines calls that x86-64 Linux makes, and `-lm` before an input fails, as Ubuntu's `--as-needed` does.

Commands that cannot run on Linux anyway (Objective-C, frameworks, Apple
device targets) pass through unchanged. `HALO_LINUX_STRICT=0` turns the
wrapper into a pass-through. Failures print a line starting with
`linux-strict:` and exit 1.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ISO_STANDARDS = re.compile(r"-std=(c89|c90|c99|c9x|c11|c1x|c17|c18|c2x|c23|iso9899:.*)$")
FEATURE_MACROS = ("_POSIX_C_SOURCE", "_XOPEN_SOURCE", "_GNU_SOURCE", "_DEFAULT_SOURCE", "_BSD_SOURCE",
                  "_DARWIN_C_SOURCE", "_ANSI_SOURCE", "_C99_SOURCE")
STRICT_MACROS = ["-D_ANSI_SOURCE", "-D_C99_SOURCE"]
# arm64 turns sqrt, floor, ceil, fma, lround and others into instructions, so an arm64 binary hides calls that
# x86-64 Linux makes. The libm check reads an extra x86-64 build instead, with Linux's -fmath-errno default
# (Darwin's is -fno-math-errno) and the baseline x86-64 CPU Ubuntu builds for. Options that only fit the arm64 build are left out of it.
X86_TARGET = ["-target", "x86_64-apple-macos11", "-march=x86-64", "-fmath-errno"]
X86_DROPPED = ("-H", "-fsanitize", "-mcpu", "-march", "-mtune", "-arch", "-target", "--target", "-mmacos")
# Darwin's combined sine and cosine, which clang emits for sin(x) with cos(x).
SINCOS = {"__sincos_stret": ("sin", "cos"), "__sincosf_stret": ("sinf", "cosf")}
DEFINES_FEATURE_MACRO = re.compile(r"^\s*#\s*define\s+(" + "|".join(FEATURE_MACROS) + r")\b", re.MULTILINE)
C_SOURCES = {".c", ".i"}
APPLE_SOURCES = {".m", ".mm", ".M"}
# Options whose value is the next argument.
SEPARATE_VALUE = {
    "--output",
    "-o", "-I", "-D", "-U", "-include", "-imacros", "-isystem", "-iquote", "-idirafter", "-isysroot",
    "-x", "-L", "-l", "-arch", "-target", "-MF", "-MT", "-MQ", "-Xlinker", "-Xclang", "-Xpreprocessor",
    "-Xassembler", "-framework", "-F", "-iframework", "-install_name", "-rpath", "-u", "-e",
}
NO_LINK = {"-c", "-S", "-E", "-M", "-MM", "-fsyntax-only", "-emit-llvm", "--precompile"}
SHARED = {"-shared", "-dynamiclib", "-bundle", "-r", "-nostdlib", "-nodefaultlibs"}
APPLE_TARGET = re.compile(r"apple-(ios|tvos|xros|visionos|watchos)|-m(ios|tvos|xros|watchos)")
SYSTEM_PREFIXES = ("/opt/homebrew/", "/usr/local/", "/opt/local/", "/Library/Developer/", "/Applications/")


def real_compiler(name: str) -> str:
    """The first `name` on PATH that is not this folder."""
    directories = [entry for entry in os.environ.get("PATH", "").split(os.pathsep)
                   if entry and Path(entry).resolve() != HERE]
    found = shutil.which(name, path=os.pathsep.join(directories))
    if not found:
        sys.exit(f"linux-strict: no real {name} on PATH")
    return found


class Command:
    """What the strict checks need to know about one compiler command."""

    def __init__(self, arguments: list[str]) -> None:
        self.arguments = arguments
        self.inputs: list[str] = []
        self.output = "a.out"
        self.language = ""
        self.c_sources: list[str] = []
        self.apple = False
        self.link = True
        self.shared = False
        self.math = False
        self.math_index = -1
        self.last_input = -1
        self.iso = False
        self.feature_macro = False
        index = 0
        while index < len(arguments):
            argument = arguments[index]
            value = arguments[index + 1] if argument in SEPARATE_VALUE and index + 1 < len(arguments) else None
            joined = value if value is not None else ""
            if argument in ("-o", "--output") and value is not None:
                self.output = value
            elif argument.startswith("-o") and len(argument) > 2:
                self.output = argument[2:]
            elif argument.startswith("--output="):
                self.output = argument[len("--output="):]
            elif argument.startswith("-Wl,") and "-lm" in argument.split(",")[1:]:
                self.math = True
            elif argument == "-x" and value is not None:
                self.language = value
            elif argument.startswith("-x") and len(argument) > 2:
                self.language = argument[2:]
            elif argument in ("-lm", "-l:libm.so") or (argument == "-l" and value == "m"):
                self.math = True
                self.math_index = index
            elif argument in NO_LINK:
                self.link = False
            elif argument in SHARED:
                self.shared = True
            elif ISO_STANDARDS.match(argument):
                self.iso = True
            elif argument in ("-framework", "-ObjC", "-fobjc-arc", "-iframework") or argument.startswith("-fobjc"):
                self.apple = True
            elif argument in ("-D", "-U") or argument.startswith(("-D", "-U")):
                name = (joined if argument in ("-D", "-U") else argument[2:]).split("=")[0]
                if name in FEATURE_MACROS:
                    self.feature_macro = True
            elif argument.startswith("--target=") or argument in ("-target", "-arch", "-isysroot"):
                if APPLE_TARGET.search(joined or argument) or argument == "-isysroot":
                    self.apple = True
            elif argument.startswith("-m") and APPLE_TARGET.search(argument):
                self.apple = True
            elif not argument.startswith("-") or argument == "-":
                self.inputs.append(argument)
                self.last_input = index
                suffix = Path(argument).suffix
                if self.language == "c" or (self.language in ("", "none") and suffix in C_SOURCES):
                    self.c_sources.append(argument)
                elif self.language.startswith("objective-c") or (self.language in ("", "none")
                                                                and suffix in APPLE_SOURCES):
                    self.apple = True
            index += 2 if value is not None else 1

    def defines_feature_macro(self) -> bool:
        if self.feature_macro:
            return True
        for source in self.c_sources:
            try:
                if DEFINES_FEATURE_MACRO.search(Path(source).read_text(errors="replace")):
                    return True
            except OSError:
                pass
        return False


def describe(name: str, arguments: list[str]) -> str:
    """The command as the test wrote it, cut to a readable length."""
    text = shlex.join([name, *arguments])
    return text if len(text) <= 400 else text[:400] + " ..."


def lines(name: str) -> set[str]:
    return set((HERE / name).read_text().split())


def is_system(path: str) -> bool:
    return ("/usr/include/" in path or "/lib/clang/" in path or ".framework/" in path
            or path.startswith(SYSTEM_PREFIXES))


def include_name(path: str) -> str:
    """How a source file would name a system header it includes (`sys/time.h`, `mach/mach_time.h`)."""
    framework = re.search(r"/([^/]+)\.framework/(?:Versions/[^/]+/)?(?:Private)?Headers/(.+)$", path)
    if framework:
        return f"{framework.group(1)}/{framework.group(2)}"
    for marker in ("/usr/include/", "/lib/clang/"):
        if marker in path:
            rest = path.rsplit(marker, 1)[1]
            if marker == "/lib/clang/":
                rest = rest.split("/include/", 1)[1] if "/include/" in rest else rest
            return rest
    return path.rsplit("/include/", 1)[1] if "/include/" in path else Path(path).name


INCLUDE = re.compile(r"^\s*#\s*(?:include|import|include_next)\s*[<\"]([^>\"]+)[>\"](.*)$")


def include_lines(file: str, name: str) -> list[str]:
    """What follows each `#include` of `name` in `file` (its comments), one entry per include line."""
    try:
        text = Path(file).read_text(errors="replace")
    except OSError:
        return []
    found = []
    for line in text.splitlines():
        match = INCLUDE.match(line)
        if match and (match.group(1) == name or name.endswith("/" + match.group(1))
                      or match.group(1).endswith("/" + name)):
            found.append(match.group(2))
    return found


def header_problems(trace: list[tuple[int, str]], sources: list[str]) -> list[str]:
    """Directly included system headers that Ubuntu 24.04 does not have."""
    allowed = lines("linux_headers.txt")
    problems = []
    parents: dict[int, str] = {}
    for depth, path in trace:
        parents[depth] = path
        including = sources if depth == 1 else [parents.get(depth - 1, "")]
        if depth > 1 and is_system(including[0]):
            continue
        if not is_system(path):
            continue
        name = include_name(path)
        # The compiler's own headers (arm_neon.h, stdatomic.h, ...) ship with clang and gcc on every platform;
        # an architecture-specific one is reached only on that architecture.
        if name in allowed or "/lib/clang/" in path:
            continue
        # With several sources in one command, -H does not say which one included a header.
        including = [file for file in including if include_lines(file, name)] or including
        if any("apple-only" in rest for file in including for rest in include_lines(file, name)):
            continue
        problems.append(f"{', '.join(including)} includes <{name}>, which Ubuntu 24.04 does not have; "
                        "guard it for Apple builds and mark the line /* apple-only */")
    return list(dict.fromkeys(problems))


def split_trace(stderr: str) -> tuple[list[tuple[int, str]], str]:
    """Separate `-H` include-tree lines from the compiler's own diagnostics."""
    trace = []
    kept = []
    guards = False
    for line in stderr.splitlines(keepends=True):
        match = re.match(r"^(\.+) (.+)$", line.rstrip("\n"))
        if match:
            # Clang also lists the SDKSettings.json and sanitizer ignore lists it reads; only headers count.
            if match.group(2).endswith(".h"):
                trace.append((len(match.group(1)), os.path.abspath(match.group(2))))
            guards = False
        elif line.startswith("Multiple include guards may be useful for:"):
            guards = True
        elif guards and os.path.exists(line.strip()):
            continue
        else:
            guards = False
            kept.append(line)
    return trace, "".join(kept)


def x86_build(compiler: str, command: Command) -> str | None:
    """Link the same C sources for x86-64 into a temporary file, or None when the command does not allow it."""
    if any(not (source in command.c_sources) for source in command.inputs):
        return None
    arguments = []
    skip = False
    for index, argument in enumerate(command.arguments):
        if skip:
            skip = False
            continue
        if argument in ("-o", "--output", "-arch", "-target"):
            skip = True
            continue
        if argument.startswith(X86_DROPPED) or argument.startswith(("-o", "--output=")):
            continue
        arguments.append(argument)
    handle, path = tempfile.mkstemp(prefix="linux-strict-x86-")
    os.close(handle)
    result = subprocess.run([compiler, *arguments, *X86_TARGET, "-o", path], capture_output=True)
    if result.returncode != 0:
        # The compiler deletes its output when the build fails; the x86-64 build is optional, so skip the check.
        Path(path).unlink(missing_ok=True)
        return None
    return path


def math_problems(output: str, shown: str) -> list[str]:
    result = subprocess.run(["nm", "-u", output], capture_output=True, text=True)
    if result.returncode != 0:
        return []
    libm = lines("linux_libm.txt")
    undefined = {symbol[1:] if symbol.startswith("_") else symbol for symbol in result.stdout.split()}
    for combined, parts in SINCOS.items():
        if combined in undefined:
            undefined.update(parts)
    names = sorted(undefined & libm)
    if not names:
        return []
    return [f"{shown} calls {', '.join(names)}: links on macOS, needs -lm on Linux"]


def main() -> int:
    name, arguments = sys.argv[1], sys.argv[2:]
    compiler = real_compiler(name)
    if os.environ.get("HALO_LINUX_STRICT") == "0":
        os.execv(compiler, [compiler, *arguments])
    command = Command(arguments)
    if command.apple or (not command.c_sources and not command.link):
        os.execv(compiler, [compiler, *arguments])
    extra = []
    strict_macros = bool(command.c_sources) and command.iso and not command.defines_feature_macro()
    if command.c_sources:
        extra.append("-H")
        if strict_macros:
            extra += STRICT_MACROS
    result = subprocess.run([compiler, *arguments, *extra], stderr=subprocess.PIPE, text=True, errors="replace")
    trace, diagnostics = split_trace(result.stderr)
    sys.stderr.write(diagnostics)
    if result.returncode != 0:
        if strict_macros and "error" in diagnostics:
            sys.stderr.write("linux-strict: compiled with -D_ANSI_SOURCE -D_C99_SOURCE to match glibc's strict "
                             "ISO C headers, which hide M_PI, strdup and clock_gettime; command: "
                             f"{describe(name, arguments)}\n")
        return result.returncode
    problems = header_problems(trace, [os.path.realpath(source) for source in command.c_sources])
    # clang links libm itself for its sanitizer runtimes, so a sanitized clang link needs no -lm on Linux either.
    sanitized = name == "clang" and any(argument.startswith("-fsanitize=") for argument in arguments)
    if command.link and not command.shared and not command.math and not sanitized and os.path.exists(command.output):
        # Only for clang, whose x86-64 code generation the Mac's clang reproduces; Ubuntu's cc is gcc, which
        # inlines and folds differently (it drops ceil of a converted integer), so cc keeps the arm64 check.
        x86 = x86_build(compiler, command) if name == "clang" else None
        try:
            problems += math_problems(x86 or command.output, command.output)
        finally:
            if x86:
                Path(x86).unlink(missing_ok=True)
    if command.link and 0 <= command.math_index < command.last_input:
        problems.append("-lm comes before an input file: Ubuntu links with --as-needed, so libm is dropped "
                        "before anything needs it; put -lm after the sources")
    for problem in problems:
        sys.stderr.write(f"linux-strict: {problem}; command: {describe(name, arguments)}\n")
    if problems:
        if command.link and os.path.exists(command.output):
            os.remove(command.output)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
