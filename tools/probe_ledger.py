"""Which probes a native test runner built and which it ran (tools/ios_test.py).

A probe that is built but never run passes silently: its checks never execute, and nothing in the runner's
output says so. The runner records each probe executable it links and each probe it runs, and fails at the end
when one it linked never ran. `compiles_probe`, `links_probe` and `output_of` are the one copy of the link rule;
the runtime ledger and tools/test_probe_ledger.py's static check both use them.
"""
from pathlib import Path

PROBE_SOURCES = 'port/ios/tests/'
SOURCE_SUFFIXES = ('.c', '.m', '.cpp')
NOT_LINKING = ('-c', '-S', '-E', '-fsyntax-only')


def output_of(args):
    """The output path of a compiler command (`-o X`, `--output X` or `--output=X`), or None."""
    args = [str(arg) for arg in args]
    for index, arg in enumerate(args):
        if arg in ('-o', '--output'):
            return args[index + 1] if index + 1 < len(args) else None
        if arg.startswith('--output='):
            return arg[len('--output='):]
    return None


def probe_sources(args):
    return [str(arg) for arg in args if str(arg).startswith(PROBE_SOURCES) and str(arg).endswith(SOURCE_SUFFIXES)]


def compiles_probe(args):
    """An `xcrun clang` command that compiles a source in port/ios/tests to an executable (not -c, -S, -E or
    -fsyntax-only), whether or not its output could be read."""
    args = tuple(str(arg) for arg in args)
    return (args[:2] == ('xcrun', 'clang') and not any(arg in NOT_LINKING for arg in args)
            and bool(probe_sources(args)))


def links_probe(args):
    """True for a clang command that links an executable from a file in port/ios/tests."""
    return compiles_probe(args) and output_of(args) is not None


class ProbeLedger:
    def __init__(self):
        self.built = {}
        self.ran = set()
        self.skipped = []

    def note_skipped(self, command, reason):
        """A run this host cannot do (a declared GUI-session runner on a headless host); reported, never silent."""
        self.skipped.append((' '.join(str(arg) for arg in command), reason))

    def note_built(self, output, command):
        """A probe executable linked at `output` by `command`; a rebuilt probe has to run again."""
        path = Path(output).resolve()
        self.built[path] = command
        self.ran.discard(path)

    def note_ran(self, program):
        self.ran.add(Path(program).resolve())

    def never_ran(self):
        return [path for path in self.built if path not in self.ran]

    def check(self, report=print):
        """Reports every skipped run, then raises SystemExit naming every probe that was built and never run."""
        for command, reason in self.skipped:
            report(f'SKIPPED (not run on this host): {command}: {reason}')
        missing = self.never_ran()
        if missing:
            lines = [f'  {path.name}: built by {" ".join(self.built[path])}' for path in missing]
            raise SystemExit('probes built but never run (their checks did not execute):\n' + '\n'.join(lines))
