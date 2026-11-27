#!/usr/bin/env python3
"""Every native probe is built by a runner something invokes, and every probe `tools/ios_test.py` links also runs.

The class: a probe whose checks never execute passes silently. Seven probe sources in `port/ios/tests` were built by
nothing, and `ios_test.py` linked `recording-pack-probe` without ever running it. The link rule these checks apply
is `tools/probe_ledger.py`'s, the same one `ios_test.py` uses at run time.
"""
import ast
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ci_test
from probe_ledger import ProbeLedger, compiles_probe, links_probe, output_of, refuse_inline_link
from probekit.commands import compiler_link

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'tools'
IOS_TEST = TOOLS / 'ios_test.py'
# Runners that cannot run in ci_test or ios_test, each with the reason; their probes count as built.
MANUAL_RUNNERS = {
    'tools/ios_benchmark_overlay_test.py': 'creates and deletes its own iOS simulator; run it as a simulator job',
}


def test_a_probe_built_and_never_run_fails_the_run(tmp_path):
    probes = ProbeLedger()
    probes.note_built(tmp_path / 'ran-probe', ('xcrun', 'clang', 'port/ios/tests/a.c', '-o', 'ran-probe'))
    probes.note_built(tmp_path / 'idle-probe', ('xcrun', 'clang', 'port/ios/tests/b.c', '-o', 'idle-probe'))
    probes.note_ran(tmp_path / 'ran-probe')
    with pytest.raises(SystemExit, match='idle-probe: built by xcrun clang port/ios/tests/b.c'):
        probes.check()
    probes.note_ran(tmp_path / 'idle-probe')
    probes.check()


def test_a_skipped_run_is_reported_not_silent():
    probes = ProbeLedger()
    probes.note_skipped(('python3', 'tools/gui_runner.py'), 'needs a GUI session')
    lines = []
    probes.check(report=lines.append)
    assert lines == ['SKIPPED (not run on this host): python3 tools/gui_runner.py: needs a GUI session']


# The only runs ios_test.py may skip: runners that launch a probe as a Mac Catalyst app (tools/uikit_probe_app.py)
UIKIT_APP_RUNNERS = sorted(f'tools/{path.name}' for path in TOOLS.glob('test_*.py')
                           if re.search(r'apple-ios[0-9.]*-macabi', path.read_text()))


def test_only_declared_uikit_app_runners_may_skip():
    source = IOS_TEST.read_text()
    calls = [node for node in ast.walk(ast.parse(source)) if isinstance(node, ast.Call)
             and isinstance(node.func, ast.Name) and node.func.id == 'run_uikit_app']
    runners = [argument.value for call in sorted(calls, key=lambda call: call.lineno) for argument in call.args
               if isinstance(argument, ast.Constant) and str(argument.value).startswith('tools/')]
    assert UIKIT_APP_RUNNERS, 'no tools/test_*.py builds a Mac Catalyst probe'
    assert sorted(runners) == UIKIT_APP_RUNNERS
    assert source.count('note_skipped(') == 1, 'ios_test.py skips only through run_uikit_app'
    for runner in UIKIT_APP_RUNNERS:
        text = (ROOT / runner).read_text()
        assert 'uikit_probe_app.require_start(' in text, runner


def test_a_rebuilt_probe_has_to_run_again(tmp_path):
    probes = ProbeLedger()
    command = ('xcrun', 'clang', 'port/ios/tests/a.c', '-o', 'probe')
    probes.note_built(tmp_path / 'probe', command)
    probes.note_ran(tmp_path / 'probe')
    probes.note_built(tmp_path / 'probe', command)
    with pytest.raises(SystemExit, match='probe: built by'):
        probes.check()


@pytest.mark.parametrize('spelling', [['-o', 'out'], ['--output', 'out'], ['--output=out']])
def test_every_output_spelling_is_a_probe_link(spelling):
    command = ['xcrun', 'clang', '-O2', 'port/ios/tests/a_probe.c', *spelling, '-framework', 'Foundation']
    assert output_of(command) == 'out'
    assert links_probe(command)


def test_a_probe_link_without_a_readable_output_is_not_a_link():
    command = ['xcrun', 'clang', 'port/ios/tests/a_probe.c', '-ofoo']
    assert compiles_probe(command) and not links_probe(command)
    assert output_of(['xcrun', 'clang', 'a.c', '-o']) is None
    for flag in ('-c', '-S', '-E', '-fsyntax-only'):
        assert not compiles_probe(['xcrun', 'clang', flag, 'port/ios/tests/a_probe.c', '-o', 'out'])
    assert not compiles_probe(['xcrun', 'clang', '-Iport/ios/tests/', 'port/ios/host/a.c', '-o', 'out'])


# ios_test.py's run(...) and execute(...) calls, with each argument as the text it evaluates to; BUILD / 'x' is
# 'BUILD/x', and a variable stays its name, so a link and a run of the same expression compare equal.

