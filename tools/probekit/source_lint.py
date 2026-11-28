"""The one detector of tests that find production source by its text, for tools/test_probe_conventions.py's
rules 3a and 3b.

- `extractor_findings(path)`: rule 3a. Line numbers of the places a Python file finds C or Objective-C source by
  its declaration text instead of through `probekit.source`:
  - a private extractor: a function that matches braces itself, by comparing against or counting both `"{"` and `"}"`,
    or with a regular expression that holds both braces (`r"enum\\s*\\{[^}]*\\};"`), or that ends a function at the first closing brace
    at the start of a line (`text.index("\\n}\\n", start)`);
  - a search whose literal pattern starts with a C declaration: `.index`, `.rindex`, `.find`, `.rfind`, `.split`,
    `.rsplit`, `.partition` or `.rpartition` (also called as `str.index(text, ...)`), or any `re` function
    (`re.search(...)`, `import re as x`, `from re import search`). The pattern is the literal at the start of the
    argument: a string, the first part of an f-string, or the left of a `+`, or, for a name, any value assigned to
    it in the module (both branches of a conditional expression).
  A declaration starts with a storage class, qualifier or type keyword (`static`, `struct`, `enum`, `void`,
  `boolean`, ...), `#define`, `typedef`, an Objective-C method (`- (`, `+ (`) or `@implementation`, or has the
  shape `type name(`. In a regular expression, escapes (`\\s*`, `\\(`, `\\*`, `\\n`, a leading `^`) are read as
  the text they match, and a leading group is read as each of its alternatives and, when it is optional, as
  absent too (`(?:static )?void f\\(` is a declaration either way).
- `extractor_definitions(path)`: the module-level functions rule 3a flags, which `imported_extractor_findings`
  counts again in a file that imports one by name (`from test_network_discovery import function`).
- `literal_assert_findings(path)`: rule 3b. Line numbers of checks that a literal statement is in C text, where the
  literal holds `(`, `;` or an operator:
  - in an `assert`, or in an `if` whose body raises: `"<literal>" in <C text>` or `not in`, `<C text>.count(...)`,
    `.find`, `.rfind`, `.index` or `.rindex` of the literal, and a needle that is a name bound to such a literal
    (assigned, or a loop or comprehension over a tuple of literals: `all(n in text for n in ("f(x);", ...))`);
  - a `reach_contains` or `assert_reaches` needle that is more than a name or `name(`.
  C text is read from a C, Objective-C, Metal or include file: `.read_text()`/`.read_bytes()`/`.read()`/`open()` of
  a path naming one, a local helper that reads a path built from its parameter (`read("port/x.c")`), a function
  imported from another module and given C text (`function(source, "f(")`), any
  `probekit.source` or `tools/harness` helper, a name assigned from those, a parameter that a call passes C text
  to, a parameter named after a function returning C text (a pytest fixture), a function that returns C text, or
  an expression built from those (slices, `.replace`, `+`, f-strings, `.join`).
  Not counted: needles held in containers or passed in as arguments, text reached through attributes (`self.text`)
  or through calls other than the ones above, C text a test generates itself (`ios_bridges.bridge(...)`), and
  literals that are only names or plain words.

These are text checks for honest authors, not a security boundary: the C-text tracking follows names by spelling
within each function, so it over-approximates, and a pattern assembled at run time defeats it.
"""
from __future__ import annotations

import ast
import re
from pathlib import Path

SEARCH_METHODS = {"index", "rindex", "find", "rfind", "split", "rsplit", "partition", "rpartition"}
RE_FUNCTIONS = {"search", "match", "fullmatch", "findall", "finditer", "split", "sub", "subn", "compile"}
DECLARATION_WORDS = {"static", "extern", "inline", "__inline", "typedef", "struct", "enum", "union", "const",
                     "volatile", "unsigned", "signed", "void", "int", "char", "short", "long", "float", "double",
                     "bool", "boolean", "BOOL", "size_t", "real", "NSString", "NSArray", "NSDictionary",
                     "instancetype"}
