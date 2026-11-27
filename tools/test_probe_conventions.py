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
- Rule 3a: tests find production C and Objective-C source through `tools/probekit/source.py`: a private extractor
  (a function that matches braces itself) or a search whose literal pattern starts with a C declaration
  (`text.index("static void f(")`, `re.search(r"^enum kind")`), as `tools/probekit/source_lint.py` describes, may
  appear only in files on `tools/probekit/legacy/source.txt`, each with its count of them in `BASELINE.json`.
- Rule 3b: an assert that a literal statement is somewhere in C text (`assert "f(x);" in text`) is counted per file
  in `tools/probekit/legacy/literal_asserts.json`; a count may only fall and a file not in it may have none. These
  break on a refactor that keeps the behavior, and are replaced by behavior probes as they break.

Rules 3a and 3b lint `tools/test_*.py`, `tools/ios_test.py` and `tools/harness/`; `legacy/source_exempt.txt` names
the tests of the detector and of `probekit.source` themselves, with reasons.

The legacy lists only shrink: each must equal its record in `legacy/BASELINE.json`, every file on one must still
break its rule, and every file that breaks a rule must be on its list. A migrated file comes off its list; a new
file cannot go on one without editing the baseline, which the architecture review treats as a finding. These are text
checks for honest authors, not a security boundary: a compiler name hidden behind a function defeats them.
"""
import ast
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probekit.commands import written_commands_in
from probekit.source_lint import extractor_findings, literal_assert_findings

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


def shrink_only_problems(name, entries, recorded, violating, more_hint='write more compiler commands than '
                         'BASELINE.json records; build the new probe with probekit.cc.build'):
    """What is wrong with a legacy list: `entries` are its files, `recorded` its BASELINE.json record (file -> count,
    or a list of names) and `violating` what breaks the rule now (file -> count, or a set of names)."""
    entries = set(entries)
    problems = []
    grown = sorted(entries - set(recorded))
    if grown:
        problems.append(f'{name} is shrink-only; these are not in BASELINE.json: {grown}')
    fixed = sorted(entries - set(violating))
    if fixed:
        problems.append(f'these no longer break the rule; take them off {name}: {fixed}')
    removed = sorted(set(recorded) - entries)
    if removed:
        problems.append(f'these came off {name}; take them out of BASELINE.json too, so that putting one back shows '
                        f'as a baseline addition: {removed}')
    if isinstance(recorded, dict):
        more = {entry: f'{recorded[entry]} -> {violating[entry]}' for entry in sorted(entries & set(recorded))
                if entry in violating and violating[entry] > recorded[entry]}
        if more:
            problems.append(f'these files on {name} {more_hint}: {more}')
    return problems


def check_shrink_only(name, violating, **hint):
    problems = shrink_only_problems(name, listed(name), baseline(name), violating, **hint)
    assert not problems, '\n'.join(problems)


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


def source_linted_files():
    """Rules 3a and 3b's files, by their path under tools/."""
    files = [*linted_files(), *sorted((TOOLS / 'harness').rglob('*.py'))]
    return {str(path.relative_to(TOOLS)): path for path in files}


def source_exempt():
    exempt = listed('source_exempt.txt')
    unexplained = sorted(name for name, reason in exempt.items() if not reason)
    assert not unexplained, f'source_exempt.txt needs a reason after # for: {unexplained}'
    return exempt


def test_rule_3a_tests_find_source_through_probekit_source():
    exempt = source_exempt()
    found = {name: len(lines) for name, path in source_linted_files().items() if (lines := extractor_findings(path))}
    check_shrink_only('source.txt', {name: count for name, count in found.items() if name not in exempt},
                      more_hint='find source by declaration text more often than BASELINE.json records; use '
                      'probekit.source.function, body, structure, region or reach instead')
    allowed = set(listed('source.txt')) | set(exempt)
    new = {name: extractor_findings(path) for name, path in source_linted_files().items()
           if name not in allowed and name in found}
    assert not new, ('these find production source by its declaration text; use probekit.source (function, body, '
                     f'structure, region, reach_contains) instead (file: lines): {new}')


def test_rule_3b_literal_statement_asserts_only_fall():
    exempt = source_exempt()
    counts = {name: len(lines) for name, path in source_linted_files().items()
              if name not in exempt and (lines := literal_assert_findings(path))}
    table = json.loads((LEGACY / 'literal_asserts.json').read_text())
    problems = shrink_only_problems('literal_asserts.json', table, table, counts,
                                    more_hint='assert more literal statements than the table records; check the '
                                    'behavior with a probe, or the call with probekit.source.reach_contains')
    new = {name: count for name, count in counts.items() if name not in table}
    if new:
        problems.append(f'these assert literal statements in C text; check the behavior with a probe, or the call '
                        f'with probekit.source.reach_contains (file: count): {new}')
    assert not problems, '\n'.join(problems)


def test_rule_3b_table_matches_todays_counts_exactly():
    """A count that fell is lowered in the table, so the cap does not leave room to grow back."""
    exempt = source_exempt()
    counts = {name: len(lines) for name, path in source_linted_files().items()
              if name not in exempt and (lines := literal_assert_findings(path))}
    table = json.loads((LEGACY / 'literal_asserts.json').read_text())
    lower = {name: f'{table[name]} -> {counts[name]}' for name in table if name in counts and counts[name] < table[name]}
    assert not lower, f'lower these counts in literal_asserts.json: {lower}'