def token(node, source):
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    if isinstance(node, ast.Name):
        return node.id
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Div):
        return token(node.left, source) + '/' + token(node.right, source)
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
        return token(node.left, source) + token(node.right, source)
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == 'str' and len(node.args) == 1:
        return token(node.args[0], source)
    if isinstance(node, ast.JoinedStr):
        return ''.join(token(part.value if isinstance(part, ast.FormattedValue) else part, source)
                       for part in node.values)
    return re.sub(r'\s', '', ast.get_source_segment(source, node))


def probe_call(node, source):
    """A probe(name, sources, flags=..., args=..., run=...) call as the link command it runs, and its run unless
    run=False: `xcrun clang`, the flags, the sources and `-o BUILD/name` (a name that is a variable holds a path, as does BUILD / ...)."""
    keywords = {keyword.arg: keyword.value for keyword in node.keywords}
    name, sources = node.args[0], node.args[1]
    flags = keywords.get('flags', node.args[2] if len(node.args) > 2 else ast.List(elts=[]))
    output = token(name, source)
    if not isinstance(name, ast.Name) and not output.startswith('BUILD/'):
        output = 'BUILD/' + output
    elements = lambda value: [token(item, source) for item in value.elts] if isinstance(value, (ast.List, ast.Tuple)) \
        else [token(value, source)]
    link = ['xcrun', 'clang', *elements(flags), *elements(sources), '-o', output]
    calls = [(node.lineno, link, None)]
    if getattr(keywords.get('run'), 'value', True) is not False:
        calls.append((node.lineno, [output, *(elements(keywords['args']) if 'args' in keywords else [])], None))
    return calls


def run_call(node, source):
    arguments = node.args[0].elts if node.func.id == 'execute' and isinstance(node.args[0], ast.List) else node.args
    probe = next((keyword.value for keyword in node.keywords if keyword.arg == 'probe'), None)
    return node.lineno, [token(argument, source) for argument in arguments], token(probe, source) if probe is not None else None


def is_call(node, *names):
    return isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in names


def ios_test_calls(source=None):
    source = IOS_TEST.read_text() if source is None else source
    calls = []
    for node in ast.walk(ast.parse(source)):
        if is_call(node, 'probe'):
            calls += probe_call(node, source)
        elif is_call(node, 'run', 'execute'):
            calls.append(run_call(node, source))
    return sorted(calls, key=lambda call: call[0])


def inline_links(source):
    """Lines of run() and execute() calls that link with a compiler, however it is spelled."""
    return sorted(line for node in ast.walk(ast.parse(source)) if is_call(node, 'run', 'execute')
                  for line, tokens, _ in [run_call(node, source)] if compiler_link(tokens))


def test_every_probe_ios_test_links_is_run_after_each_link():
    """Static twin of the ledger's run-time check, so ci_test catches it on any OS before an ios_test run does."""
    calls = ios_test_calls()
    # every link probe() makes is ledgered, whatever its sources (tools/crt_stills.m too)
    linking = lambda tokens: compiler_link(tokens) and output_of(tokens) is not None
    links = [(line, output_of(tokens)) for line, tokens, _ in calls if linking(tokens)]
    unreadable = [line for line, tokens, _ in calls if compiler_link(tokens) and output_of(tokens) is None]
    assert not unreadable, f'probe links at these lines have no -o, --output or --output= output: {unreadable}'
    assert len(links) >= 150, f'only {len(links)} probe links found: did the compile calls move behind a helper?'
    runs = [(line, probe if probe is not None else tokens[0]) for line, tokens, probe in calls
            if not linking(tokens) and tokens]
    unrun = []
    for index, (line, output) in enumerate(links):
        next_link = next((later for later, other in links[index + 1:] if other == output), float('inf'))
        if not any(line <= run_line < next_link and program == output for run_line, program in runs):
            unrun.append(f'line {line}: {output}')
    assert not unrun, f'probes ios_test.py links and does not run before relinking or the end: {unrun}'


def test_ios_test_links_every_probe_with_probe():
    """ios_test.py's run() refuses a compiler link at run time, however the compiler is spelled; this finds one
    before an ios_test run does."""
    inline = inline_links(IOS_TEST.read_text())
    assert not inline, f'link these probes with probe(name, sources, flags=...), not run(): lines {inline}'


def test_the_static_check_finds_a_sample_inline_link():
    sample = ("run('xcrun', 'clang', '-O2', 'port/ios/tests/a_probe.c', '-o', BUILD / 'a-probe')\n"
              "run(clang, 'port/ios/tests/b_probe.c', '-o', BUILD / 'b-probe')\n"
              "run('xcrun', '--sdk', 'macosx', 'clang', 'tools/c.m', '-o', BUILD / 'c')\n"
              "execute(['/usr/bin/cc', 'd.c', '-o', 'd'])\n"
              "run(clang, '-c', 'port/ios/host/e.c', '-o', BUILD / 'e.o')\n"
              "run('python3', 'tools/test_clang.py')\n"
              "probe('f-probe', ['port/ios/tests/f_probe.c'], flags=['-O2'])\n")
    assert inline_links(sample) == [1, 2, 3, 4]


