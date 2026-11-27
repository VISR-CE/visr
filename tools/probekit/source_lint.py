"""The one detector of tests that find production source by its text, for tools/test_probe_conventions.py's
rules 3a and 3b.

- `extractor_findings(path)`: rule 3a. Line numbers of the places a Python file finds C or Objective-C source by
  its declaration text instead of through `probekit.source`:
  - a private extractor: a function that matches braces itself (it compares against both `"{"` and `"}"`);
  - a search whose literal pattern starts with a C declaration: `.index`, `.rindex`, `.find`, `.rfind`, `.split`,
    `.rsplit`, `.partition` or `.rpartition` (also called as `str.index(text, ...)`), or any `re` function
    (`re.search(...)`, `import re as x`, `from re import search`). The pattern is the literal at the start of the
    argument: a string, the first part of an f-string, or the left of a `+`.
  A declaration starts with a storage class, qualifier or type keyword (`static`, `struct`, `enum`, `void`,
  `boolean`, ...), `#define`, `typedef`, an Objective-C method (`- (`, `+ (`) or `@implementation`, or has the
  shape `type name(`. Regular-expression escapes (`\\s*`, `\\(`, `\\*`, a leading `^`) are read as the text they
  match.
- `literal_assert_findings(path)`: rule 3b. Line numbers of asserts of the form `"<literal>" in <C text>` (or
  `not in`), where the literal holds `(`, `;` or an operator, and the C text is read from a C, Objective-C, Metal
  or include file: `.read_text()`/`.read_bytes()`/`.read()`/`open()` of a path naming one, any `probekit.source`
  or `tools/harness` helper, a name assigned from either, a function that returns either, or any expression
  built from those (slices, `.replace`, `.join`).

These are text checks for honest authors, not a security boundary: the C-text tracking follows names by spelling,
not by scope, so it over-approximates.
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
                     "instancetype", "id"}
STATEMENT_WORDS = {"return", "if", "else", "while", "for", "switch", "case", "do", "goto", "sizeof", "defined"}
DECLARATION_SHAPE = re.compile(r"(?:[A-Za-z_]\w*[\s*]+)+[A-Za-z_]\w*\s*\(")
C_PATH = re.compile(r"\.(c|h|m|mm|metal|inc|cpp|cc|hpp)$")
SOURCE_MODULES = {"probekit.source", "source", "harness", "tools.harness"}
READS = {"read_text", "read_bytes", "read"}
ASSERT_LITERAL = re.compile(r"[(;=+\-*/<>&|!?%^~]")


def _regex_text(pattern: str) -> str:
    """A regular expression's leading text as the text it matches, enough to see a declaration."""
    text = re.sub(r"^\(\?[a-zA-Z]+\)", "", pattern)
    text = text.lstrip("^")
    text = re.sub(r"\\s[*+?]?|\\[nt]|\\b", " ", text)
    return re.sub(r"\\([(){}*\[\].+?|#-])", r"\1", text)


def starts_with_declaration(literal: str, regex: bool = False) -> bool:
    text = (_regex_text(literal) if regex else literal).lstrip()
    if text.startswith(("#define", "@implementation", "@interface", "- (", "+ (", "-(", "+(")):
        return True
    word = re.match(r"[A-Za-z_]\w*", text)
    if not word or word.group() in STATEMENT_WORDS:
        return False
    return word.group() in DECLARATION_WORDS or bool(DECLARATION_SHAPE.match(text))


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


def _matches_braces(function) -> bool:
    strings = {node.value for node in ast.walk(function) if isinstance(node, ast.Constant)}
    return "{" in strings and "}" in strings


def extractor_findings(path) -> list[int]:
    tree = ast.parse(Path(path).read_text(errors="replace"))
    re_modules, re_functions = _re_names(tree)
    lines = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.Lambda)) and _matches_braces(node):
            lines.add(node.lineno)
        elif isinstance(node, ast.Call):
            found = _pattern(node, re_modules, re_functions)
            if found and found[0] is not None:
                literal = leading_literal(found[0])
                if literal is not None and starts_with_declaration(literal, regex=found[1]):
                    lines.add(node.lineno)
    return sorted(lines)


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
        self.returning = set()
        while True:
            before = (sum(map(len, self.paths.values())), sum(map(len, self.texts.values())), len(self.returning))
            for scope in self.paths:
                self._sweep(scope)
            if before == (sum(map(len, self.paths.values())), sum(map(len, self.texts.values())), len(self.returning)):
                break

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
            elif isinstance(node, ast.Return) and node.value is not None and scope is not self.tree:
                if self.is_text(node.value, scope):
                    self.returning.add(scope.name)


def literal_assert_findings(path) -> list[int]:
    tree = ast.parse(Path(path).read_text(errors="replace"))
    taint = _Taint(tree)
    lines = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assert):
            continue
        scope = taint.scope_of(node)
        for compare in ast.walk(node.test):
            if not isinstance(compare, ast.Compare):
                continue
            operands = [compare.left, *compare.comparators]
            for left, operator, right in zip(operands, compare.ops, operands[1:]):
                literal = leading_literal(left)
                if (isinstance(operator, (ast.In, ast.NotIn)) and literal is not None
                        and ASSERT_LITERAL.search(literal) and taint.is_text(right, scope)):
                    lines.append(compare.lineno)
    return sorted(lines)
