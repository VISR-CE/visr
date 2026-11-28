"""The one way a test finds production C and Objective-C source.

A test that finds code by its text breaks when the code is refactored: a function becomes a wrapper, a body moves
into a helper, a signature gains a parameter. These helpers find code by name, not by declaration text, and say
what happened when the name is gone:

- `function(path, name, which=None)`: a definition by name (C function, or Objective-C method by its first selector
  part or its full selector, `drawFrame:withScale:`), from the start of its line to its closing brace, with comment-
  and string-aware brace matching. A return type on the line before is not included. A missing name raises
  `LookupError` saying it may have been renamed, wrapped or split, with the closest names defined in the file. A name
  defined more than once (`#if` variants, methods in several `@implementation` blocks) raises `LookupError` too,
  unless `which` picks one: a string that only the chosen definition contains, or an index.
- `defined(path, name)`: whether the file defines it.
- `body(path, name)`: the text inside that definition's braces; `inline(path, name)`: the definition with its first
  `__inline` made `static`.
- `reach(path, name, depth=3)`, `reach_contains(path, name, needle)` and `assert_reaches(path, name, needle)`: the
  named function and the same-file functions it calls by name, transitively, every definition of each name, so a
  wiring test survives a wrap or a split inside the file. It follows direct calls only (not function pointers,
  macros, message sends or other files), and it over-approximates: it can show that a path contains something, never
  that it does not. Comments do not count; strings do (`config_integer("debug.ssao")`).
- `calls_in_order(body, *calls)`: whether one body contains the calls in that order, outside comments.
- `region(path, name)`: the text between `/* probe-region: name */` and `/* probe-region-end: name */` (markers go
  only in files under `port/`).
- `mutating(path, before, after)`: within the block, every helper reads `path` with one substitution applied (a
  negative control); `mutated(text, before, after)` does the same to a text.
- `structure(path, name)`: a named struct's definition through its semicolon.
- `enumeration(path, member)`: the `enum` whose list holds the enumerator `member`, from `enum` through its
  semicolon (named or anonymous, `typedef` or not), for a probe that needs the real values.
- `block(path, marker)`: the brace block that opens first at or after the start of `marker`, a piece of code that
  occurs exactly once outside comments (`"if (!misfired)"`, `"{\\n\\t\\treal scale;\\n\\n\\t\\tright_angle"`), through its
  closing brace: for an unnamed block in a file under `source/`.
  A function, struct or enum has its own helper; in `port/`, prefer a `region()` marker.
- `span(path, first, last)`: the text from the line that defines `first` (a function, or a `#define`) through the end
  of the function `last`, for a probe that needs a run of definitions with the macros and file-scope variables
  between them (a file under `source/`, where `region()` markers do not go). Each name is found as `function()`
  finds it, with the same message when it is gone, and `last` must come after `first`.

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


def mask(text: str, strings: bool = True) -> str:
    """The text with every comment, and unless `strings` is false every string or character literal, blanked (same
    length, newlines kept), so braces and names inside them do not count."""
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
            if strings:
                blank(index + 1, end - 1)
            index = end
        else:
            index += 1
    return "".join(out)


class UnbalancedBraces(LookupError):
    pass


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
    raise UnbalancedBraces("unbalanced braces")


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


def definitions(text: str, where: str = "the text") -> list[tuple[str, int, int, int]]:
    """Every top-level definition in a text: (name, start of its line, index of its opening brace, end). An Objective-C
    method is named by its first selector part; `selector()` gives the full one."""
    try:
        return list(_definitions(text))
    except UnbalancedBraces:
        raise UnbalancedBraces(f"{where} has unbalanced braces (an #if branch that opens a brace it does not close?)") \
            from None


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
        if masked[line_start:match.start()].lstrip().startswith("#"):
            continue
        # past a leading annotation on the line (`__attribute__((unused)) static void f(`); a `)` still before the
        # name is a call or an annotation after a signature (`f(void) API_AVAILABLE(ios(16.0))`), not a definition
        try:
            after = min(_after_annotations(masked, line_start), match.start())
        except LookupError:
            after = line_start
        if ")" in masked[after:match.start()]:
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


def selector(text: str, item) -> str:
    """An Objective-C method's full selector (`drawFrame:withScale:`), or its name when it takes no arguments; a C
    function's name."""
    name, start, brace, _ = item
    header = mask(text)[start:brace]
    if not re.match(r"[ \t]*[-+]", header):
        return name
    without_types = re.sub(r"\([^()]*(?:\([^()]*\)[^()]*)*\)", " ", header)
    parts = re.findall(r"([A-Za-z_]\w*)\s*:", without_types)
    return "".join(f"{part}:" for part in parts) or name


def _missing(where: str, name: str, names) -> LookupError:
    closest = difflib.get_close_matches(name, sorted(set(names)), n=5, cutoff=0.5)
    hint = f" Closest names: {', '.join(f'`{item}`' for item in closest)}." if closest else ""
    return LookupError(f"`{name}` is not defined in {where}: it may have been renamed, wrapped or split.{hint}")


