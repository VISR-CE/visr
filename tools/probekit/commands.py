"""The one detector of C compiler commands, for a command being run and for one written in a Python file.

`compiler_link(args)` says whether an argument list runs a C or C++ compiler that links (tools/ios_test.py's run()
refuses those, because every probe link goes through probe()). `written_commands(path)` finds the compiler commands
a Python file writes, for tools/test_probe_conventions.py's rule 1 and tools/test_probe_ledger.py's static check.

A compiler is a program whose file name is `cc`, `gcc`, `clang`, `c++`, `g++` or `clang++`, with or without a
version suffix (`clang-18`), run directly or through `xcrun` (`xcrun clang`, `xcrun --sdk macosx clang`). In Python
source, a command is any of:

- a list or tuple whose first element (after `xcrun` and its options) is a compiler: a string naming one, a name
  or attribute bound to one, `str()` of one, a parameter named like one, a lookup (`shutil.which("clang")`,
  `os.environ.get("CC")`, `os.environ["CC"]`, a local function returning one), or an `or`/`if` expression with one;
- `shlex.split(...)` of a string whose first word is a compiler;
- a string run with `shell=True` whose first word is a compiler.

These are text checks for honest authors, not a security boundary.
"""
from __future__ import annotations

import ast
import re
from pathlib import Path

COMPILER_NAME = re.compile(r"^(cc|gcc|clang|c\+\+|g\+\+|clang\+\+)(-[0-9.]+)?$")
NOT_LINKING = ("-c", "-S", "-E", "-M", "-MM", "-fsyntax-only")
# xcrun's own options, each with whether it takes a value
XCRUN_OPTIONS = {"--sdk": True, "-sdk": True, "--toolchain": True, "-toolchain": True, "--run": False, "-r": False,
                 "--log": False, "-l": False, "--verbose": False, "-v": False, "--no-cache": False, "-n": False}
# names and attributes that hold a compiler when nothing else says so (a parameter, self.cc)
COMPILER_VARIABLES = {"cc", "CC", "clang", "CLANG", "gcc", "compiler", "COMPILER", "clang_path", "cc_path"}
LOOKUP_FUNCTIONS = {"which", "get", "getenv", "llvm_tool", "find_program"}


def is_compiler(program) -> bool:
    return bool(COMPILER_NAME.match(Path(str(program)).name))


def compiler_index(args) -> int | None:
    """The index of the compiler in an argument list, or None when the list does not run one."""
    args = [str(arg) for arg in args]
    index = 0
    if args and Path(args[0]).name == "xcrun":
        index = 1
        while index < len(args) and args[index] in XCRUN_OPTIONS:
            index += 2 if XCRUN_OPTIONS[args[index]] else 1
    return index if index < len(args) and is_compiler(args[index]) else None


def compiler_link(args) -> bool:
    """A compiler command that links (no -c, -S, -E, -M, -MM or -fsyntax-only)."""
    args = [str(arg) for arg in args]
    return compiler_index(args) is not None and not any(arg in NOT_LINKING for arg in args)


def _first_word(text: str) -> str | None:
    words = text.split()
    if not words:
        return None
    if Path(words[0]).name == "xcrun":
        index = 1
        while index < len(words) and words[index] in XCRUN_OPTIONS:
            index += 2 if XCRUN_OPTIONS[words[index]] else 1
        return words[index] if index < len(words) else None
    return words[0]


def _string_start(node) -> str | None:
    """The literal text a string expression starts with: a constant, an f-string's first part, `"..." + x`."""
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    if isinstance(node, ast.JoinedStr) and node.values and isinstance(node.values[0], ast.Constant):
        return node.values[0].value
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
        return _string_start(node.left)
    return None


