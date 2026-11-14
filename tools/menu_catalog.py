"""String catalog support for the generated menu XML (design: i18n string catalog, 0.7.0).

The generators (tools/xbox_menus.py, tools/ce_menus.py through tools/port_settings.py)
call add_ids() on each XML document they write. It gives every translatable text
attribute of a VISR-written element an ID attribute, derived from the file and the
element's name, and leaves the text itself inline, so the English output is the same
bytes as before apart from the added ID attributes.

port/assets/strings/en.toml is extracted from the shipped XML by this script
(--write), and CI checks it is current (--check): its keys are the IDs, its values
the inline English.

    python3 tools/menu_catalog.py --write    # regenerate en.toml
    python3 tools/menu_catalog.py --check    # exit 1 if en.toml is stale
"""

import argparse
import hashlib
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MENUS = ROOT / 'port/assets/menus'
CATALOG = ROOT / 'port/assets/strings/en.toml'
# Folders of generated files that carry IDs (ce files are listed by name: only
# the ones tools/port_settings.py writes).
ID_FOLDERS = ('xbox',)

TAG = re.compile(r'<(/?)(widget|string|strings|row|replace)\b([^<>]*?)(/?)>', re.S)
ATTR = re.compile(r'([\w:.-]+)="([^"]*)"')
STRIP = re.compile(r' (?:id|text_id|description_id|strings_ids|help_ids)="[^"]*"')
SEGMENT = re.compile(r'[^a-z0-9_]+')


def segment(text):
    return SEGMENT.sub('_', text.lower()).strip('_') or 'x'


def add_ids(xml, stem, prefix):
    """Return the XML with an ID attribute after each translatable attribute."""
    xml = STRIP.sub('', xml)  # IDs copied from another generated file are reassigned
    stem = f'{prefix}.{segment(stem)}'
    seen = {}
    list_name = None

    def unique(base):
        count = seen.get(base, 0) + 1
        seen[base] = count
        return base if count == 1 else f'{base}_{count}'

    def rewrite(match):
        nonlocal list_name
        closing, tag, body, selfclose = match.groups()
        if closing:
            if tag == 'strings':
                list_name = None
            return match.group(0)
        attributes = dict(ATTR.findall(body))
        if tag == 'strings':
            list_name = None if selfclose else segment(attributes.get('name', ''))
            return match.group(0)
        pairs = []  # (attribute, id attribute, [ids])
        name = segment(attributes.get('name', '').replace('\\', '/'))  # the whole path: stable when siblings move
        if tag == 'string':
            if list_name is None or not attributes.get('text'):
                return match.group(0)
            slug = segment(attributes['text'])
            if len(slug) > 40:  # long text: a cut slug plus a hash of the whole
                slug = f'{slug[:32]}_{hashlib.sha1(attributes["text"].encode()).hexdigest()[:6]}'
            ident = unique(f'{stem}.{list_name}.{slug}')
            pairs.append(('text', 'id', [ident]))
        else:
            base = unique(f'{stem}.{name}')
            for attribute, id_attribute, role in (('text', 'text_id', 'text'), ('description', 'description_id', 'description')):
                value = attributes.get(attribute)
                if attribute == 'description' and tag == 'widget':
                    continue  # a widget's description names a dependency, not text
                if value and '|' not in value and re.search(r'[A-Za-z]', value):
                    pairs.append((attribute, id_attribute, [f'{base}.{role}']))
            for attribute, id_attribute, role in (('strings', 'strings_ids', 's'), ('help', 'help_ids', 'help')):
                value = attributes.get(attribute)
                if value and ((tag == 'widget') != (attribute == 'strings')):
                    continue  # strings belongs to widgets, help to patch rows
                if value:
                    parts = value.split('|')
                    pairs.append((attribute, id_attribute, [f'{base}.{role}{i}' for i in range(len(parts))]))
        for attribute, id_attribute, identifiers in pairs:
            value_match = re.search(rf'\b{attribute}="[^"]*"', body)
            if not value_match:
                continue
            insert = f' {id_attribute}="{"|".join(identifiers)}"'
            body = body[:value_match.end()] + insert + body[value_match.end():]
        return f'<{tag}{body}{"/" if selfclose else ""}>'

    return TAG.sub(rewrite, xml)


def extract(menus=MENUS):
    """Return {id: english} from every shipped file's ID attributes, in file order."""
    entries = {}
    for folder in sorted(menus.iterdir()):
        if not folder.is_dir():
            continue
        for path in sorted(folder.glob('*.xml')):
            text = path.read_text(encoding='utf-8')
            list_texts = None
            for match in TAG.finditer(text):
                closing, tag, body, _ = match.groups()
                if closing:
                    continue
                attributes = {k: html.unescape(v) for k, v in ATTR.findall(body)}
                for attribute, id_attribute in (('text', 'id'), ('text', 'text_id'), ('description', 'description_id'),
                                                ('strings', 'strings_ids'), ('help', 'help_ids')):
                    if id_attribute not in attributes:
                        continue
                    if id_attribute == 'id' and tag != 'string':
                        continue
                    identifiers = attributes[id_attribute].split('|')
                    values = attributes[attribute].split('|')
                    if len(identifiers) != len(values):
                        raise ValueError(f'{path.name}: {id_attribute} count differs from {attribute}')
                    for identifier, value in zip(identifiers, values):
                        if identifier in entries and entries[identifier] != value:
                            raise ValueError(f'{path.name}: ID {identifier} has two texts')
                        entries[identifier] = value
    return entries


def toml_quote(value):
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def render(entries):
    tables = {}
    for identifier, value in entries.items():
        head, _, leaf = identifier.rpartition('.')
        tables.setdefault(head, {})[leaf] = value
    lines = ['# English menu text, extracted from the generated XML by tools/menu_catalog.py.',
             '# Do not edit: change the generator and run `python3 tools/menu_catalog.py --write`.', '']
    for head in sorted(tables):
        lines.append(f'[{head}]')
        for leaf, value in tables[head].items():
            lines.append(f'{leaf} = {toml_quote(value)}')
        lines.append('')
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    text = render(extract())
    if args.write:
        CATALOG.parent.mkdir(parents=True, exist_ok=True)
        CATALOG.write_text(text, encoding='utf-8')
    elif args.check:
        if not CATALOG.is_file() or CATALOG.read_text(encoding='utf-8') != text:
            print('port/assets/strings/en.toml is stale: run python3 tools/menu_catalog.py --write', file=sys.stderr)
            sys.exit(1)


if __name__ == '__main__':
    main()