def _named(text: str, name: str, where: str) -> list:
    found = definitions(text, where)
    if ":" in name:
        return [item for item in found if item[0] == name.split(":", 1)[0] and selector(text, item) == name]
    return [item for item in found if item[0] == name]


def _find(text: str, name: str, where: str, which=None):
    matches = _named(text, name, where)
    if not matches:
        raise _missing(where, name, (item[0] for item in definitions(text, where)))
    if isinstance(which, int):
        if not -len(matches) <= which < len(matches):
            raise LookupError(f"`{name}` has {len(matches)} definitions in {where}, so which={which} is out of range")
        return matches[which]
    if isinstance(which, str):
        chosen = [item for item in matches if which in text[item[1]:item[3]]]
        if len(chosen) != 1:
            raise LookupError(f"{len(chosen)} of the {len(matches)} definitions of `{name}` in {where} contain "
                              f"{which!r}; pass a string only the one you want contains")
        return chosen[0]
    if len(matches) > 1:
        lines = ", ".join(str(text.count("\n", 0, item[1]) + 1) for item in matches)
        raise LookupError(f"`{name}` is defined {len(matches)} times in {where} (lines {lines}: #if variants or "
                          f"several @implementation blocks); pass which= a string only the one you want contains")
    return matches[0]


def function_in(text: str, name: str, where: str = "the text", which=None) -> str:
    _, start, _, end = _find(text, name, where, which)
    return text[start:end]


def function(path, name: str, which=None) -> str:
    return function_in(read(path), name, str(path), which)


def defined_in(text: str, name: str) -> bool:
    return bool(_named(text, name, "the text"))


def defined(path, name: str) -> bool:
    """Whether the file defines a function or method of that name."""
    return defined_in(read(path), name)


def inline_in(text: str, name: str, where: str = "the text", which=None) -> str:
    """A definition with its first `__inline` made `static`, so a probe can compile a header function itself."""
    return function_in(text, name, where, which).replace("__inline", "static", 1)


def inline(path, name: str, which=None) -> str:
    return inline_in(read(path), name, str(path), which)


def body_in(text: str, name: str, where: str = "the text", which=None) -> str:
    """The text inside the definition's braces, without the newline after `{` and the one before `}`."""
    _, _, brace, end = _find(text, name, where, which)
    inner = text[brace + 1:end - 1]
    inner = inner[1:] if inner.startswith("\n") else inner
    return inner[:-1] if inner.endswith("\n") else inner


def body(path, name: str, which=None) -> str:
    return body_in(read(path), name, str(path), which)


def called_names(text: str) -> list[str]:
    """Names a body calls directly, in order of first call."""
    masked = mask(text)
    names = [match.group(1) for match in _C_HEAD.finditer(masked) if match.group(1) not in KEYWORDS]
    return list(dict.fromkeys(names))


def reach_in(text: str, name: str, depth: int = 3, where: str = "the text") -> dict[str, str]:
    """name -> the text of every definition of it (joined by a blank line), comments blanked, strings kept."""
    found: dict[str, list] = {}
    for item in definitions(text, where):
        found.setdefault(item[0], []).append(item)
    if name not in found:
        raise _missing(where, name, found)
    uncommented = mask(text, strings=False)
    reached, frontier = {}, [name]
    for _ in range(depth + 1):
        following = []
        for current in frontier:
            if current in reached:
                continue
            reached[current] = "\n\n".join(uncommented[start:end] for _, start, _, end in found[current])
            for _, _, brace, end in found[current]:
                following += [called for called in called_names(text[brace:end]) if called in found]
        frontier = following
    return reached


def reach(path, name: str, depth: int = 3) -> dict[str, str]:
    return reach_in(read(path), name, depth, str(path))


def reach_contains(path, name: str, needle: str, depth: int = 3) -> bool:
    return any(needle in text for text in reach(path, name, depth).values())


def assert_reaches(path, name: str, needle: str, depth: int = 3) -> None:
    """`reach_contains` as an assertion that names the functions it searched when the needle is missing."""
    reached = reach(path, name, depth)
    if not any(needle in text for text in reached.values()):
        raise AssertionError(f"{needle!r} is not in `{name}` or the same-file functions it reaches in {path} "
                             f"(searched: {', '.join(reached)})")


def calls_in_order(body_text: str, *calls: str) -> bool:
    body_text = mask(body_text, strings=False)
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


_DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)", re.M)


