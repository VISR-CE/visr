"""Class test: the Linux build includes no header that only the Apple builds can find.

The Linux release compile (`ninja linux`, .github/workflows/linux-compile.yml) is not run
by push CI, so a shared file that includes a header from a directory only the Apple
builds search (port/ios, port/runtime/include, ...) broke it unnoticed. Bug class: an
include that resolves on the Apple include path only, outside an Apple-only guard.

This reads the quoted includes of every file the Linux build compiles (and of the project
headers they reach), resolves each one the way the Linux compile does, and fails on one
that does not resolve while a header of that name exists elsewhere in the tree, unless it
sits inside a `#if` that names an Apple-only macro.

A second class: the game calls a host function the Linux link does not define. A `host_*`
or `map_pictures_*` function the Linux-built code calls outside an Apple-only conditional
must be defined by a file the Linux build compiles (the Apple hosts define theirs in
port/ios/host, which Linux does not link): `ninja linux` failed to link on
`host_development_hooks` and `map_pictures_render_active` that way.
"""

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = json.loads((ROOT / "port/linux/port.json").read_text())
# (GPU_GL_HOST: the Apple hosts' GL backend; *_TEST: a probe's own build)
APPLE_GUARD = re.compile(r"HALO_ILP32|HALO_IOS|HALO_APPLE|__APPLE__|TARGET_OS_|GPU_GL_HOST|[A-Z]_TEST\b")
DIRECTIVE = re.compile(r"^\s*#\s*(ifdef|ifndef|if|elif|else|endif|include)\b(.*)$")
QUOTED = re.compile(r'^\s*"([^"]+)"')


def search_dirs():
    """the directories the Linux compiles search for a quoted include, besides the file's own"""
    game = CONFIG["game"]
    dirs = [ROOT / "port/linux/src", ROOT / "port/linux/include", ROOT / "source", ROOT / "source/cseries",
            ROOT / CONFIG["game_sources"]]
    dirs += [ROOT / directory for directory in game.get("include_dirs", [])]
    # the third-party units' own directories (tools/linux_build.py adds them per unit)
    for library in (ROOT / "port/third_party").iterdir():
        dirs += [library, library / "include"]
    return dirs


def linux_sources():
    excluded = set(CONFIG["game"].get("exclude", []))
    sources = [p for p in (ROOT / CONFIG["game"]["root"]).rglob("*.c") if p.relative_to(ROOT).as_posix() not in excluded]
    for directory in ("port/linux/src", CONFIG["game_sources"]):
        sources += (ROOT / directory).glob("*.c")
    return sources


def unguarded_includes(path):
    """the quoted includes of a file that no Apple-only conditional encloses"""
    stack = []  # one entry per open conditional: True when this branch is Apple-only
    found = []
    for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        match = DIRECTIVE.match(line)
        if not match:
            continue
        word, rest = match.groups()
        if word in ("if", "ifdef"):
            stack.append(bool(APPLE_GUARD.search(rest)) and "!" not in rest)
        elif word == "ifndef":
            stack.append(False)
        elif word == "elif":
            stack[-1:] = [bool(APPLE_GUARD.search(rest)) and "!" not in rest] if stack else []
        elif word == "else":
            if stack:
                stack[-1] = False
        elif word == "endif":
            if stack:
                stack.pop()
        elif word == "include" and not any(stack):
            quoted = QUOTED.match(rest)
            if quoted:
                found.append((number, quoted.group(1)))
    return found


HOST_NAME = re.compile(r"\b((?:host_|map_pictures_)\w+)\s*\(")
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
DECLARATION = re.compile(r"^\s*(?:extern\s+)?(?:const\s+|unsigned\s+|struct\s+|enum\s+)*[A-Za-z_]\w*[\s\*]+\*?(\w+)\s*\([^;{}]*\)\s*;\s*$")
DEFINITION = re.compile(r"^(static\s+)?(?:inline\s+)?(?:const\s+|unsigned\s+|struct\s+|enum\s+)*[A-Za-z_]\w*[\s\*]+\*?(\w+)\s*\(")


