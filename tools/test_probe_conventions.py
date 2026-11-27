#!/usr/bin/env python3
"""Probe conventions: new tests build probes through tools/probekit/cc.py, and every harness test is run by a workflow.

The class: about 170 test files each built their probes with their own compiler command, so a fix to one command
(`-lm`, a feature-test macro, a header Linux lacks) reached one file, and the same failure came back from the next.
`probekit.cc.build` is the one command. These lints keep the number of files with their own command from growing:

- Rule 1: a compiler command, in any of the forms `tools/probekit/commands.py` describes (a list whose first element
  is a compiler however it is spelled or looked up, `shlex.split` of one, a `shell=True` string), may appear only in
  files on
  `tools/probekit/legacy/compile.txt` (not migrated yet) or `exempt.txt` (tests of the build tools themselves, each
  with its reason) may have one. Both lists are shrink-only.
  Each listed file's count of commands is recorded too, and may only fall.
- Rule 2: calls that pass `flags=` (a caller's own options in place of build()'s defaults) are counted per file in
  `BASELINE.json`, and a count may only fall; new code uses build()'s structured parameters.
- Rule H: every `tools/harness/tests/test_*.py` is named in `.github/workflows/ci.yml` or `cinematic-harness.yml`, or
  is on `tools/probekit/legacy/harness_not_run.txt`.

The legacy lists only shrink: each must be a subset of its record in `legacy/BASELINE.json`, every file on one must
still break its rule, and every file that breaks a rule must be on its list. A migrated file comes off its list; a new
file cannot go on one without editing the baseline, which the architecture review treats as a finding. These are text
checks for honest authors, not a security boundary: a compiler name hidden behind a function defeats them.
"""
import ast
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probekit.commands import written_commands_in

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'tools'
LEGACY = TOOLS / 'probekit/legacy'


def compiler_commands(path):
    """Line numbers of the compiler commands written in a Python file (tools/probekit/commands.py)."""
    return written_commands_in(path)


def linted_files():
    return sorted([*TOOLS.glob('test_*.py'), TOOLS / 'ios_test.py'])


def listed(name):
    """A legacy list's file names; text after `#` is a comment (exempt.txt's reasons)."""
    entries = {}
    for line in (LEGACY / name).read_text().splitlines():
        entry, _, reason = line.partition('#')
        if entry.strip():
            entries[entry.strip()] = reason.strip()
    return entries


def baseline(name):
    """A legacy list's record: file -> number of commands for rule 1's lists, a list of names for rule H's."""
    return json.loads((LEGACY / 'BASELINE.json').read_text())[name]


def check_shrink_only(name, violating):
    """`violating` holds what breaks the rule now: file -> count of commands, or a set of names."""
    entries = set(listed(name))
    recorded = baseline(name)
    grown = sorted(entries - set(recorded))
    assert not grown, f'{name} is shrink-only; these are not in BASELINE.json: {grown}'
    fixed = sorted(entries - set(violating))
    assert not fixed, f'these no longer break the rule; take them off {name}: {fixed}'
    if isinstance(recorded, dict):
        more = {entry: f'{recorded[entry]} -> {violating[entry]}' for entry in sorted(entries)
                if violating[entry] > recorded[entry]}
        assert not more, (f'these files on {name} write more compiler commands than BASELINE.json records; build the '
                          f'new probe with probekit.cc.build: {more}')


def compile_violations():
    return {path.name: lines for path in linted_files() if (lines := compiler_commands(path))}


def test_rule_1_new_tests_build_probes_with_probekit_cc():
    violations = compile_violations()
    exempt = listed('exempt.txt')
    unexplained = sorted(name for name, reason in exempt.items() if not reason)
    assert not unexplained, f'exempt.txt needs a reason after # for: {unexplained}'
    counts = {name: len(lines) for name, lines in violations.items()}
    check_shrink_only('exempt.txt', counts)
    check_shrink_only('compile.txt', counts)
    allowed = set(listed('compile.txt')) | set(exempt)
    new = {name: lines for name, lines in violations.items() if name not in allowed}
    assert not new, ('these write their own compiler command; build the probe with probekit.cc.build instead '
                     f'(file: lines): {new}')


