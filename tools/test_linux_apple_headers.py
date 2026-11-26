"""Class test: the Linux build includes no header that only the Apple builds can find.

The Linux release compile (`ninja linux`, .github/workflows/linux-compile.yml) is not run
by push CI, so a shared file that includes a header from a directory only the Apple
builds search (port/ios, port/runtime/include, ...) broke it unnoticed. Bug class: an
include that resolves on the Apple include path only, outside an Apple-only guard.

This reads the quoted includes of every file the Linux build compiles (and of the project
headers they reach), resolves each one the way the Linux compile does, and fails on one
that does not resolve while a header of that name exists elsewhere in the tree, unless it
sits inside a `#if` that names an Apple-only macro.
"""

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = json.loads((ROOT / "port/linux/port.json").read_text())
APPLE_GUARD = re.compile(r"HALO_ILP32|HALO_IOS|HALO_APPLE|__APPLE__|TARGET_OS_")
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
