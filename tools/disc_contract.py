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

    # (a replaced row is still found by the map's path, as the game's patch code finds it)
    replaced = {}

    def index_of(name):
        name = replaced.get(name, name)
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
                replaced[op.get('child')] = op.get('name')
    return rows


OPTION_ROW_ART = 'option_bkds'
# The kinds of disc rows the engine makes the standard tall row, by the art every row of a list draws: option rows
# (a label and a value), and picture-list rows (text rows beside a picture panel, the first with a top variant).
ROW_KINDS = {'option': {OPTION_ROW_ART}, 'picture': {'list_item_bkd', 'list_item_bkd_top'}}
TALL_PITCH = 39
# where a picture list's rows stop: the bottom bar (its panel, not a help text, is beside the rows)
LEGEND_Y = 414


def row_kind(layout, path):
    """The kind of a disc column list's rows ('option' or 'picture'), or None for any other list."""
    widget = layout[path]
    if widget['type'] != 'column_list' or not widget['children']:
        return None
    arts = {layout[child['widget']]['bitmap'].rsplit('\\', 1)[-1] for child in widget['children']}
    return next((kind for kind, known in ROW_KINDS.items() if arts <= known), None)


def disc_row_tops(layout, path):
    """Where each of a disc list's rows starts, in order: an option row at its child offset, a picture row at its
    child offset plus its own bounds' top (bounds are left, top, right, bottom)."""
    return [child['y'] + layout[child['widget']]['bounds'][1] for child in layout[path]['children']]


def disc_gaps(tops):
    """The extra space the disc leaves between rows: each step's surplus over the list's closest pitch."""
    steps = [b - a for a, b in zip(tops, tops[1:]) if b > a]
    return sum(step - min(steps) for step in steps) if steps else 0


def standard_lists_tall(layout, patches_root, ours=None):
    """Every disc list of a standard row kind, laid out with every row a standard tall row at the standard pitch
    from its first row (a picture list keeping the disc's gaps): {list path: (rows, where they end, the limit, the
    screen that holds the list, the row kind)}. The limit is the list's patch's footer, else the top of an option
    list's help text or the bottom bar for a picture list."""
    patches = {patch.get('list'): patch for patch in patches_root.iter('patch')}
    found = {}
    for path, widget in layout.items():
        kind = row_kind(layout, path)
        if kind is None:
            continue
        disc = widget['children']
        patch = patches.get(path)
        rows = len(patched_rows(layout, ours or {}, patch)) if patch is not None else len(disc)
        if patch is not None and patch.get('footer'):
            limit = int(patch.get('footer'))
        else:
            limit = layout[widget['description']]['bounds'][1] if kind == 'option' else LEGEND_Y
        tops = disc_row_tops(layout, path) if kind == 'picture' else [child['y'] for child in disc]
        bottom = min(tops) + rows * TALL_PITCH + (disc_gaps(tops) if kind == 'picture' else 0)
        screen = next(parent for parent, other in layout.items()
                      if any(child['widget'] == path for child in other['children']))
        found[path] = (rows, bottom, limit, screen, kind)
    return found


def option_lists_tall(layout, patches_root, ours=None):
    """standard_lists_tall's option lists, as (rows, where they end, the limit, the screen)."""
    return {path: fit[:4] for path, fit in standard_lists_tall(layout, patches_root, ours).items() if fit[4] == 'option'}


def reachable_screens(layout, patches_root, ours, redirects, named=()):
    """The screens the game can show: every widget reached from the main menu and from the screens the game opens
    by name (named), through the map's own handlers and ours, as the game follows them: a redirected screen is
    never shown (its target is), a row a patch removes opens nothing, and a row a patch adds opens its target.
    ours is {name: element} of the menus' own widgets; names are tag paths, ours under pc\\."""
    removed, added = set(), {}
    for patch in patches_root.iter('patch'):
        for op in patch:
            if op.tag in ('remove', 'replace'):
                removed.add(op.get('child'))
            if op.tag in ('row', 'replace') and (op.get('open') or op.get('widget')):
                added.setdefault(patch.get('screen'), set()).add(op.get('open') or op.get('widget'))

    def tag(name):
        return name if '\\' in name else 'pc\\' + name.replace('/', '\\')

    def map_opens(path, seen):
        if path in seen or path not in layout or path in removed:
            return set()
        seen.add(path)
        found = {handler['widget'] for handler in layout[path]['handlers'] if handler.get('widget')}
        for child in layout[path]['children']:
            found |= map_opens(child['widget'], seen)
        return found

    def our_opens(name, seen):
        if name in seen or name not in ours:
            return set()
        seen.add(name)
        found = set()
        for element in ours[name].iter():
            found |= {tag(element.get(key)) for key in ('open', 'replace', 'otherwise') if element.get(key)}
            if element.tag == 'child':
                found |= our_opens(element.get('widget'), seen)
        return found

    shown, waiting = set(), ['ui\\shell\\main_menu\\main_menu', *named]
    while waiting:
        screen = waiting.pop()
        screen = redirects.get(screen, screen)
        if screen in shown:
            continue
        shown.add(screen)
        if screen.startswith('pc\\'):
            waiting += our_opens(screen[3:].replace('\\', '/'), set())
        else:
            waiting += map_opens(screen, set()) | {tag(name) for name in added.get(screen, ())}
    return shown


def screens_named_in_the_game():
    """Tag paths the game's own menu code names: screens it may open itself (an over-count is safe for a fit test)."""
    text = ''.join((ROOT / 'port/linux/game' / name).read_text() for name in ('menu_functions.c', 'menu_tags.c'))
    return sorted({match.replace('\\\\', '\\') for match in re.findall(r'"((?:ui|pc)\\\\[^"]+)"', text)})


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