def host_calls_and_definitions(path):
    """(names called, names defined non-static, names defined static) outside Apple-only conditionals"""
    text = COMMENT.sub(lambda m: re.sub(r"[^\n]", "", m.group(0)), path.read_text(errors="replace"))
    stack, calls, defined, local = [], set(), set(), set()
    for line in text.splitlines():
        match = DIRECTIVE.match(line)
        if match:
            word, rest = match.groups()
            if word in ("if", "ifdef"):
                stack.append(bool(APPLE_GUARD.search(rest)) and "!" not in rest)
            elif word == "ifndef":
                stack.append(False)
            elif word == "elif":
                stack[-1:] = [bool(APPLE_GUARD.search(rest)) and "!" not in rest] if stack else []
            elif word == "else" and stack:
                stack[-1] = False
            elif word == "endif" and stack:
                stack.pop()
            continue
        if any(stack) or line.startswith("#"):
            continue
        declaration = DECLARATION.match(line)
        definition = DEFINITION.match(line)
        if definition and not declaration and HOST_NAME.match(definition.group(2) + "("):
            (local if definition.group(1) else defined).add(definition.group(2))
        elif not declaration:
            calls.update(HOST_NAME.findall(line))
    return calls, defined, local


def test_linux_build_defines_every_host_function_it_calls():
    called = {}
    defined = set()
    for path in linux_sources():
        calls, defs, local = host_calls_and_definitions(path)
        defined |= defs
        for name in calls - local:
            called.setdefault(name, path)
    missing = sorted(f"{name} (called in {path.relative_to(ROOT)})" for name, path in called.items() if name not in defined)
    assert not missing, "host functions called by the Linux build but defined by none of its files:\n" + "\n".join(missing)


def resolves(path, name, dirs):
    return any((base / name).is_file() for base in [path.parent, *dirs])


def test_linux_build_includes_only_headers_the_linux_compile_can_find():
    dirs = search_dirs()
    tree = {}
    # headers the Linux search path does not reach: the other platforms' and the shared runtime's
    for sub in ("ios", "macos", "web", "windows", "runtime"):
        for header in ROOT.joinpath("port", sub).rglob("*.h"):
            tree.setdefault(header.name, header)
    pending, seen, bad = list(linux_sources()), set(), []
    while pending:
        path = pending.pop()
        if path in seen:
            continue
        seen.add(path)
        for number, name in unguarded_includes(path):
            if resolves(path, name, dirs):
                target = next(base / name for base in [path.parent, *dirs] if (base / name).is_file())
                if target.suffix == ".h" and "third_party" not in target.parts:
                    pending.append(target.resolve())
            elif Path(name).name in tree:  # a project header the Linux search path does not reach
                bad.append(f"{path.relative_to(ROOT)}:{number}: \"{name}\" is {tree[Path(name).name].relative_to(ROOT)}")
    assert not bad, "includes only the Apple builds resolve:\n" + "\n".join(sorted(bad))


def test_game_units_use_no_c99_math_macros():
    """the game compiles as gnu89 with __STRICT_ANSI__, where glibc's isfinite, isnan and the like are not
    macros, so a bare call links as an undefined function: `__builtin_` forms work everywhere"""
    macro = re.compile(r"(?<![\w])(isfinite|isnan|isinf|isnormal|signbit|fpclassify)\s*\(")
    files = list(ROOT.joinpath(CONFIG["game_sources"]).glob("*.[ch]")) + list(ROOT.joinpath("port/linux/include").glob("*.h"))
    bad = [f"{p.relative_to(ROOT)}:{n}" for p in files
           for n, line in enumerate(COMMENT.sub("", p.read_text(errors="replace")).splitlines(), 1) if macro.search(line)]
    assert not bad, "bare C99 math macros in game-compiled files:\n" + "\n".join(bad)
