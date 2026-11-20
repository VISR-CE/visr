#!/usr/bin/env python3
"""What the disc's menu code requires of the lists it is bound to, checked against the lists as the port patches them.

The disc's option-list handlers find a setting by walking to the Nth row of their list and assert a spinner in each
(`source/interface/ui_widget_event_handler_functions.c`), and its help updater asserts a spinner in every row down
to the focused one (`game_options_menu_update_text_desc`). A patch that removes, moves or adds rows in such a list
breaks them: the game halts, or reads the wrong row. This module reads each function's requirement out of the
source, takes the function indexes bound to each list from the disc layout manifest
(`tools/fixtures/disc_menu_layout.json`), applies `port/assets/menus/xbox/patches.xml` to the list's rows, and
reports every requirement the patched list no longer meets.

    python3 tools/disc_contract.py        # print the violations; exit 1 if there are any
"""
from __future__ import annotations

from pathlib import Path
import re
import sys
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

import ui_map_tags  # noqa: E402

HANDLERS = ROOT / 'source/interface/ui_widget_event_handler_functions.c'
DATA_INPUTS = ROOT / 'source/interface/ui_widget_game_data_input_functions.c'
XBOX = ROOT / 'port/assets/menus/xbox'


# Port calls that take over for a list a patch changed and return before the disc's row walk
# (`if (ui_patched_options_help_update(widget)) return;` in game_options_menu_update_text_desc).
PATCHED_LIST_GUARDS = ('if (ui_patched_options_help_update(',)


def function_tables():
    """The C functions the tags name by index: event handlers, then game data inputs."""
    handlers = HANDLERS.read_text(errors='replace')
    start = handlers.index('event_handler_function_list =')
    events = re.findall(r'^\t\t([a-z_0-9]+|NULL),?\s*$', handlers[start:handlers.index('\n};', start)], re.M)
    data = DATA_INPUTS.read_text(errors='replace')
    table = re.search(r'game_data_input_functions?\w*\[[^\]]*\] =\n\{(.*?)\};', data, re.S).group(1)
    inputs = [name.strip() for name in table.split(',') if name.strip()]
    return events, inputs


def function_body(source, name):
    match = re.search(r'\nstatic (?:boolean|void) %s\(\n[^;{]*?\)\n\{\n(.*?)\n\}\n' % re.escape(name), source, re.S)
    return match.group(1) if match else ''


def requirement(source, name):
    """What a function needs of its list: ('positional', rows walked), ('spinner_in_every_row',) or None."""
    body = function_body(source, name)
    walks = len(re.findall(r'\blist_item = list_item->next;', body))
    if walks:
        return ('positional', walks + 1)
    marker = body.find('expected a spinner list somewhere')
    if marker >= 0:
        # a function that hands a patched list to the port's own code before it looks at the rows is safe
        guard = min((at for at in (body.find(name) for name in PATCHED_LIST_GUARDS) if at >= 0), default=-1)
        if 0 <= guard < marker:
            return None
        return ('spinner_in_every_row',)
    return None


def xbox_widgets():
    widgets = {}
    for path in sorted(XBOX.glob('*.xml')):
        for widget in ET.parse(path).getroot().iter('widget'):
            widgets[widget.get('name')] = widget
    return widgets


def patched_rows(layout, ours, patch):
    """The list's rows after the patch, in order: (name, the disc's tag path or None, whether it holds a spinner)."""
    def disc_row(path):
        spinner = any(layout.get(child['widget'], {}).get('type') == 'spinner_list'
                      for child in layout.get(path, {}).get('children', []))
        return (path.rsplit('\\', 1)[-1], path, spinner)

    def our_row(op, model):
        if op.get('widget'):
            widget = ours.get(op.get('widget'))
            spinner = widget is not None and any(
                ours.get(child.get('widget')) is not None and ours[child.get('widget')].get('type') == 'spinner'
                for child in widget.findall('child'))
            return (op.get('name'), None, spinner)
        # a copy of its model keeps the model's children
        return (op.get('name'), None, model[2] if model else False)

    rows = [disc_row(child['widget']) for child in layout[patch.get('list')]['children']]

    def index_of(name):
        for index, row in enumerate(rows):
            if name in (row[0], row[1]):
                return index
        return None

    for op in patch:
        if op.tag == 'row':
            anchor = op.get('after') or op.get('before')
            at = index_of(anchor)
            if at is None:
                continue  # (an anchor that is a screen child, not a row: test_shipped_menu_patches checks those)
            model = disc_row(op.get('model')) if op.get('model') else rows[at]
            rows.insert(at + 1 if op.get('after') else at, our_row(op, model))
        elif op.tag == 'remove':
            at = index_of(op.get('child'))
            if at is not None:
                del rows[at]
        elif op.tag == 'replace':
            at = index_of(op.get('child'))
            if at is not None:
                rows[at] = our_row(op, rows[at])
    return rows


def violations(layout=None, patches=XBOX / 'patches.xml'):
    """(list, function, problem) for every requirement a patched list does not meet."""
    layout = layout or ui_map_tags.load_layout()
    events, inputs = function_tables()
    handler_source = HANDLERS.read_text(errors='replace')
    data_source = DATA_INPUTS.read_text(errors='replace')
    ours = xbox_widgets()
    found = set()
    for patch in ET.parse(patches).getroot().iter('patch'):
        path = patch.get('list')
        leaf = path.rsplit('\\', 1)[-1]
        disc = [child['widget'] for child in layout[path]['children']]
        rows = patched_rows(layout, ours, patch)
        bound = [(events[h['function']], handler_source) for h in layout[path]['handlers']
                 if h['function'] < len(events)]
        bound += [(inputs[index], data_source) for index in layout[path]['game_data_inputs'] if index < len(inputs)]
        for name, source in dict.fromkeys(bound):
            need = requirement(source, name)
            if need is None:
                continue
            if need[0] == 'positional':
                walked = need[1]
                if [row[1] for row in rows[:walked]] != disc[:walked]:
                    found.add((leaf, name, f'walks the first {walked} rows by position, and the patch changes them'))
            else:
                for row in rows:
                    if not row[2]:
                        found.add((leaf, name, f'asserts a spinner in every row; {row[0]} has none'))
    return found


def main():
    found = sorted(violations())
    for leaf, name, problem in found:
        print(f'{leaf}: {name} {problem}')
    return 1 if found else 0


if __name__ == '__main__':
    sys.exit(main())
