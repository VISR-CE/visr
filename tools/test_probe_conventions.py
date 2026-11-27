#!/usr/bin/env python3
"""Probe conventions: new tests build probes through tools/probekit/cc.py, and every harness test is run by a workflow.

The class: about 170 test files each built their probes with their own compiler command, so a fix to one command
(`-lm`, a feature-test macro, a header Linux lacks) reached one file, and the same failure came back from the next.
`probekit.cc.build` is the one command. These lints keep the number of files with their own command from growing:

- Rule 1: a list or tuple whose first element is a compiler (`cc`, `clang`, `gcc`, `xcrun` + one of those, or a name
  assigned from `shutil.which`, `$CC` or a toolchain lookup of one) is a compiler command. Only files on
  `tools/probekit/legacy/compile.txt` (not migrated yet) or `exempt.txt` (tests of the build tools themselves, each
  with its reason) may have one.
- Rule H: every `tools/harness/tests/test_*.py` is named in `.github/workflows/ci.yml` or `cinematic-harness.yml`, or
  is on `tools/probekit/legacy/harness_not_run.txt`.

The legacy lists only shrink: each must be a subset of its record in `legacy/BASELINE.json`, every file on one must
still break its rule, and every file that breaks a rule must be on its list. A migrated file comes off its list; a new
file cannot go on one without editing the baseline, which the architecture review treats as a finding. These are text
checks for honest authors, not a security boundary: a compiler name hidden behind a function defeats them.
"""
import ast
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'tools'
LEGACY = TOOLS / 'probekit/legacy'
COMPILERS = {'cc', 'clang', 'gcc'}
LOOKUPS = ('which', 'environ', 'getenv', 'llvm_tool')


def constant(node):
    return node.value if isinstance(node, ast.Constant) and isinstance(node.value, str) else None


def compiler_variables(tree):
    """Names assigned from a lookup of a compiler: `shutil.which('clang')`, `os.environ.get('CC', 'cc')`, ..."""
    names = set()
    for node in ast.walk(tree):
        if not isinstance(node, (ast.Assign, ast.AnnAssign)) or node.value is None:
            continue
        strings = {constant(item) for item in ast.walk(node.value)}
        text = ast.unparse(node.value)
        if strings & (COMPILERS | {'CC'}) and any(lookup in text for lookup in LOOKUPS):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            names |= {target.id for target in targets if isinstance(target, ast.Name)}
    return names


def compiler_commands(path):
    """Line numbers of the compiler commands written in a Python file."""
    tree = ast.parse(path.read_text(errors='replace'))
    variables = compiler_variables(tree)
    lines = []
    for node in ast.walk(tree):
        if not isinstance(node, (ast.List, ast.Tuple)) or not node.elts:
            continue
        first, second = node.elts[0], node.elts[1] if len(node.elts) > 1 else None
        if (constant(first) in COMPILERS or (constant(first) == 'xcrun' and constant(second) in COMPILERS)
                or (isinstance(first, ast.Name) and first.id in variables)):
            lines.append(node.lineno)
    return sorted(set(lines))


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
    return set(json.loads((LEGACY / 'BASELINE.json').read_text())[name])


def check_shrink_only(name, violating):
    entries = set(listed(name))
    grown = sorted(entries - baseline(name))
    assert not grown, f'{name} is shrink-only; these are not in BASELINE.json: {grown}'
    fixed = sorted(entries - violating)
    assert not fixed, f'these no longer break the rule; take them off {name}: {fixed}'


def compile_violations():
    return {path.name: lines for path in linted_files() if (lines := compiler_commands(path))}


def test_rule_1_new_tests_build_probes_with_probekit_cc():
    violations = compile_violations()
    exempt = listed('exempt.txt')
    unexplained = sorted(name for name, reason in exempt.items() if not reason)
    assert not unexplained, f'exempt.txt needs a reason after # for: {unexplained}'
    stale = sorted(set(exempt) - set(violations))
    assert not stale, f'these no longer write a compiler command; take them off exempt.txt: {stale}'
    check_shrink_only('compile.txt', set(violations))
    allowed = set(listed('compile.txt')) | set(exempt)
    new = {name: lines for name, lines in violations.items() if name not in allowed}
    assert not new, ('these write their own compiler command; build the probe with probekit.cc.build instead '
                     f'(file: lines): {new}')


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
    probe.write_text('import os, shutil, subprocess\n'
                     'subprocess.run(["clang", "a.c"])\n'
                     'subprocess.run(("xcrun", "clang", "a.c"))\n'
                     'compiler = shutil.which("cc") or shutil.which("clang")\n'
                     'subprocess.run([compiler, "a.c"])\n'
                     'CC = os.environ.get("CC", "cc")\n'
                     'command = [CC, "-o", "x", "a.c"]\n'
                     'subprocess.run(["python3", "clang"])\n'
                     'subprocess.run(["xcrun", "nm", "x"])\n')
    assert compiler_commands(probe) == [2, 3, 5, 7]