STATEMENT_WORDS = {"return", "if", "else", "while", "for", "switch", "case", "do", "goto", "sizeof", "defined"}
DECLARATION_SHAPE = re.compile(r"(?:[A-Za-z_]\w*[\s*]+)+[A-Za-z_]\w*\s*\(")
C_PATH = re.compile(r"\.(c|h|m|mm|metal|inc|cpp|cc|hpp)$")
SOURCE_MODULES = {"probekit.source", "source", "harness", "tools.harness"}
READS = {"read_text", "read_bytes", "read"}
ASSERT_LITERAL = re.compile(r"[(;=+\-*/<>&|!?%^~]")


def _group_end(text: str):
    """The index just past the group that opens at text[0], or None."""
    depth, index, in_class = 0, 0, False
    while index < len(text):
        character = text[index]
        if character == "\\":
            index += 2
            continue
        if in_class:
            in_class = character != "]"
        elif character == "[":
            in_class = True
        elif character == "(":
            depth += 1
        elif character == ")":
            depth -= 1
            if depth == 0:
                return index + 1
        index += 1
    return None


def _alternatives(text: str) -> list[str]:
    """A group's top-level alternatives."""
    parts, depth, start, index, in_class = [], 0, 0, 0, False
    while index < len(text):
        character = text[index]
        if character == "\\":
            index += 2
            continue
        if in_class:
            in_class = character != "]"
        elif character == "[":
            in_class = True
        elif character == "(":
            depth += 1
        elif character == ")":
            depth -= 1
        elif character == "|" and depth == 0:
            parts.append(text[start:index])
            start = index + 1
        index += 1
    return [*parts, text[start:]]


_LEADING_SPACE = re.compile(r"^(?:\\[nt]|\\s[*+?]?|\\b|\s)+")


def _readings(pattern: str, depth: int = 0) -> list[str]:
    """The ways a regular expression can start, as text: a leading group gives each of its alternatives, and
    nothing when it is optional."""
    text = _LEADING_SPACE.sub("", pattern)
    leading_class = re.match(r"\[(?:\\.|[^\]\\])*\][*?]\??", text)
    if depth <= 4 and leading_class:
        # an optional leading class (`[\\w ]*`): the pattern can start after it
        return [text] + _readings(text[leading_class.end():], depth + 1)
    if depth > 4 or not text.startswith("("):
        return [text]
    end = _group_end(text)
    if end is None:
        return [text]
    inner = re.sub(r"^\?(?::|P<\w+>|<\w+>|[=!])", "", text[1:end - 1])
    rest = text[end:]
    optional = bool(re.match(r"[?*]|\{0,", rest))
    rest = re.sub(r"^(?:[?*+]|\{\d*,?\d*\})\??", "", rest)
    readings = [reading for alternative in _alternatives(inner) for reading in _readings(alternative + rest, depth + 1)]
    return readings + (_readings(rest, depth + 1) if optional else [])


def _regex_text(pattern: str) -> str:
    """A regular expression's text as the text it matches, enough to see a declaration: whitespace escapes become a
    space, an escaped character becomes itself, and every metacharacter and class escape (`(`, `[`, `*`, `\\w`)
    becomes NUL, so a group is never read as a call's parenthesis."""
    out, index = [], 0
    while index < len(pattern):
        character = pattern[index]
        if character == "\\" and index + 1 < len(pattern):
            escaped = pattern[index + 1]
            index += 2
            if escaped == "s":
                out.append(" ")
                if index < len(pattern) and pattern[index] in "*+?":
                    index += 1
            elif escaped in "ntb":
                out.append(" ")
            else:
                out.append("\0" if escaped.isalnum() else escaped)
            continue
        out.append("\0" if character in "()[]{}|?*+^$." else character)
        index += 1
    return "".join(out)


def _is_declaration(text: str) -> bool:
    text = text.lstrip()
    if text.startswith(("#define", "@implementation", "@interface", "- (", "+ (", "-(", "+(")):
        return True
    word = re.match(r"[A-Za-z_]\w*", text)
    if not word or word.group() in STATEMENT_WORDS:
        return False
    return word.group() in DECLARATION_WORDS or bool(DECLARATION_SHAPE.match(text))


def starts_with_declaration(literal: str, regex: bool = False) -> bool:
    if not regex:
        return _is_declaration(literal)
    pattern = re.sub(r"^\(\?[a-zA-Z]+\)", "", literal).lstrip("^")
    return any(_is_declaration(_regex_text(reading)) for reading in _readings(pattern))


