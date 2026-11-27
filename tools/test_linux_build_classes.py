"""Class tests for the native Linux build: shared code must not need what only the Apple builds provide.

The Linux release compile (`ninja linux`, .github/workflows/linux-compile.yml) is not run by push CI, so shared
files came to include headers, call host functions and use libc names that exist for the Apple builds only. Three
classes, each read from the sources the Linux build compiles (tools/linux_build.py names them and their include
directories, so nothing here restates the build):

- an include that resolves on the Apple include path only (port/runtime/include held `halo_ui_drag.h`);
- a call to a function only the Apple link defines (`host_development_hooks`, `map_pictures_render_active`);
- a C99 math macro in the game's strict-ANSI units, where glibc does not define it (`isfinite`).

The real check is the Linux build itself; these fail early and name the line.
"""

import json
import re
import sys
from pathlib import Path

from tools import linux_build

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probekit import source as probe_source  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
CONFIG = json.loads((ROOT / "port/linux/port.json").read_text())

# What an Apple-only conditional names: the Apple builds' macros, the Apple hosts' GL backend, and the three
# probes' own builds (MAP_PICTURES_*_TEST), which the Linux build never defines.
APPLE_ONLY = re.compile(r"\b(HALO_ILP32|HALO_IOS|HALO_APPLE|__APPLE__|GPU_GL_HOST|MAP_PICTURES_TEST|"
                        r"MAP_PICTURES_IMPORT_TEST|MAP_PICTURES_IMAGE_TEST)\b|\bTARGET_OS_")
DIRECTIVE = re.compile(r"^\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)$")
LITERAL = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'')
KEYWORDS = {"return", "else", "sizeof", "case", "goto", "if", "while", "for", "switch", "do"}


def blank_comments(text):
    """the text with its comments blanked, lines and strings kept (probekit.source's one comment splitter)"""
    return probe_source.mask(text, strings=False)


def apple_branch(word, rest):
    """(this branch is Apple-only, the #else of it is): a plain positive test of an Apple macro; a negated or
    combined (`!`, `||`) one is neither, which can only make the test stricter"""
    named = bool(APPLE_ONLY.search(rest))
    if word == "ifndef":
        return False, named
    if "!" in rest or "||" in rest:
        return False, False
    return named, False


def classified_lines(text):
    """(number, line, apple_only) for every line that is not a conditional directive"""
    stack = []  # per open conditional: [this branch is Apple-only, the else branch would be]
    for number, line in enumerate(blank_comments(text).splitlines(), 1):
        match = DIRECTIVE.match(line)
        if not match:
            if not line.lstrip().startswith("#"):
                line = LITERAL.sub('""', line)  # (a brace or semicolon inside a string ends no statement)
            yield number, line, any(branch[0] for branch in stack)
            continue
        word, rest = match.groups()
        if word in ("if", "ifdef", "ifndef"):
            stack.append(list(apple_branch(word, rest)))
        elif word == "elif" and stack:
            stack[-1] = list(apple_branch("if", rest))
        elif word == "else" and stack:
            stack[-1] = [stack[-1][1], False]
        elif word == "endif" and stack:
            stack.pop()


def statements(text):
    """(first line number, statement, apple_only) with each statement's lines joined, up to its `;`, `{` or `}`
    at the top parenthesis level; preprocessor lines stand alone"""
    pending, depth, first, apple = [], 0, 0, False
    for number, line, apple_only in classified_lines(text):
        if line.lstrip().startswith("#"):
            if not pending:
                yield number, line.strip(), apple_only
            else:
                pending.append(line)
            continue
        if not line.strip() and not pending:
            continue
        if pending and apple_only != apple:
            yield first, " ".join(part.strip() for part in pending), apple
            pending, depth = [], 0
        if not pending:
            first, apple = number, apple_only
        pending.append(line)
        depth += line.count("(") - line.count(")")
        if depth <= 0 and line.rstrip().endswith((";", "{", "}")):
            yield first, " ".join(part.strip() for part in pending), apple
            pending, depth = [], 0
    if pending:
        yield first, " ".join(part.strip() for part in pending), apple