class _Taint:
    """The names, attributes and local functions of one module that hold or return a compiler."""

    def __init__(self, tree):
        self.names = set(COMPILER_VARIABLES)
        self.functions = set()
        functions = [node for node in ast.walk(tree) if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))]
        assignments = [node for node in ast.walk(tree) if isinstance(node, (ast.Assign, ast.AnnAssign, ast.NamedExpr))]
        for node in functions:
            for argument in [*node.args.args, *node.args.kwonlyargs, *node.args.posonlyargs]:
                if argument.arg in COMPILER_VARIABLES:
                    self.names.add(argument.arg)
        changed = True
        while changed:
            changed = False
            for node in assignments:
                value = node.value
                if value is None or not self.compiler(value):
                    continue
                targets = node.targets if isinstance(node, ast.Assign) else [node.target]
                for target in targets:
                    name = target.id if isinstance(target, ast.Name) else \
                        target.attr if isinstance(target, ast.Attribute) else None
                    if name and name not in self.names:
                        self.names.add(name)
                        changed = True
            for node in functions:
                if node.name in self.functions:
                    continue
                if any(isinstance(item, ast.Return) and item.value is not None and self.compiler(item.value)
                       for item in ast.walk(node)):
                    self.functions.add(node.name)
                    changed = True

    def compiler(self, node) -> bool:
        """An expression that evaluates to a compiler program."""
        if isinstance(node, ast.Constant) and isinstance(node.value, str):
            return is_compiler(node.value)
        if isinstance(node, ast.Name):
            return node.id in self.names
        if isinstance(node, ast.Attribute):
            return node.attr in self.names
        if isinstance(node, ast.BoolOp):
            return any(self.compiler(value) for value in node.values)
        if isinstance(node, ast.IfExp):
            return self.compiler(node.body) or self.compiler(node.orelse)
        if isinstance(node, ast.Subscript):
            key = node.slice
            return isinstance(key, ast.Constant) and key.value in ("CC", "CXX")
        if isinstance(node, ast.JoinedStr) or isinstance(node, ast.BinOp):
            start = _string_start(node)
            return start is not None and is_compiler(start.split()[0] if start.split() else "")
        if isinstance(node, ast.Call):
            function = node.func.attr if isinstance(node.func, ast.Attribute) else \
                node.func.id if isinstance(node.func, ast.Name) else None
            if function in ("str", "Path", "fspath") and node.args:
                return self.compiler(node.args[0])
            if function in self.functions:
                return True
            if function in LOOKUP_FUNCTIONS or (function or "").startswith("find_"):
                return any(self.compiler(argument) or (isinstance(argument, ast.Constant) and argument.value in
                                                       ("CC", "CXX")) for argument in node.args)
        return False

    def command(self, node) -> bool:
        """A list or tuple written as a compiler command."""
        elements = node.elts
        if not elements or isinstance(elements[0], ast.Starred):
            return False
        index = 0
        if isinstance(elements[0], ast.Constant) and isinstance(elements[0].value, str) \
                and Path(elements[0].value).name == "xcrun":
            index = 1
            while index < len(elements) and isinstance(elements[index], ast.Constant) \
                    and elements[index].value in XCRUN_OPTIONS:
                index += 2 if XCRUN_OPTIONS[elements[index].value] else 1
            if index >= len(elements):
                return False
        return self.compiler(elements[index])


def written_commands(source: str) -> list[int]:
    """Line numbers of the compiler commands a Python source writes."""
    tree = ast.parse(source)
    taint = _Taint(tree)
    lines = set()
    # a tuple of candidates (`for compiler in (which("clang"), ...)`) or of unpacked values is not a command
    candidates = {id(node.iter) for node in ast.walk(tree) if isinstance(node, (ast.For, ast.comprehension))}
    candidates |= {id(node.value) for node in ast.walk(tree) if isinstance(node, ast.Assign)
                   and isinstance(node.targets[0], ast.Tuple)}
    for node in ast.walk(tree):
        if isinstance(node, (ast.List, ast.Tuple)) and isinstance(node.ctx, ast.Load) and id(node) not in candidates \
                and taint.command(node):
            lines.add(node.lineno)
        elif isinstance(node, ast.Call):
            function = node.func.attr if isinstance(node.func, ast.Attribute) else \
                node.func.id if isinstance(node.func, ast.Name) else None
            if function == "split" and node.args:
                start = _string_start(node.args[0])
                if start is not None and _first_word(start) and is_compiler(_first_word(start)):
                    lines.add(node.lineno)
            shell = any(keyword.arg == "shell" and getattr(keyword.value, "value", False) is True
                        for keyword in node.keywords)
            if shell and node.args:
                start = _string_start(node.args[0])
                if start is not None and _first_word(start) and is_compiler(_first_word(start)):
                    lines.add(node.lineno)
    return sorted(lines)


def written_commands_in(path: Path) -> list[int]:
    return written_commands(Path(path).read_text(errors="replace"))