def leading_literal(node) -> str | None:
    """The literal text a pattern argument starts with."""
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    if isinstance(node, ast.JoinedStr) and node.values and isinstance(node.values[0], ast.Constant):
        return node.values[0].value
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
        return leading_literal(node.left)
    return None


def _re_names(tree) -> tuple[set[str], set[str]]:
    """Names bound to the `re` module, and names bound to its functions."""
    modules, functions = set(), set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            modules |= {alias.asname or alias.name for alias in node.names if alias.name == "re"}
        elif isinstance(node, ast.ImportFrom) and node.module == "re":
            functions |= {alias.asname or alias.name for alias in node.names if alias.name in RE_FUNCTIONS}
    return modules, functions


def _pattern(call, re_modules, re_functions):
    """(pattern node, whether it is a regular expression) for a search call, or None."""
    function = call.func
    pattern = call.args[0] if call.args else next((k.value for k in call.keywords if k.arg == "pattern"), None)
    if isinstance(function, ast.Attribute):
        if isinstance(function.value, ast.Name) and function.value.id in re_modules and function.attr in RE_FUNCTIONS:
            return pattern, True
        if function.attr in SEARCH_METHODS:
            if isinstance(function.value, ast.Name) and function.value.id == "str":
                return (call.args[1] if len(call.args) > 1 else None), False
            return (call.args[0] if call.args else None), False
    elif isinstance(function, ast.Name) and function.id in re_functions:
        return pattern, True
    return None


# an escaped brace, a brace in a class, or a bare brace that is not a `{m,n}` repeat
_REGEX_OPEN = re.compile(r"\\\{|\[[^\]]*\{[^\]]*\]|(?<!\\)\{(?!\d*,?\d*\})")
_REGEX_CLOSE = re.compile(r"\\\}|\[[^\]]*\}[^\]]*\]|(?<![\\\d,])\}")


def _brace_tests(function) -> set[str]:
    """The one-character brace strings a function compares against or counts (`c == "{"`, `text.count("}")`)."""
    found = set()
    for node in ast.walk(function):
        operands = []
        if isinstance(node, ast.Compare):
            operands = [node.left, *node.comparators]
        elif isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr in \
                ("count", "index", "find", "rfind", "rindex"):
            operands = node.args[:1]
        found |= {item.value for item in operands if isinstance(item, ast.Constant) and item.value in ("{", "}")}
    return found


def _ends_at_a_line_brace(function) -> bool:
    """Whether a function searches for a closing brace at the start of a line (`"\\n}\\n"`, `"\\n}"`), the end of
    a C function as its author formats it: a body found from a start the caller names."""
    for node in ast.walk(function):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and \
                node.func.attr in SEARCH_METHODS and node.args and isinstance(node.args[0], ast.Constant) and \
                isinstance(node.args[0].value, str) and "\n}" in node.args[0].value:
            return True
    return False


def _matches_braces(function, re_modules, re_functions) -> bool:
    if _brace_tests(function) == {"{", "}"} or _ends_at_a_line_brace(function):
        return True
    for node in ast.walk(function):
        if isinstance(node, ast.Call):
            found = _pattern(node, re_modules, re_functions)
            if found and found[1] and found[0] is not None:
                literal = "".join(item.value for item in ast.walk(found[0])
                                  if isinstance(item, ast.Constant) and isinstance(item.value, str))
                if _REGEX_OPEN.search(literal) and _REGEX_CLOSE.search(literal):
                    return True
    return False


def _parse(path):
    return ast.parse(Path(path).read_text(errors="replace"))


def _assigned_patterns(tree) -> dict[str, list]:
    """name -> the values assigned to it anywhere in the module, both branches of a conditional expression."""
    values: dict[str, list] = {}
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    stack = [node.value]
                    while stack:
                        value = stack.pop()
                        if isinstance(value, ast.IfExp):
                            stack += [value.body, value.orelse]
                        else:
                            values.setdefault(target.id, []).append(value)
    return values


