"""Which probes a native test runner built and which it ran (tools/ios_test.py).

A probe that is built but never run passes silently: its checks never execute, and nothing in the runner's
output says so. The runner records each probe executable it links and each probe it runs, and fails at the end
when one it linked never ran. `compiles_probe`, `links_probe` and `output_of` are the one copy of the link rule;
the runtime ledger and tools/test_probe_ledger.py's static check both use them.
"""
import json
import os
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
    def __init__(self, root=None):
        self.root = root
        self.built = {}
        self.ran = set()
        self.skipped = []
        # Every note in order, for HALO_PROBE_LEDGER: two runs' records compare equal when they built the same probes
        # with the same commands and ran them the same way (`python3 tools/probe_ledger.py --compare A B`).
        self.events = []

    def note_skipped(self, command, reason):
        """A run this host cannot do (a declared GUI-session runner on a headless host); reported, never silent."""
        self.skipped.append((' '.join(str(arg) for arg in command), reason))
        self.events.append(['skipped', [str(arg) for arg in command], reason])

    def note_built(self, output, command):
        """A probe executable linked at `output` by `command`; a rebuilt probe has to run again."""
        path = Path(output).resolve()
        self.built[path] = command
        self.ran.discard(path)
        self.events.append(['built', str(path), [str(arg) for arg in command]])

    def note_ran(self, program, arguments=()):
        self.ran.add(Path(program).resolve())
        self.events.append(['ran', str(Path(program).resolve()), [str(arg) for arg in arguments]])

    def portable_events(self):
        """The events with the checkout's own path left out of every string, so two checkouts' records compare."""
        prefix = f'{Path(self.root).resolve()}/' if self.root is not None else None
        def portable(value):
            if isinstance(value, list):
                return [portable(item) for item in value]
            return value.replace(prefix, '') if prefix and isinstance(value, str) else value
        return portable(self.events)

    def never_ran(self):
        return [path for path in self.built if path not in self.ran]

    def check(self, report=print):
        """Reports every skipped run, then raises SystemExit naming every probe that was built and never run. With
        HALO_PROBE_LEDGER set, first writes every note, in order, to that JSON file."""
        if os.environ.get('HALO_PROBE_LEDGER'):
            Path(os.environ['HALO_PROBE_LEDGER']).write_text(json.dumps(self.portable_events(), indent=1) + '\n')
        for command, reason in self.skipped:
            report(f'SKIPPED (not run on this host): {command}: {reason}')
        missing = self.never_ran()
        if missing:
            lines = [f'  {path.name}: built by {" ".join(self.built[path])}' for path in missing]
            raise SystemExit('probes built but never run (their checks did not execute):\n' + '\n'.join(lines))


# Options whose value is the next argument, so it is not an input file.
SEPARATE_VALUE = {'-o', '--output', '-I', '-D', '-U', '-include', '-imacros', '-isystem', '-iquote', '-idirafter',
                  '-isysroot', '-x', '-L', '-l', '-arch', '-target', '-Xlinker', '-Xclang', '-mllvm', '-framework',
                  '-F', '-iframework', '-rpath'}


def link_parts(command):
    """A compiler command as its program, output, input files in order and options in order: two commands with the
    same parts build the same thing, wherever the inputs and the output sit among the options."""
    command = [str(arg) for arg in command]
    program = command[:2] if command[:2] == ['xcrun', 'clang'] else command[:1]
    output, inputs, options = output_of(command), [], []
    index = len(program)
    while index < len(command):
        argument = command[index]
        if argument in SEPARATE_VALUE and index + 1 < len(command):
            if argument not in ('-o', '--output'):
                options.append(f'{argument} {command[index + 1]}')
            index += 2
            continue
        if argument.startswith('--output='):
            pass
        elif argument.startswith('-'):
            options.append(argument)
        else:
            inputs.append(argument)
        index += 1
    return {'program': program, 'output': output, 'inputs': inputs, 'options': options}


def comparable(events):
    return [[kind, rest[0], link_parts(rest[1])] if kind == 'built' else [kind, *rest] for kind, *rest in events]


def compare(before, after):
    """The differences between two HALO_PROBE_LEDGER records, as lines; empty when they built and ran the same."""
    before, after = comparable(before), comparable(after)
    if before == after:
        return []
    import difflib
    dump = lambda events: [json.dumps(event, sort_keys=True) for event in events]
    return list(difflib.unified_diff(dump(before), dump(after), 'before', 'after', lineterm='', n=1))


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description='Compare two HALO_PROBE_LEDGER records from tools/ios_test.py.')
    parser.add_argument('--compare', nargs=2, metavar=('BEFORE', 'AFTER'), required=True)
    options = parser.parse_args(argv)
    before, after = (json.loads(Path(path).read_text()) for path in options.compare)
    differences = compare(before, after)
    built = sum(1 for event in after if event[0] == 'built')
    ran = sum(1 for event in after if event[0] == 'ran')
    if differences:
        print('\n'.join(differences))
        print(f'probe ledgers differ ({len(differences)} diff lines)')
        return 1
    print(f'probe ledgers match: {built} builds, {ran} runs, in the same order')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