@pytest.mark.parametrize('command', [
    ('xcrun', 'clang', 'port/ios/tests/a_probe.c', '-o', 'a'), ('clang', 'a.c', '-o', 'a'),
    ('/opt/homebrew/opt/llvm/bin/clang', 'a.c'), ('xcrun', '--sdk', 'xros', 'clang', 'a.c'), ('cc', 'a.c'),
    ('clang++', 'a.cc')])
def test_run_refuses_a_compiler_link(command):
    with pytest.raises(SystemExit, match='link probes with probe'):
        refuse_inline_link(command)


@pytest.mark.parametrize('command', [
    ('xcrun', 'clang', '-c', 'a.c', '-o', 'a.o'), ('clang', '-fsyntax-only', 'a.h'), ('xcrun', 'nm', 'a'),
    ('python3', 'tools/test_clang.py'), ('xcrun', '--sdk', 'xros', 'clang', '-E', 'a.m')])
def test_run_allows_what_does_not_link(command):
    refuse_inline_link(command)


def test_ios_test_checks_its_ledger_and_cannot_ignore_a_failure():
    """A tripwire beside the run-time ledger: the file ends with the check and has no unchecked command."""
    source = IOS_TEST.read_text()
    assert source.rstrip().endswith('probes.check()'), 'ios_test.py must end with probes.check()'
    assert 'check=False' not in source and 'os.system' not in source
    calls = [node for node in ast.walk(ast.parse(source)) if isinstance(node, ast.Call)
             and ast.unparse(node.func) in ('subprocess.run', 'subprocess.call', 'subprocess.Popen')]
    assert len(calls) == source.count('subprocess.run(') + source.count('subprocess.call(') + source.count('subprocess.Popen(')
    for call in calls:
        assert ast.unparse(call.func) == 'subprocess.run', ast.unparse(call)
        assert any(keyword.arg == 'check' and getattr(keyword.value, 'value', None) is True for keyword in call.keywords), \
            ast.unparse(call)


# Every probe source has a runner that something invokes, and that runner names it in code (a string in a command
# or a list of sources, not a comment or a docstring).

def code_strings(path):
    """The string constants of a Python file, docstrings left out."""
    tree = ast.parse(path.read_text(errors='replace'))
    docstrings = {id(node.value) for node in ast.walk(tree) if isinstance(node, ast.Expr)
                  and isinstance(node.value, ast.Constant)}
    return [node.value for node in ast.walk(tree) if isinstance(node, ast.Constant) and isinstance(node.value, str)
            and id(node) not in docstrings]


def imported_tools(path):
    names = set()
    for node in ast.walk(ast.parse(path.read_text(errors='replace'))):
        if isinstance(node, ast.Import):
            names.update(alias.name for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module:
            names.add(node.module)
    return {f'tools/{name}.py' for name in names if (TOOLS / f'{name}.py').is_file()}


def invoked_runners():
    """Runners something invokes: ci_test's tests and standalone programs, what the workflows name, and what those
    run or import, transitively."""
    workflows = '\n'.join(path.read_text() for path in (ROOT / '.github/workflows').glob('*.yml'))
    pending = {f'tools/{name}' for name in re.findall(r'tools/([\w/]+\.py)', workflows)}
    pending |= {f'tools/{path.name}' for path in TOOLS.glob('test_*.py')
                if path.name not in ci_test.APPLE_PROBES | ci_test.STANDALONE}
    pending |= {f'tools/{name}' for name in ci_test.STANDALONE}
    pending |= set(MANUAL_RUNNERS)
    found = set()
    while pending:
        runner = pending.pop()
        if runner in found or not (ROOT / runner).is_file():
            continue
        found.add(runner)
        strings = code_strings(ROOT / runner)
        pending |= {match for text in strings for match in re.findall(r'tools/[\w/]+\.py', text)}
        pending |= imported_tools(ROOT / runner)
    return found, workflows


def test_every_probe_source_is_built_by_a_runner_something_invokes():
    runners, workflows = invoked_runners()
    assert 'tools/ios_test.py' in runners and 'tools/macos_test.py' in runners, sorted(runners)
    strings = [text for runner in runners for text in code_strings(ROOT / runner)]
    sources = [path for path in sorted((ROOT / 'port/ios/tests').glob('*probe*'))
               if path.suffix in ('.c', '.m', '.cpp')]
    assert len(sources) >= 200, f'only {len(sources)} probe sources found'
    def named(path):
        return (path.stem in strings or path.name in strings or path.name in workflows
                or any(text.endswith('/' + path.name) or '/' + path.name + ' ' in text or text == path.name
                       for text in strings))
    orphans = [path.name for path in sources if not named(path)]
    assert not orphans, f'probe sources no invoked runner builds: {orphans}'
