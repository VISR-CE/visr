"""The one way a test finds production C and Objective-C source.

A test that finds code by its text breaks when the code is refactored: a function becomes a wrapper, a body moves
into a helper, a signature gains a parameter. These helpers find code by name, not by declaration text, and say
what happened when the name is gone:

- `function(path, name)`: a definition by name (C function or Objective-C method), from the start of its line to its
  closing brace, with comment- and string-aware brace matching. A missing name raises `LookupError` saying it may
  have been renamed, wrapped or split, with the closest names defined in the file.
- `body(path, name)`: the text inside that definition's braces.
- `reach(path, name, depth=3)` and `reach_contains(path, name, needle)`: the named function and the same-file
  functions it calls by name, transitively, so a wiring test survives a wrap or a split inside the file. It follows
  direct calls only (not function pointers, macros, message sends or other files), and it over-approximates: it can
  show that a path contains something, never that it does not.
- `calls_in_order(body, *calls)`: whether one body contains the calls in that order.
- `region(path, name)`: the text between `/* probe-region: name */` and `/* probe-region-end: name */` (markers go
  only in files under `port/`).
- `mutating(path, before, after)`: within the block, every helper reads `path` with one substitution applied (a
  negative control); `mutated(text, before, after)` does the same to a text.
- `structure(path, name)`: a named struct's definition through its semicolon.

Paths are relative to the repository root; files are read as latin-1, as the compiler reads them. Each helper has a
`*_in(text, ...)` form for text a test already holds.
"""
from __future__ import annotations

import contextlib
import difflib
import functools
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_overrides: dict[Path, str] = {}
IDENTIFIER = re.compile(r"[A-Za-z_]\w*")
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "do", "else", "case", "goto", "defined",
            "_Static_assert", "__attribute__", "__declspec", "_Alignof", "alignof", "typeof", "__typeof__"}


def _resolve(path) -> Path:
    path = Path(path)
    return (path if path.is_absolute() else ROOT / path).resolve()


def read(path) -> str:
    """A repository file as latin-1 text, with any `mutating` substitution applied."""
    resolved = _resolve(path)
    if resolved in _overrides:
        return _overrides[resolved]
    return resolved.read_text(encoding="latin-1")


def mask(text: str) -> str:
    """The text with every comment and string or character literal blanked (same length, newlines kept), so
    braces and names inside them do not count."""
    out = list(text)
    index, length = 0, len(text)
    def blank(start, end):
        for position in range(start, end):
            if out[position] != "\n":
                out[position] = " "
    while index < length:
        character = text[index]
        if text.startswith("//", index):
            end = text.find("\n", index)
            end = length if end == -1 else end
            blank(index, end)
            index = end
        elif text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = length if end == -1 else end + 2
            blank(index, end)
            index = end
        elif character in "\"'":
            end = index + 1
            while end < length and text[end] != character and text[end] != "\n":
                end += 2 if text[end] == "\\" else 1
            end = min(end + 1, length)
            blank(index + 1, end - 1)
            index = end
        else:
            index += 1
    return "".join(out)


def _closing(masked: str, opening: int) -> int:
    """The index just past the brace or parenthesis that closes the one at `opening`."""
    pair = {"{": "}", "(": ")", "[": "]"}[masked[opening]]
    depth = 0
    for index in range(opening, len(masked)):
        if masked[index] == masked[opening]:
            depth += 1
        elif masked[index] == pair:
            depth -= 1
            if depth == 0:
                return index + 1
    raise LookupError("unbalanced braces")


def _depths(masked: str) -> list[int]:
    depth, depths = 0, []
    for character in masked:
        depths.append(depth)
        depth += (character == "{") - (character == "}")
    return depths


_C_HEAD = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
_OBJC_HEAD = re.compile(r"^[ \t]*[-+][ \t]*\([^)]*\)[ \t]*([A-Za-z_]\w*)", re.M)
_ANNOTATION = re.compile(r"\s*(__attribute__|[A-Z_][A-Z0-9_]*)\b\s*")


def _after_annotations(masked: str, index: int) -> int:
    """Past the whitespace and the annotations between a signature and its body: `__attribute__((...))` and
    upper-case macros, with or without arguments (`API_AVAILABLE(ios(16.0))`, `NS_RETURNS_RETAINED`)."""
    while True:
        match = _ANNOTATION.match(masked, index)
        if not match:
            break
        index = match.end()
        if index < len(masked) and masked[index] == "(":
            index = _closing(masked, index)
    while index < len(masked) and masked[index].isspace():
        index += 1
    return index


def definitions(text: str) -> list[tuple[str, int, int, int]]:
    """Every top-level definition in a text: (name, start of its line, index of its opening brace, end)."""
    return list(_definitions(text))