def _span_end(text: str, name: str, where: str, macro: bool):
    """(start, end) of the definition bounding a span: `#define name` (when `macro`) or the function `name`."""
    masked = mask(text)
    defines = [match for match in _DEFINE.finditer(masked) if match.group(1) == name] if macro else []
    if len(defines) > 1:
        lines = ", ".join(str(text.count("\n", 0, match.start()) + 1) for match in defines)
        raise LookupError(f"`{name}` is #defined {len(defines)} times in {where} (lines {lines}), so it does not "
                          f"bound one span")
    if defines:
        return defines[0].start(), defines[0].end()
    matches = _named(text, name, where)
    if len(matches) > 1:
        lines = ", ".join(str(text.count("\n", 0, item[1]) + 1) for item in matches)
        raise LookupError(f"`{name}` is defined {len(matches)} times in {where} (lines {lines}: #if variants?), so "
                          f"it does not bound one span; bound it by a name defined once")
    if not matches:
        names = [item[0] for item in definitions(text, where)] + \
            ([match.group(1) for match in _DEFINE.finditer(masked)] if macro else [])
        closest = difflib.get_close_matches(name, sorted(set(names)), n=5, cutoff=0.5)
        hint = f" Closest names: {', '.join(f'`{item}`' for item in closest)}." if closest else ""
        kind = f"no `#define {name}` and no function `{name}`" if macro else f"no function `{name}`"
        raise LookupError(f"{where} has {kind} to bound a span: it may have been renamed, wrapped or split.{hint}")
    return matches[0][1], matches[0][3]


def span_in(text: str, first: str, last: str, where: str = "the text") -> str:
    start, _ = _span_end(text, first, where, macro=True)
    last_start, end = _span_end(text, last, where, macro=False)
    if last_start < start:
        raise LookupError(f"`{last}` comes before `{first}` in {where}, so they do not bound a span")
    return text[start:end]


def span(path, first: str, last: str) -> str:
    return span_in(read(path), first, last, str(path))


def structure_in(text: str, name: str, where: str = "the text") -> str:
    masked = mask(text)
    match = re.search(r"^struct\s+" + re.escape(name) + r"\s*\{", masked, re.M)
    if not match:
        names = re.findall(r"^struct\s+(\w+)\s*\{", masked, re.M)
        raise _missing(where, f"struct {name}", [f"struct {item}" for item in names])
    try:
        end = _closing(masked, masked.index("{", match.start()))
    except UnbalancedBraces:
        raise UnbalancedBraces(f"{where} has unbalanced braces (an #if branch that opens a brace it does not close?)") \
            from None
    return text[match.start():masked.index(";", end) + 1]


def structure(path, name: str) -> str:
    return structure_in(read(path), name, str(path))


_ENUM = re.compile(r"\benum\b(?:\s+\w+)?\s*\{")


def enumeration_in(text: str, member: str, where: str = "the text") -> str:
    masked = mask(text)
    found = []
    for match in _ENUM.finditer(masked):
        opening = masked.index("{", match.start())
        try:
            end = _closing(masked, opening)
        except UnbalancedBraces:
            raise UnbalancedBraces(f"{where} has unbalanced braces (an #if branch that opens a brace it does not "
                                   f"close?)") from None
        if re.search(r"(?:^|[{,\s])" + re.escape(member) + r"\b\s*(?:=|,|})", masked[opening:end]):
            found.append((match.start(), masked.index(";", end) + 1))
    if len(found) != 1:
        lines = ", ".join(str(text.count("\n", 0, start) + 1) for start, _ in found)
        detail = f"is in {len(found)} enums (lines {lines})" if found else "is in no enum: it may have been renamed"
        raise LookupError(f"`{member}` {detail} in {where}")
    start, end = found[0]
    return text[start:end]


def enumeration(path, member: str) -> str:
    return enumeration_in(read(path), member, str(path))


_VALUE = r"(0x[0-9A-Fa-f]+|\d+(?:\.\d*)?|[A-Za-z_]\w*)"


def constant_in(text: str, name: str, where: str = "the text") -> str:
    """The value token an enumerator (`name = value`) or a `#define name value` sets, outside comments and strings:
    an integer or decimal literal, or another name. LookupError unless exactly one such setting exists."""
    masked = mask(text)
    found = [m for m in re.finditer(r"(?:^|[{,\s])" + re.escape(name) + r"\s*=\s*" + _VALUE + r"\s*[,}]", masked)]
    found += [m for m in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+" + re.escape(name) + r"[ \t]+" + _VALUE
                                     + r"[ \t]*$", masked, re.MULTILINE)]
    values = {m.group(1) for m in found}
    if len(values) != 1:
        detail = f"is set to {len(values)} different values ({', '.join(sorted(values))})" if values else \
            "is not set by an enumerator or #define: it may have been renamed"
        raise LookupError(f"`{name}` {detail} in {where}")
    return values.pop()


def constant(path, name: str) -> str:
    return constant_in(read(path), name, str(path))


def block_in(text: str, marker: str, where: str = "the text") -> str:
    masked = mask(text, strings=False)
    count = masked.count(marker)
    if count != 1:
        raise LookupError(f"{where} has {count} copies of {marker!r} outside comments; a block() marker must occur "
                          f"exactly once")
    start = masked.index(marker)
    masked_all = mask(text)
    opening = masked_all.find("{", start)   # a marker may start with its own brace
    if opening < 0:
        raise LookupError(f"no brace opens after {marker!r} in {where}")
    try:
        end = _closing(masked_all, opening)
    except UnbalancedBraces:
        raise UnbalancedBraces(f"{where} has unbalanced braces after {marker!r}") from None
    return text[start:end]


def block(path, marker: str) -> str:
    return block_in(read(path), marker, str(path))


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