TYPE = r"(?:(?:const|unsigned|signed|struct|enum|static|extern|inline)\s+)*[A-Za-z_]\w*"
DECLARATION = re.compile(rf"^(?:extern\s+)?{TYPE}[\s\*]+(\w+)\s*\([^{{}}]*\)\s*(?:__attribute__\s*\(\(.*\)\)\s*)?;$")
DEFINITION = re.compile(rf"^(static\s+)?(?:inline\s+)?{TYPE}[\s\*]+(\w+)\s*\([^{{}}]*\)\s*{{")
CALL = re.compile(r"\b(\w+)\s*\(")


def declared_name(statement):
    match = DECLARATION.match(statement)
    return match.group(1) if match and not _keyword_typed(statement, match.group(1)) else None


def defined_name(statement):
    """(name, static) of a function definition, else None"""
    match = DEFINITION.match(statement)
    if not match or _keyword_typed(statement, match.group(2)):
        return None
    return match.group(2), bool(match.group(1))


def _keyword_typed(statement, name):
    words = re.findall(r"\w+", statement.split(name, 1)[0])
    return name in KEYWORDS or bool(words and words[0] in KEYWORDS)


# ---------- the units and what each searches


def unit_kind(path):
    relative = path.relative_to(ROOT)
    if relative.parts[:3] == ("port", "linux", "src"):
        return "posix" if path.name.startswith("posix_") else "platform"
    return "game"


def unit_dirs(path):
    """every directory the Linux compile of this unit searches, from the build's own lists"""
    kind = unit_kind(path)
    if kind == "game":
        dirs = [*linux_build.game_include_dirs(CONFIG), *linux_build.game_quote_dirs(CONFIG)]
    elif kind == "platform":
        dirs = linux_build.platform_include_dirs(CONFIG)
    else:
        dirs = linux_build.posix_include_dirs(CONFIG, path.name)
    return [ROOT / directory for directory in [*dirs, linux_build.XDK_INCLUDE]]


def linux_sources():
    """the C files the Linux build compiles, which it names: the game, the port's game units, the platform layer"""
    sources = [ROOT / source for source in linux_build.game_sources(CONFIG)]
    for directory in (CONFIG["game_sources"], CONFIG["platform_sources"]):
        sources += sorted((ROOT / directory).glob("*.c"))
    return sources


def other_platform_headers():
    """headers only the other builds' include paths reach, by name"""
    found = {}
    for sub in ("ios", "macos", "web", "windows", "runtime"):
        for header in ROOT.joinpath("port", sub).rglob("*.h"):
            found.setdefault(header.name, header)
    return found


# ---------- the walker's own cases


def test_walker_tracks_apple_only_conditionals():
    text = "\n".join([
        "a", "#ifdef HALO_ILP32", "b", "#else", "c", "#endif",
        "#ifndef HALO_IOS", "d", "#else", "e", "#endif",
        "#if defined(HALO_IOS) || defined(OTHER)", "f", "#endif",
        "#if !defined(HALO_IOS)", "g", "#endif",
        "#ifdef HALO_IOS", "#if X", "h", "#elif Y", "i", "#endif", "#else", "j", "#endif",
        "#ifdef GPU_GL_HOST", "k", "#endif", "#ifdef FOO_TEST", "l", "#endif", "#ifdef MAP_PICTURES_TEST", "m", "#endif",
    ])
    apple = {line: only for _, line, only in classified_lines(text)}
    assert [name for name, only in apple.items() if only] == ["b", "e", "h", "i", "k", "m"]
    assert apple["f"] is False and apple["g"] is False and apple["j"] is False and apple["l"] is False


def test_statements_join_lines_and_split_declarations_from_definitions():
    text = "\n".join([
        "void host_wrapped(int a,", "\tint b);",
        "int", "host_defined(void)", "{", "\treturn host_called(1);", "\telse host_other();", "}",
        "static int local(void) {", "\tif (host_cond(2))", "\t\thost_in_if();", "}",
    ])
    kinds = []
    for _, statement, _ in statements(text):
        kinds.append((declared_name(statement), defined_name(statement)))
    assert kinds[0] == ("host_wrapped", None)
    assert kinds[1] == (None, ("host_defined", False))
    assert kinds[2] == (None, None) and kinds[3] == (None, None)  # `return host_called(1);`, `else host_other();`
    assert kinds[5] == (None, ("local", True)) or kinds[4] == (None, ("local", True))
    assert all(defined_name(s) is None for _, s, _ in statements("if (host_cond(2)) {\nelse if (x) {\n"))