def test_the_source_lint_sees_each_form_of_declaration_search(tmp_path):
    probe = tmp_path / 'test_example.py'
    probe.write_text('import re\n'
                     'import re as regex\n'
                     'from re import findall as every\n'
                     'def block(text, marker):\n'                                       # 4
                     '    return text.count("{") - text.count("}")\n'
                     'text = open("x.c").read()\n'
                     'text.index("static void present(")\n'                             # 7
                     'text.rindex("struct pair\\n")\n'                                  # 8
                     'text.find("boolean ok(void)")\n'                                  # 9
                     'text.rfind("#define LIMIT")\n'                                    # 10
                     'text.split("enum kind")\n'                                        # 11
                     'text.rsplit("const char *name")\n'                                # 12
                     'text.partition("- (void)drawFrame")\n'                            # 13
                     'text.rpartition("unsigned long table[")\n'                        # 14
                     'str.index(text, "void f(")\n'                                     # 15
                     're.search(r"^static\\s+int\\s+f\\(", text)\n'                       # 16
                     'regex.findall(r"enum\\s*\\{[^}]*\\};", text)\n'                       # 17
                     'every(r"struct\\s+(\\w+)", text)\n'                                  # 18
                     'text.index("int host_frame_present(int on)" + "")\n'                  # 19
                     'name = "f"\n'
                     'text.index(f"static int {name}(")\n'                              # 21
                     'text.index("\\treturn value;")\n'
                     'text.index("if (ready)")\n'
                     'text.index("x = f(y);")\n'
                     'text.split()\n'
                     'text.split("\\n")\n'
                     're.findall(r\'"([^"]*)"\', text)\n'
                     're.search(r"case (P2P_[A-Z_]+):", text)\n')
    assert extractor_findings(probe) == [4, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21]


def test_the_source_lint_sees_literal_statement_asserts_on_c_text_only(tmp_path):
    probe = tmp_path / 'test_example.py'
    probe.write_text('import subprocess\n'
                     'from pathlib import Path\n'
                     'from probekit import source\n'
                     'ROOT = Path(".")\n'
                     'HEADER = ROOT / "port/x.h"\n'
                     'MODULE_TEXT = HEADER.read_text()\n'
                     'def kernel(name):\n'
                     '    return source.function("port/k.c", name)\n'
                     'def test_a():\n'
                     '    text = (ROOT / "port/a.m").read_text()\n'
                     '    assert "f(x);" in text\n'                                     # 11
                     '    assert "g()" not in text[10:]\n'                              # 12
                     '    assert "plain words" in text\n'
                     '    assert "a == b" in MODULE_TEXT\n'                             # 14
                     '    assert "h(1)" in kernel("k")\n'                               # 15
                     '    assert "i(2);" in source.body("port/b.c", "b")\n'             # 16
                     '    joined = "\\n".join(source.function("port/c.c", n) for n in ("c",))\n'
                     '    assert "j();" in joined.replace("a", "b")\n'                   # 18
                     'def test_b():\n'
                     '    result = subprocess.run(["tool", "port/a.c"], capture_output=True, text=True)\n'
                     '    assert "error: f(" in result.stderr\n'
                     '    text = "made here"\n'
                     '    assert "f(x);" in text\n'
                     '    assert "-flag" in " ".join(["-flag"])\n')
    assert literal_assert_findings(probe) == [11, 12, 14, 15, 16, 18]


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


def test_a_shrink_only_list_refuses_growth_stale_entries_and_more_commands():
    recorded = {'test_a.py': 2, 'test_b.py': 1}
    assert shrink_only_problems('compile.txt', {'test_a.py', 'test_b.py'}, recorded, {'test_a.py': 2, 'test_b.py': 1}) == []
    assert shrink_only_problems('compile.txt', {'test_a.py', 'test_b.py'}, recorded, {'test_a.py': 1, 'test_b.py': 1}) == []
    grown = shrink_only_problems('compile.txt', {'test_a.py', 'test_b.py', 'test_c.py'}, recorded,
                                 {'test_a.py': 2, 'test_b.py': 1, 'test_c.py': 1})
    assert len(grown) == 1 and 'not in BASELINE.json' in grown[0] and 'test_c.py' in grown[0]
    stale = shrink_only_problems('compile.txt', {'test_a.py', 'test_b.py'}, recorded, {'test_a.py': 2})
    assert len(stale) == 1 and 'no longer break the rule' in stale[0] and 'test_b.py' in stale[0]
    removed = shrink_only_problems('compile.txt', {'test_a.py'}, recorded, {'test_a.py': 2})
    assert len(removed) == 1 and 'take them out of BASELINE.json' in removed[0]
    more = shrink_only_problems('compile.txt', {'test_a.py', 'test_b.py'}, recorded, {'test_a.py': 3, 'test_b.py': 1})
    assert len(more) == 1 and '2 -> 3' in more[0]
    harness = shrink_only_problems('harness_not_run.txt', {'tools/harness/tests/test_x.py'},
                                   ['tools/harness/tests/test_x.py'], {'tools/harness/tests/test_x.py'})
    assert harness == []