def _declaration_searches(tree, re_modules, re_functions, assigned=None):
    assigned = _assigned_patterns(tree) if assigned is None else assigned
    for node in ast.walk(tree):
        if isinstance(node, ast.Call):
            found = _pattern(node, re_modules, re_functions)
            if found and found[0] is not None:
                candidates = assigned.get(found[0].id, []) if isinstance(found[0], ast.Name) else [found[0]]
                if any((literal := leading_literal(candidate)) is not None
                       and starts_with_declaration(literal, regex=found[1]) for candidate in candidates):
                    yield node


def extractor_findings(path) -> list[int]:
    tree = _parse(path)
    re_modules, re_functions = _re_names(tree)
    lines = {node.lineno for node in ast.walk(tree) if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef,
             ast.Lambda)) and _matches_braces(node, re_modules, re_functions)}
    lines |= {node.lineno for node in _declaration_searches(tree, re_modules, re_functions)}
    return sorted(lines)


def extractor_definitions(path) -> set[str]:
    """The module-level functions that match braces themselves or search for a declaration."""
    tree = _parse(path)
    re_modules, re_functions = _re_names(tree)
    assigned = _assigned_patterns(tree)
    return {node.name for node in tree.body if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
            and (_matches_braces(node, re_modules, re_functions)
                 or any(True for _ in _declaration_searches(node, re_modules, re_functions, assigned)))}


def imported_extractor_findings(path, extractors: dict[str, set[str]]) -> list[int]:
    """Lines that import, by name, a function `extractors` (module name -> its flagged functions) lists."""
    lines = []
    for node in ast.walk(_parse(path)):
        if isinstance(node, ast.ImportFrom) and node.module:
            module = node.module[len("tools."):] if node.module.startswith("tools.") else node.module
            lines += [node.lineno for alias in node.names if alias.name in extractors.get(module, ())]
    return lines


def _source_modules(tree) -> set[str]:
    names = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.ImportFrom):
            if node.module in ("probekit", "tools.probekit"):
                names |= {alias.asname or alias.name for alias in node.names if alias.name == "source"}
            elif node.module in ("tools", None) or node.module == "":
                names |= {alias.asname or alias.name for alias in node.names if alias.name == "harness"}
            elif node.module in SOURCE_MODULES:
                names |= {alias.asname or alias.name for alias in node.names}
        elif isinstance(node, ast.Import):
            names |= {(alias.asname or alias.name).split(".")[0] for alias in node.names
                      if alias.name in SOURCE_MODULES}
    return names


def _own_nodes(scope):
    """The nodes of a module or function, without those of the functions defined inside it."""
    stack = list(ast.iter_child_nodes(scope))
    while stack:
        node = stack.pop()
        yield node
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.Lambda)):
            stack.extend(ast.iter_child_nodes(node))