# ---------- the classes


def test_linux_build_includes_only_headers_the_linux_compile_can_find():
    """an include that does not resolve where this unit's compile searches, though a header of that name exists
    where only the other builds look"""
    elsewhere = other_platform_headers()
    bad, seen = [], set()
    for source in linux_sources():
        dirs, pending = unit_dirs(source), [source]
        while pending:
            path = pending.pop()
            if (source, path) in seen:
                continue
            seen.add((source, path))
            for number, statement, apple_only in statements(path.read_text(errors="replace")):
                match = re.match(r'#\s*include\s*"([^"]+)"', statement)
                if apple_only or not match:
                    continue
                name = match.group(1)
                found = next((base / name for base in [path.parent, *dirs] if (base / name).is_file()), None)
                if found is not None:
                    if found.suffix == ".h" and "third_party" not in found.parts:
                        pending.append(found.resolve())
                elif Path(name).name in elsewhere:
                    bad.append(f"{path.relative_to(ROOT)}:{number}: \"{name}\" is {elsewhere[Path(name).name].relative_to(ROOT)}"
                               f" (reached from {source.relative_to(ROOT)})")
    assert not bad, "includes only the Apple builds resolve:\n" + "\n".join(sorted(set(bad)))


def apple_link_names(files):
    """the functions only the Apple link defines: the guest's host imports (port/runtime/guest/runtime/guest_host.h)
    and every non-static function defined inside an Apple-only conditional of a Linux-built file"""
    names = set()
    host = ROOT / "port/runtime/guest/runtime/guest_host.h"
    for _, statement, _ in statements(host.read_text()):
        name = declared_name(statement)
        if name:
            names.add(name)
    for path in files:
        if path.name.endswith("_test.c"):
            continue  # (a probe's own stand-ins are not the Apple link's: the Linux build defines these another way)
        for _, statement, apple_only in statements(path.read_text(errors="replace")):
            definition = defined_name(statement)
            if apple_only and definition and not definition[1]:
                names.add(definition[0])
    return names


def test_linux_build_defines_every_apple_link_function_it_calls():
    """a call outside an Apple-only conditional of a function only the Apple link defines, with no definition the
    Linux build compiles (project headers' inline ones included) beside it"""
    sources = linux_sources()
    headers = sorted(ROOT.joinpath("port/linux/include").glob("*.h")) + sorted(ROOT.joinpath("port/linux/src").glob("*.h"))
    apple_only_names = apple_link_names(sources + headers)
    defined, called = set(), {}
    for path in sources + headers:
        local = set()
        calls = []
        for _, statement, apple_only in statements(path.read_text(errors="replace")):
            if apple_only:
                continue
            definition = defined_name(statement)
            if definition:
                (local if definition[1] else defined).add(definition[0])
            elif declared_name(statement) is None and not statement.startswith("#"):
                calls += [name for name in CALL.findall(statement) if name in apple_only_names]
        defined |= local if path.suffix == ".h" else set()
        for name in calls:
            if name not in local:
                called.setdefault(name, path)
    missing = sorted(f"{name} (called in {path.relative_to(ROOT)})" for name, path in called.items() if name not in defined)
    assert not missing, "functions only the Apple link defines, called by the Linux build:\n" + "\n".join(missing)


def test_game_units_use_no_c99_math_macros():
    """the game compiles as gnu89 with __STRICT_ANSI__ (tools/linux_build.py GAME_FLAGS), where glibc's isfinite,
    isnan and the like are not macros, so a bare call links as an undefined function: `__builtin_` forms work"""
    macro = re.compile(r"(?<![\w])(isfinite|isnan|isinf|isnormal|signbit|fpclassify)\s*\(")
    files = [ROOT / source for source in linux_build.game_sources(CONFIG)]
    files += sorted(ROOT.joinpath(CONFIG["game_sources"]).glob("*.[ch]")) + sorted(ROOT.joinpath("port/linux/include").glob("*.h"))
    files += sorted(ROOT.joinpath("source").rglob("*.h"))
    bad = [f"{path.relative_to(ROOT)}:{number}" for path in files
           for number, line in enumerate(blank_comments(path.read_text(errors="replace")).splitlines(), 1) if macro.search(line)]
    assert not bad, "bare C99 math macros in game-compiled files:\n" + "\n".join(bad)
