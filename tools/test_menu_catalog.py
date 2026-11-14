"""The English string catalog matches the ID attributes in the shipped menu XML."""
import html
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import menu_catalog

LEAF = re.compile(r'^([A-Za-z0-9_]+) = "((?:[^"\\]|\\.)*)"$')
TABLE = re.compile(r'^\[([a-z0-9_.]+)\]$')


def catalog():
    """Parse en.toml (Python 3.10 has no tomllib) into {id: text}."""
    entries, table = {}, None
    for number, line in enumerate(menu_catalog.CATALOG.read_text(encoding='utf-8').splitlines(), 1):
        if not line or line.startswith('#'):
            continue
        match = TABLE.match(line)
        if match:
            table = match.group(1)
            continue
        match = LEAF.match(line)
        assert match and table, f'en.toml:{number}: unparseable line'
        identifier = f'{table}.{match.group(1)}'
        assert identifier not in entries, f'en.toml:{number}: duplicate ID {identifier}'
        entries[identifier] = re.sub(r'\\(.)', r'\1', match.group(2))
    return entries


def test_catalog_is_current():
    assert menu_catalog.CATALOG.read_text(encoding='utf-8') == menu_catalog.render(menu_catalog.extract())


def test_inline_text_equals_catalog_english():
    # extract() reads text and IDs from the XML; compare against the stored file entry by entry.
    assert menu_catalog.extract() == catalog()


def test_ids_are_well_formed():
    entries = catalog()
    assert entries
    identifiers = sorted(entries)
    for identifier in identifiers:
        assert re.fullmatch(r'[a-z0-9_]+(\.[a-z0-9_]+)+', identifier), identifier
    for first, second in zip(identifiers, identifiers[1:]):
        assert not second.startswith(first + '.'), f'{first} is a prefix of {second}'
    for identifier, text in entries.items():
        assert '|' not in text, f'{identifier} contains a pipe'
        opened = re.findall(r'\{([^{}]*)\}', text)
        assert all(re.fullmatch(r'[a-z][a-z0-9_]*', name) for name in opened), f'{identifier}: bad placeholder'
        assert text.count('{') == text.count('}') == len(opened), f'{identifier}: unbalanced braces'


def test_every_menu_id_is_in_the_catalog():
    entries = catalog()
    attributes = re.compile(r'(?:\bid|text_id|description_id|strings_ids|help_ids)="([^"]*)"')
    for path in sorted((ROOT / 'port/assets/menus').glob('*/*.xml')):
        text = path.read_text(encoding='utf-8')
        for tag in re.findall(r'<(?:widget|string|row|replace)\b[^<>]*>', text):
            for value in attributes.findall(tag):
                for identifier in html.unescape(value).split('|'):
                    assert identifier in entries, f'{path.name}: {identifier} missing from en.toml'


def test_adding_ids_changes_nothing_else():
    # Generated files differ from the ID-free XML only by the ID attributes, and re-adding is idempotent.
    for path in sorted((ROOT / 'port/assets/menus').glob('*/*.xml')):
        text = path.read_text(encoding='utf-8')
        stripped = menu_catalog.STRIP.sub('', text)
        prefix = path.parent.name
        if prefix in ('ce', 'xbox') and stripped != text:
            assert menu_catalog.add_ids(stripped, path.stem, prefix) == text, path.name


def test_check_command_passes():
    subprocess.run([sys.executable, 'tools/menu_catalog.py', '--check'], cwd=ROOT, check=True)


def test_ids_survive_inserting_a_row():
    # An ID follows the element's own path (or a list entry's text), never its position.
    rows = ['<widget name="a/b/one" type="text" text="One" />', '<widget name="a/c/one" type="text" text="Uno" />']
    lists = ['<strings name="L"><string text="Alpha" /><string text="Beta" /></strings>']
    before = menu_catalog.add_ids('\n'.join(lists + rows), 'f', 'xbox')
    after = menu_catalog.add_ids('\n'.join(['<strings name="L"><string text="New" /><string text="Alpha" /><string text="Beta" /></strings>',
                                            '<widget name="a/z/new" type="text" text="New" />'] + rows), 'f', 'xbox')
    ids = re.compile(r'\b(?:id|text_id)="([^"]*)"')
    assert set(ids.findall(before)) <= set(ids.findall(after))