class _Taint:
    """Which names hold a C file's path or its text, per function (a function also sees the module's names), to a
    fixpoint. Text flows through names, slices, methods called on it (`.replace`, `.lower`), `+`, f-strings,
    `.join` and conditional expressions; not through other calls."""

    def __init__(self, tree):
        self.tree, self.modules = tree, _source_modules(tree)
        self.helpers = {alias.asname or alias.name for node in ast.walk(tree) if isinstance(node, ast.ImportFrom)
                        and node.module in SOURCE_MODULES for alias in node.names}
        self.functions = [node for node in ast.walk(tree) if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))]
        self.paths = {scope: set() for scope in [tree, *self.functions]}
        self.texts = {scope: set() for scope in [tree, *self.functions]}
        self.parents = {child: node for node in ast.walk(tree) for child in ast.iter_child_nodes(node)}
        self.returning, self.readers = set(), set()
        self.imported = {alias.asname or alias.name for node in ast.walk(tree) if isinstance(node, ast.ImportFrom)
                         for alias in node.names}
        self.by_name: dict[str, list] = {}
        for function in self.functions:
            self.by_name.setdefault(function.name, []).append(function)
        while True:
            before = self._size()
            for scope in self.paths:
                self._sweep(scope)
            if before == self._size():
                break

    def _size(self):
        return (sum(map(len, self.paths.values())), sum(map(len, self.texts.values())), len(self.returning),
                len(self.readers))

    def scope_of(self, node):
        while node in self.parents:
            node = self.parents[node]
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                return node
        return self.tree

    def _visible(self, table, scope):
        names, current = set(table[self.tree]), scope
        while current is not self.tree:
            names |= table[current]
            current = self.scope_of(current)
        return names

    def is_path(self, node, scope) -> bool:
        paths = self._visible(self.paths, scope)
        return any((isinstance(item, ast.Constant) and isinstance(item.value, str) and C_PATH.search(item.value))
                   or (isinstance(item, ast.Name) and item.id in paths) for item in ast.walk(node))

    def is_text(self, node, scope) -> bool:
        if isinstance(node, ast.Name):
            return node.id in self._visible(self.texts, scope)
        if isinstance(node, (ast.Subscript, ast.Starred)):
            return self.is_text(node.value, scope)
        if isinstance(node, ast.BinOp):
            return self.is_text(node.left, scope) or self.is_text(node.right, scope)
        if isinstance(node, ast.IfExp):
            return self.is_text(node.body, scope) or self.is_text(node.orelse, scope)
        if isinstance(node, ast.BoolOp):
            return any(self.is_text(value, scope) for value in node.values)
        if isinstance(node, ast.JoinedStr):
            return any(isinstance(value, ast.FormattedValue) and self.is_text(value.value, scope)
                       for value in node.values)
        if isinstance(node, (ast.List, ast.Tuple, ast.Set)):
            return any(self.is_text(item, scope) for item in node.elts)
        if isinstance(node, (ast.GeneratorExp, ast.ListComp, ast.SetComp)):
            return self.is_text(node.elt, scope)
        if not isinstance(node, ast.Call):
            return False
        function = node.func
        if isinstance(function, ast.Attribute):
            if isinstance(function.value, ast.Name) and function.value.id in self.modules:
                return True
            if function.attr in READS:
                return self.is_path(function.value, scope)
            if function.attr == "join":
                return any(self.is_text(argument, scope) for argument in node.args)
            return self.is_text(function.value, scope)
        if isinstance(function, ast.Name):
            if function.id in self.helpers or function.id in self.returning:
                return True
            if function.id in self.readers and any(self.is_path(argument, scope) for argument in node.args):
                return True
            # an extractor imported from another module, given C text (`function(source, "f(")`)
            if function.id in self.imported and any(self.is_text(argument, scope) for argument in node.args):
                return True
            if function.id == "open" and node.args:
                return self.is_path(node.args[0], scope)
        return False

    def _bind(self, scope, target, value):
        names = [item.id for item in ast.walk(target) if isinstance(item, ast.Name)]
        if self.is_text(value, scope):
            self.texts[scope].update(names)
        elif self.is_path(value, scope):
            self.paths[scope].update(names)

    def _sweep(self, scope):
        for node in _own_nodes(scope):
            if isinstance(node, ast.Assign):
                for target in node.targets:
                    self._bind(scope, target, node.value)
            elif isinstance(node, (ast.AnnAssign, ast.AugAssign, ast.NamedExpr)) and node.value is not None:
                self._bind(scope, node.target, node.value)
            elif isinstance(node, ast.For):
                self._bind(scope, node.target, node.iter)
            elif isinstance(node, ast.comprehension):
                self._bind(scope, node.target, node.iter)
            elif isinstance(node, ast.withitem) and node.optional_vars is not None:
                self._bind(scope, node.optional_vars, node.context_expr)
            elif isinstance(node, (ast.Return, ast.Yield)) and node.value is not None and scope is not self.tree:
                if self.is_text(node.value, scope):
                    self.returning.add(scope.name)
                elif self._reads_parameter(node.value, scope):
                    self.readers.add(scope.name)
            elif isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in self.by_name:
                self._pass_arguments(scope, node)
        if scope is not self.tree:
            # a parameter named after a function that returns C text: a pytest fixture
            self.texts[scope].update(name for name in _parameters(scope) if name in self.returning)

    def _reads_parameter(self, value, scope) -> bool:
        """Whether a returned value reads a file whose path is built from one of the function's parameters."""
        parameters = set(_parameters(scope))
        for item in ast.walk(value):
            if isinstance(item, ast.Call):
                function = item.func
                read = function.value if isinstance(function, ast.Attribute) and function.attr in READS else \
                    item.args[0] if isinstance(function, ast.Name) and function.id == "open" and item.args else None
                if read is not None and any(isinstance(name, ast.Name) and name.id in parameters
                                            for name in ast.walk(read)):
                    return True
        return False

    def _pass_arguments(self, scope, call):
        """A parameter holds C text when a call passes C text to it."""
        for callee in self.by_name[call.func.id]:
            parameters = _parameters(callee)
            for index, argument in enumerate(call.args):
                if index < len(parameters) and self.is_text(argument, scope):
                    self.texts[callee].add(parameters[index])
            for keyword in call.keywords:
                if keyword.arg in parameters and self.is_text(keyword.value, scope):
                    self.texts[callee].add(keyword.arg)