def flags_calls(path):
    """probe(..., flags=...) and probekit.cc build(..., flags=...) / command(..., flags=...) calls in a file."""
    tree = ast.parse(path.read_text(errors='replace'))
    imported = {alias.asname or alias.name for node in ast.walk(tree) if isinstance(node, ast.ImportFrom)
                and node.module in ('probekit.cc', 'cc') for alias in node.names}
    def raw(call):
        function = call.func
        return (isinstance(function, ast.Name) and (function.id == 'probe' or function.id in imported)) or \
            (isinstance(function, ast.Attribute) and function.attr in ('build', 'command')
             and isinstance(function.value, ast.Name) and function.value.id == 'cc')
    return sum(1 for node in ast.walk(tree) if isinstance(node, ast.Call) and raw(node)
               and any(keyword.arg == 'flags' for keyword in node.keywords))


def test_rule_2_raw_flags_only_shrink():
    """`flags=` keeps a caller's own options in place of build()'s defaults, which only moves the duplication; the
    runners that predate build() use it, and a new caller uses the structured parameters instead."""
    counts = {str(path.relative_to(ROOT)): count for path in sorted(TOOLS.rglob('*.py'))
              if (count := flags_calls(path))}
    recorded = baseline('flags')
    more = {path: f'{recorded.get(path, 0)} -> {count}' for path, count in counts.items()
            if count > recorded.get(path, 0)}
    assert not more, ('these build probes with flags= more often than BASELINE.json records; use build()\'s '
                      f'structured parameters (std, includes, defines, sanitize, ...): {more}')


def harness_tests():
    return sorted(f'tools/harness/tests/{path.name}' for path in (TOOLS / 'harness/tests').glob('test_*.py'))


def test_rule_h_every_harness_test_is_run_by_a_workflow():
    workflows = ''.join((ROOT / '.github/workflows' / name).read_text() for name in ('ci.yml', 'cinematic-harness.yml'))
    unrun = {test for test in harness_tests() if test not in workflows and Path(test).name not in workflows}
    check_shrink_only('harness_not_run.txt', unrun)
    new = sorted(unrun - set(listed('harness_not_run.txt')))
    assert not new, f'no workflow runs these harness tests; name them in ci.yml or cinematic-harness.yml: {new}'


def test_the_lint_sees_each_form_of_compiler_command(tmp_path):
    probe = tmp_path / 'test_example.py'
    probe.write_text('import os, shlex, shutil, subprocess\n'
                     'subprocess.run(["clang", "a.c"])\n'                                # 2
                     'subprocess.run(("xcrun", "clang", "a.c"))\n'                       # 3
                     'compiler = shutil.which("cc") or shutil.which("clang")\n'
                     'subprocess.run([compiler, "a.c"])\n'                               # 5
                     'CC = os.environ.get("CC", "cc")\n'
                     'command = [CC, "-o", "x", "a.c"]\n'                                # 7
                     'subprocess.run(["python3", "clang"])\n'
                     'subprocess.run(["xcrun", "nm", "x"])\n'
                     'subprocess.run([shutil.which("clang") or "cc", "a.c"])\n'          # 10
                     'def find_clang():\n'
                     '    return shutil.which("clang")\n'
                     'tool = find_clang()\n'
                     'subprocess.run([tool, "a.c"])\n'                                   # 14
                     'subprocess.run(["/usr/bin/clang", "a.c"])\n'                       # 15
                     'subprocess.run(["xcrun", "--sdk", "macosx", "clang", "a.c"])\n'    # 16
                     'subprocess.run([str(tool), "a.c"])\n'                              # 17
                     'class Builder:\n'
                     '    def go(self):\n'
                     '        subprocess.run([self.cc, "a.c"])\n'                        # 20
                     'def build(compiler):\n'
                     '    subprocess.run([compiler, "a.c"])\n'                           # 22
                     'subprocess.run(shlex.split("clang -O2 a.c"))\n'                    # 23
                     'subprocess.run("clang++ a.cc -o a", shell=True)\n'                 # 24
                     'for candidate in (shutil.which("clang"), "cc"):\n'
                     '    pass\n'
                     'clang, ninja = shutil.which("clang"), shutil.which("ninja")\n')
    assert compiler_commands(probe) == [2, 3, 5, 7, 10, 14, 15, 16, 17, 20, 22, 23, 24]