@functools.lru_cache(maxsize=64)
def _definitions(text: str) -> tuple[tuple[str, int, int, int], ...]:
    masked = mask(text)
    depths = _depths(masked)
    found = []
    for match in _C_HEAD.finditer(masked):
        name = match.group(1)
        if name in KEYWORDS or depths[match.start()] != 0:
            continue
        line_start = masked.rfind("\n", 0, match.start()) + 1
        prefix = masked[line_start:match.start()]
        # a preprocessor line, or an annotation after a signature (`f(void) API_AVAILABLE(ios(16.0))`)
        if prefix.lstrip().startswith("#") or ")" in prefix:
            continue
        try:
            after_parameters = _closing(masked, match.end() - 1)
        except LookupError:
            continue
        try:
            brace = _after_annotations(masked, after_parameters)
        except LookupError:
            continue
        if brace >= len(masked) or masked[brace] != "{":
            continue
        found.append((name, line_start, brace, _closing(masked, brace)))
    for match in _OBJC_HEAD.finditer(masked):
        if depths[match.start(1)] != 0:
            continue
        brace = masked.find("{", match.end())
        semicolon = masked.find(";", match.end())
        if brace == -1 or (semicolon != -1 and semicolon < brace):
            continue
        line_start = masked.rfind("\n", 0, match.start(1)) + 1
        found.append((match.group(1), line_start, brace, _closing(masked, brace)))
    return tuple(sorted(found, key=lambda item: item[1]))


def _missing(where: str, name: str, names) -> LookupError:
    closest = difflib.get_close_matches(name, sorted(set(names)), n=5, cutoff=0.5)
    hint = f" Closest names: {', '.join(f'`{item}`' for item in closest)}." if closest else ""
    return LookupError(f"`{name}` is not defined in {where}: it may have been renamed, wrapped or split.{hint}")


def _find(text: str, name: str, where: str):
    found = definitions(text)
    for item in found:
        if item[0] == name:
            return item
    raise _missing(where, name, (item[0] for item in found))


def function_in(text: str, name: str, where: str = "the text") -> str:
    _, start, _, end = _find(text, name, where)
    return text[start:end]


def function(path, name: str) -> str:
    return function_in(read(path), name, str(path))


def body_in(text: str, name: str, where: str = "the text") -> str:
    """The text inside the definition's braces, without the newline after `{` and the one before `}`."""
    _, _, brace, end = _find(text, name, where)
    inner = text[brace + 1:end - 1]
    inner = inner[1:] if inner.startswith("\n") else inner
    return inner[:-1] if inner.endswith("\n") else inner


def body(path, name: str) -> str:
    return body_in(read(path), name, str(path))


def called_names(text: str) -> list[str]:
    """Names a body calls directly, in order of first call."""
    masked = mask(text)
    names = [match.group(1) for match in _C_HEAD.finditer(masked) if match.group(1) not in KEYWORDS]
    return list(dict.fromkeys(names))


def reach_in(text: str, name: str, depth: int = 3, where: str = "the text") -> dict[str, str]:
    found = {item[0]: item for item in reversed(definitions(text))}
    if name not in found:
        raise _missing(where, name, found)
    reached, frontier = {}, [name]
    for _ in range(depth + 1):
        following = []
        for current in frontier:
            if current in reached:
                continue
            _, start, _, end = found[current]
            reached[current] = text[start:end]
            following += [called for called in called_names(text[found[current][2]:end]) if called in found]
        frontier = following
    return reached


def reach(path, name: str, depth: int = 3) -> dict[str, str]:
    return reach_in(read(path), name, depth, str(path))


def reach_contains(path, name: str, needle: str, depth: int = 3) -> bool:
    return any(needle in text for text in reach(path, name, depth).values())


def calls_in_order(body_text: str, *calls: str) -> bool:
    position = 0
    for call in calls:
        found = body_text.find(call, position)
        if found == -1:
            return False
        position = found + len(call)
    return True


def region_in(text: str, name: str, where: str = "the text") -> str:
    begin, end = f"/* probe-region: {name} */", f"/* probe-region-end: {name} */"
    if text.count(begin) != 1 or text.count(end) != 1 or text.index(begin) > text.index(end):
        raise LookupError(f"{where} needs exactly one {begin} ... {end} pair")
    return text[text.index(begin) + len(begin):text.index(end)]


def region(path, name: str) -> str:
    if not str(_resolve(path)).startswith(str(ROOT / "port") + "/"):
        raise ValueError(f"probe-region markers go only in files under port/, not {path}")
    return region_in(read(path), name, str(path))


def structure_in(text: str, name: str, where: str = "the text") -> str:
    masked = mask(text)
    match = re.search(r"^struct\s+" + re.escape(name) + r"\s*\{", masked, re.M)
    if not match:
        names = re.findall(r"^struct\s+(\w+)\s*\{", masked, re.M)
        raise _missing(where, f"struct {name}", [f"struct {item}" for item in names])
    end = _closing(masked, masked.index("{", match.start()))
    return text[match.start():masked.index(";", end) + 1]


def structure(path, name: str) -> str:
    return structure_in(read(path), name, str(path))


def mutated(text: str, before: str, after: str) -> str:
    """The text with one deliberate fault; LookupError once the code no longer has it exactly once."""
    if text.count(before) != 1:
        raise LookupError(f"negative control no longer applies (found {text.count(before)} times): {before!r}")
    return text.replace(before, after, 1)


@contextlib.contextmanager
def mutating(path, before: str, after: str):
    resolved = _resolve(path)
    previous = _overrides.get(resolved)
    _overrides[resolved] = mutated(read(path), before, after)
    try:
        yield _overrides[resolved]
    finally:
        if previous is None:
            _overrides.pop(resolved, None)
        else:
            _overrides[resolved] = previous