def _parameters(function) -> list[str]:
    arguments = function.args
    return [item.arg for item in [*arguments.posonlyargs, *arguments.args, *arguments.kwonlyargs]]


CHECK_METHODS = {"count", "find", "rfind", "index", "rindex"}
REACH_HELPERS = {"reach_contains", "assert_reaches"}


def _statement(literal) -> bool:
    return literal is not None and bool(ASSERT_LITERAL.search(literal))


def _bound_literals(scope_nodes, comprehensions=False) -> dict[str, list[str]]:
    """Names bound to string literals: assigned one, or a `for` (or, within one expression, a comprehension) over a
    tuple or list of them."""
    bound: dict[str, list[str]] = {}
    for node in scope_nodes:
        if isinstance(node, ast.Assign) and isinstance(node.value, ast.Constant) and isinstance(node.value.value, str):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    bound.setdefault(target.id, []).append(node.value.value)
        elif isinstance(node, (ast.For, *((ast.comprehension,) if comprehensions else ()))) \
                and isinstance(node.target, ast.Name) \
                and isinstance(node.iter, (ast.Tuple, ast.List, ast.Set)):
            bound.setdefault(node.target.id, []).extend(
                item.value for item in node.iter.elts if isinstance(item, ast.Constant) and isinstance(item.value, str))
    return bound


def _needles(node, bound) -> list[str]:
    literal = leading_literal(node)
    if literal is not None:
        return [literal]
    if isinstance(node, ast.Name):
        return bound.get(node.id, [])
    return []


def _checks(tree):
    """The expressions an `assert` tests, and the tests of `if` statements whose body raises."""
    for node in ast.walk(tree):
        if isinstance(node, ast.Assert):
            yield node, node.test
        elif isinstance(node, ast.If) and any(isinstance(statement, ast.Raise) for statement in node.body):
            yield node, node.test


def literal_assert_findings(path) -> list[int]:
    tree = _parse(path)
    taint = _Taint(tree)
    bound_by_scope = {scope: _bound_literals(_own_nodes(scope)) for scope in taint.paths}
    lines = []
    for statement, test in _checks(tree):
        scope = taint.scope_of(statement)
        bound = dict(bound_by_scope[taint.tree])
        bound.update(bound_by_scope[scope])
        bound.update(_bound_literals(ast.walk(test), comprehensions=True))
        for node in ast.walk(test):
            if isinstance(node, ast.Compare):
                operands = [node.left, *node.comparators]
                for left, operator, right in zip(operands, node.ops, operands[1:]):
                    if isinstance(operator, (ast.In, ast.NotIn)) and taint.is_text(right, scope):
                        lines += [node.lineno] * sum(map(_statement, _needles(left, bound)))
            elif isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.args \
                    and node.func.attr in CHECK_METHODS and taint.is_text(node.func.value, scope):
                lines += [node.lineno] * sum(map(_statement, _needles(node.args[0], bound)))
    for node in ast.walk(tree):
        if isinstance(node, ast.Call):
            name = node.func.attr if isinstance(node.func, ast.Attribute) else getattr(node.func, "id", None)
            if name in REACH_HELPERS:
                needle = node.args[2] if len(node.args) > 2 else next(
                    (keyword.value for keyword in node.keywords if keyword.arg == "needle"), None)
                literal = leading_literal(needle) if needle is not None else None
                if _statement(literal) and not re.fullmatch(r"[A-Za-z_]\w*\s*\(?", literal):
                    lines.append(node.lineno)
    return sorted(lines)
