#!/usr/bin/env python3
"""Generate Xbox-look settings and multiplayer screens from the shipped PC rows (no game data needed).

    python3 tools/xbox_menus.py

Widget geometry follows ui.map's advanced_controls_screen and profile edit
list. Colors were checked against advanced_controls/invert_joystick_label
and advanced_controls_help: ARGB (1, 40/255, 150/255, 1) and white.
Only bitmap and font tags refer to the map; PNGs reuse the shipped redraws.
The map+PNG frame parser/build support is supplied by the fallback lane.
"""
from __future__ import annotations

import math
import argparse
import about_data
import menu_titles
from copy import deepcopy
import json
import re
import textwrap
from pathlib import Path
from xml.etree import ElementTree as ET

import menu_catalog
import menu_geometry
import port_settings
from port_settings import apply_now_spinner_handlers
# Shared geometry (rows, legend, value columns, dropdown) lives in menu_geometry.py, which also writes the
# engine's menu_geometry.h.
from menu_geometry import (COMFORTABLE_PITCH, DROPDOWN_PITCH, FIRST_ROW, LEGEND_Y,
                           ROW_HEIGHT, RULE_Y)

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'
# The generator's own source rows: widget definitions it builds the screens from. They are input to this tool only
# and are not shipped or read by the game. The settings and multiplayer screens come from tools/port_settings.py in
# memory; the rest are the XML files in this folder.
SOURCES = ROOT / 'tools/menu_sources'


def generated_sources():
    return {**port_settings.settings_files(), **port_settings.multiplayer_files()}


def source_names():
    return sorted({path.name for path in SOURCES.glob('*.xml')} | set(generated_sources()))


def source(name):
    """A source document's root element, parsed anew each call so a caller may change it."""
    lines = generated_sources().get(name)
    if lines is not None:
        return ET.fromstring('\n'.join(lines))
    return ET.parse(SOURCES / name).getroot()
PE = 'main_menu/settings_select/player_setup/player_profile_edit'
# fields under the header rule sit this far down, leaving a gap below the rule
FIELD_GAP = 8
FIELD_Y = FIRST_ROW + FIELD_GAP
ROW_PITCH = COMFORTABLE_PITCH
# the scroll arrows sit to the right of the 512-wide rows
ARROW_X = 574
PAGES = ('video_settings', 'audio_settings', 'network_setup', 'controls_setup',
         'mouse_settings', 'gamepad_setup', 'about', 'graphics', 'maps')
BLUE = '#FF2896FF'
# the PC solo list's widgets the Xbox lobby shows beside its map
LOBBY_MAP_WIDGETS = ('main_menu/solo_level_select/replay_level_right_pic',
                     'main_menu/solo_level_select/replay_level_right_name')
MP = 'main_menu/multiplayer_type_select'
MP_PAGES = tuple('main_menu.multiplayer_type_select.' + name for name in (
    'join_game', 'join_game.filters', 'join_game.port', 'direct_ip', 'server_settings', 'coop_options', 'lobby',
    'mp_map_select', 'game_options_edit', 'respawn_options_edit', 'teams_hud_edit', 'weapons_edit', 'vehicle_types_edit')) + (
    'main_menu.quit_select', 'main_menu.new_select.port',
    'main_menu.settings_select.multiplayer_setup.playlist_select')
# Bounds from tags/widgets.json: connected/server_list, advanced_controls,
# and error/confirm_buttons. The probe omits colors; ui.map's text_color
# at 0x10C confirms server_list_item0 = ARGB(1, 0, .5, 1), description
# labels = BLUE, values = white. No game data is needed to regenerate.
SERVER_BLUE = '#FF0080FF'
# ui.map has the left and right arrows only: the up and down arrows (scroll
# cues, sort arrows) are PC redraws with PNG frames.
# PC-only redraws have PNG frames; only these shared names exist in ui.map.
MAP_BITMAPS = {'gradient', 'list_field', 'list_item_bkd_top', 'list_item_bkd',
               'semi_transparent_grey', 'alert_bkd', 'arrow_big_left', 'arrow_big_right',
               'spinner_list_3_wide_item_background', 'option_bkds', 'arrow_sm_left',
               'arrow_sm_right', 'game_type_grafix', 'black'}



# Shared geometry is in the game's 640x480 menu space. Row art remains 36
# units tall inside a comfortable 39-unit tap band.
TITLE = (35, 11, 512, 64)
LEGEND_A_X, LEGEND_B_X = 471, 326
GLYPH_WIDTH, GLYPH_HEIGHT, CAPTION_OFFSET = 21, 26, 22
HELP_TOP = 335
# Both right panels end at the bottom rule (y 409, drawn by the gradient art), as the disc's panels do.
PANEL_A = {'list': (51, 78, 33), 'panel': (275, 78, 320, RULE_Y - 78)}
PANEL_B = {'list': (48, 78, ROW_PITCH), 'panel': (323, 115, 254, RULE_Y - 115)}
# list_field__0.png is a 2048 square whose visible panel outline fills only 1260x1288 of it, and a frame is drawn at
# its own width and height: a panel's own frame size makes the outline fill the panel's bounds exactly (the stock
# 512 frame leaves it 315x322 units, so the taller panels ended short of the rule and the narrower ones lost their
# right and bottom edges). The leaf stays list_field for ui_redraw.
FIELD_PNG, FIELD_ART = 2048, (1260, 1288)
PANEL_BITMAPS = {'a': 'xbox/panel_a/list_field', 'b': 'xbox/panel_b/list_field'}


def panel_frame_size(panel):
    return (round(FIELD_PNG * panel[2] / FIELD_ART[0]), round(FIELD_PNG * panel[3] / FIELD_ART[1]))
DIALOG_PANEL = (192, 176, 239, 159)
DIALOG_HEADER = (9, 4)
DIALOG_HEADER_POSITION = tuple(a + b for a, b in zip(DIALOG_PANEL[:2], DIALOG_HEADER))
# B and A targets sit inside the panel's width: 4 units left of B, 5 right of A.
# Drawn pill: x, y, w, h. It sits in the footer band (rule at panel y 130, panel bottom at 159), centered.
DIALOG_BUTTON = (122, 132, 108, 25)
# Tap area: at least 53 units tall (44 pt), 112 wide, centered on the pill; it extends up across the rule.
DIALOG_HIT = (122, 118, 112, 53)
# The footer band runs from the rule (panel y 130) to the panel's bottom (159). The glyph (26 tall) is centered
# in it, as is the drawn pill (DIALOG_BUTTON); the caption sits 2 below the glyph's top.
DIALOG_RULE, DIALOG_BOTTOM = 130, 159
DIALOG_GLYPH_Y = (DIALOG_RULE + DIALOG_BOTTOM - 26) // 2
DIALOG_GLYPH, DIALOG_GLYPH_GAP = 21, 4
# large_ui capitals run about 12.5 units each; slots hug glyph + caption + padding, 60 wide at least (tap area),
# and the row is centered as a group.
DIALOG_CAPTION_CHAR = 12.5
DIALOG_SLOT_PAD, DIALOG_SLOT_GAP, DIALOG_SLOT_MIN = 8, 6, 60


def title():
    return dict(zip(('left', 'top', 'width', 'height'), TITLE))


def rows(x, y, pitch, count):
    return ((x, y + index * pitch) for index in range(count))


def help_box(top):
    return dict(left=68, top=top, width=482, height=LEGEND_Y - top - 4)


def panel(spec):
    geometry = spec['panel'] if isinstance(spec, dict) else spec
    return dict(zip(('left', 'top', 'width', 'height'), geometry))


# Caption boxes for known legend labels, in small_ui units. The touch pill
# runs from the glyph to the caption box's right edge plus 8 units each side,
# so these widths decide how tightly a bar packs.
LEGEND_CAPTION_WIDTHS = {'START NOW': 90, 'SWITCH TEAM': 100, 'ADD PLAYER': 108,
                         'LEAVE': 50, 'SHARE': 55}
LEGEND_RIGHT = 608
LEGEND_PILL_PAD, LEGEND_GAP = 8, 4
# Touch pills run from y 397 (60 units centered on the glyph); boxes above
# the legend end by here so a pill never touches their border.
LEGEND_CLEAR_Y = 389
# large_ui captions wider than the B/A pair's 100-unit box.
LEGEND_PAIR_WIDTHS = {'JOIN GAME': 112}


def legend_caption_width(label):
    """Caption box width for a legend label; unknown text gets the 90 default."""
    if not label:
        return 90
    return LEGEND_CAPTION_WIDTHS.get(label, 10 * len(label))


def legend_positions(buttons, widths=None):
    """Keep the B/A pair in place; pack denser bars from the right by caption width.

    Touch pills span x-8 .. x+22+width+8, so neighbors are placed 4 units apart.
    """
    buttons = tuple(buttons)
    if len(buttons) <= 2 and set(buttons) <= {'a', 'b'}:
        return {b: LEGEND_A_X if b == 'a' else LEGEND_B_X for b in buttons}
    widths = widths or {}
    positions, right = {}, LEGEND_RIGHT
    for b in reversed(buttons):
        w = widths.get(b, 90)
        positions[b] = right - CAPTION_OFFSET - w
        right = positions[b] - 2 * LEGEND_PILL_PAD - LEGEND_GAP
    return {b: positions[b] for b in buttons}


def widget(root, name, **attributes):
    return ET.SubElement(root, 'widget', {'name': name, **{k: str(v) for k, v in attributes.items()}})


def child(parent, name, **attributes):
    return ET.SubElement(parent, 'child', {'widget': name, **{k: str(v) for k, v in attributes.items()}})


def text_widget(root, name, text, **attributes):
    return widget(root, name, type='text', text=text, font=r'ui\large_ui', color=BLUE,
                  **attributes)


def validate(root, assets):
    """Refuse incomplete map frames before writing any generated XML."""
    for frame in root.iter('frame'):
        png = frame.get('png')
        if not png or not (assets / png).is_file():
            raise ValueError(f'frame needs an existing PNG fallback: {png!r}')
    for element in root.iter():
        if element.tag == 'widget' and '\\' in element.get('name', ''):
            raise ValueError(f"map widget definition: {element.get('name')}")
        for key in ('widget', 'description', 'root'):
            # (a patch row's description is text, whose "\n" is a line break; a <title> names a disc screen's
            # title widget by its path, as a patch names its list)
            if (element.tag == 'row' and key == 'description') or (element.tag == 'title' and key == 'widget'):
                continue
            if '\\' in element.get(key, ''):
                raise ValueError(f'map widget dependency: {element.get(key)}')


def serialize(root):
    ET.indent(root, space='\t')
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<!-- Generated by tools/xbox_menus.py; edit the generator or PC source rows. -->\n'
            + ET.tostring(root, encoding='unicode', short_empty_elements=True) + '\n')


# PNGs the game names itself (port/linux/game/menu_tags.c), beside those the XML's frames name.
RUNTIME_PNGS = ('pictures/port/blank__0.png',)


def manifest_files(menus):
    """The files the game embeds: the generated XML and every PNG it or the game names."""
    names = set(RUNTIME_PNGS)
    for path in sorted((menus / 'xbox').glob('*.xml')):
        names.add(path.relative_to(menus).as_posix())
        root = ET.parse(path).getroot()
        validate(root, menus)
        names.update(frame.get('png') for frame in root.iter('frame'))
        names.update(row.get('picture')[4:] for row in root.iter('row')
                     if row.get('picture', '').startswith('png:'))
    return sorted(names)


def bar(screen, actions):
    """The screen's button bar: what each action does, never where it goes (the engine places it, menu_files.c).

    actions: (slot, scope, caption, what) with slot x, y, b or a; scope global, select or back; caption a string,
    or a list of captions the port picks from at run time (ui_widget_port_bar_caption; what's 'index' is the first
    shown); what is {'focus': 'true'} (the focused widget takes the button) or {'run': ...} / {'open': ...}, plus
    'while_typing': 'pointer' for an action whose key is a letter while a field is being typed in.
    """
    element = ET.SubElement(screen, 'bar')
    for slot, scope, caption, what in actions:
        text = {'strings': '|'.join(caption)} if isinstance(caption, (list, tuple)) else {'text': caption}
        ET.SubElement(element, 'action', {'slot': slot, 'scope': scope, **text, **what})
    return element


def legend(root, screen, base, accept='SELECT', cancel='CANCEL', accept_platform=None, buttons=None,
           declared=True, extra=()):
    """A/B icons post buttons to the current focus, as the Xbox legend does.

    A plain SELECT/BACK legend is a declared bar (`bar`); declared=False keeps the placed glyphs and captions for
    the callers that still read them by name (a caption that changes at run time, dialogs).
    """
    hosting = screen.get('name').endswith('/server_settings_screen')
    if declared and buttons is None and not hosting and not accept_platform:
        bar(screen, list(extra) + [(slot, scope, label, {'focus': 'true'})
                                   for slot, scope, label in (('b', 'back', cancel), ('a', 'select', accept))
                                   if label is not None])
        return
    if buttons is None:
        buttons = [('b', 'cancel', cancel), ('a', 'accept', accept)]
    buttons = [item for item in buttons if item[2] is not None]
    # Co-op's B-only legend keeps the standard two-button place.
    widths = {b: legend_caption_width(label) if len(buttons) > 2 else
              max(100, LEGEND_PAIR_WIDTHS.get(label, 100)) for b, _, label in buttons}
    positions = legend_positions([b for b, _, _ in buttons], widths)
    if set(positions) <= {'a', 'b'}:
        # A wider caption grows left; its neighbor moves with it so the pair
        # keeps its right edge and the gap between the pills.
        shift = 0
        for b in ('a', 'b'):
            if b in positions:
                shift += widths[b] - 100
                positions[b] -= shift
    if hosting:
        buttons = [('x', 'copy', 'INVITE'), ('b', 'cancel', cancel), ('a', 'accept', accept)]
        positions = SCREEN_LAYOUTS[screen.get('name')]['legend_targets']
        widths = SCREEN_LAYOUTS[screen.get('name')]['legend_widths']
    for button, caption, label in buttons:
        icon = button + '_butn'
        x = positions[button]
        if hosting:
            # The invite field needs its own 238-unit slot before these targets.
            # Each target spans its touch pill (glyph, caption, 8-unit pad).
            key = widget(root, f'{base}/{icon}', width=CAPTION_OFFSET + widths[button] + 8, height=53)
            child(screen, key.get('name'), x=x, y=LEGEND_Y)
            art = widget(root, f'{base}/{icon}/{icon}', width=GLYPH_WIDTH, height=GLYPH_HEIGHT,
                         bitmap=f'xbox/browser/{icon}')
            child(key, art.get('name'), y=4)
            text_widget(root, f'{base}/{caption}', label, width=widths[button], height=22)
            child(key, f'{base}/{caption}', x=CAPTION_OFFSET, y=4)
            ET.SubElement(key, 'on', event='left_mouse', run='ss copy invite' if button == 'x' else
                          ('mouse emit accept event' if button == 'a' else 'mouse emit back event'))
            continue
        only = {'platform': accept_platform} if accept_platform and button == 'a' else {}
        child(screen, f'{base}/{icon}', x=x, y=LEGEND_Y, **only)
        child(screen, f'{base}/{caption}', x=x + CAPTION_OFFSET, y=LEGEND_Y, **only)
        widget(root, f'{base}/{icon}', type='container', width=GLYPH_WIDTH, height=GLYPH_HEIGHT,
               bitmap=f'xbox/browser/{icon}')
        label_widget = text_widget(root, f'{base}/{caption}', label, width=widths[button], height=22)
        if len(buttons) > 2:
            label_widget.set('font', r'ui\small_ui')


def footer(root, base, defaults):
    """A settings page's bar: X DEFAULTS (a screen-wide action, so it works whichever row has the focus), B BACK and A
    SELECT. Each change applies and saves as it is made, so the bar has no SAVE."""
    screen = root[0]
    # (the PC page's own button row, a focusable item of the options list, is not this look's)
    for parent in root.iter('widget'):
        for c in list(parent.findall('child')):
            if c.get('widget') == f'{base}/button_bar':
                parent.remove(c)
    legend(root, screen, base, cancel='BACK', extra=[('x', 'global', 'DEFAULTS', {'run': defaults})])


def apply_now_spinners(root):
    for spinner in root.iter('widget'):
        if spinner.get('type') != 'spinner' or not spinner.get('setting'):
            continue
        # (each step is written and kept, so nothing is saved as the screen closes)
        for on in list(spinner.findall('on[@event="deleted"]')):
            spinner.remove(on)
        for handler in apply_now_spinner_handlers():
            spinner.append(ET.fromstring(handler))


# Every value control (spinner: value box plus arrows) takes one of these widths by its longest value, so arrows
# line up across the rows of a class. Text is about 10.5 units per character, which sets each class's limit.
# (name, width, most characters). WIDE holds the longest lists ("DISPLAY (240 HZ)" is 16 characters) and is the widest
# that fits a 512-unit settings row with the child at x=320.
SPINNER_WIDTHS = (('short', 80, 4), ('medium', 103, 9), ('wide', 206, 19))
SPINNER_UNITS_PER_CHARACTER = 10.5
SPINNER_ROW_MARGIN = 8
# A spinner taps as two halves (back, forward); each half is at least the 39-unit row pitch wide
SPINNER_MINIMUM_HALF_WIDTH = 39


# Values the game writes into a spinner at run time, beyond its listed strings (menu_tags.c): the Mac app's own
# refresh rate reads "DISPLAY (240 HZ)"
SPINNER_RUNTIME_VALUES = {'frame_rate_spinner': ('DISPLAY (240 HZ)',)}


# Spinners whose values are wider than their character count suggests (capital M, T and W run wide in the menu font:
# "MOVEMENT" clipped its last letter in the medium class) take the wide class
SPINNER_WIDTH_FLOOR = {'group_spinner': 206}


def is_value_control(widget):
    """A spinner, or the state word of a row that opens a screen of its own (a text of a setting, shown in the
    place and width a spinner's value has, so the values of a screen's rows line up)"""
    return widget.get('type') == 'spinner' or (widget.get('type') == 'text' and widget.get('setting') is not None)


def spinner_width_class(longest):
    for name, width, limit in SPINNER_WIDTHS:
        if longest <= limit:
            return name, width
    raise ValueError(f'spinner value of {longest} characters fits no standard width')


def spinner_longest_values(documents):
    """Each spinner's longest value in characters: inline `strings`, or its `string_list` found in any document."""
    lists = {}
    for root in documents.values():
        for strings in root.iter('strings'):
            lists[strings.get('name')] = [s.get('text', '') for s in strings.findall('string')]
    result = []
    for root in documents.values():
        for spinner in root.iter('widget'):
            if not is_value_control(spinner):
                continue
            if spinner.get('strings') is not None:
                values = spinner.get('strings').split('|')
            else:
                values = lists[spinner.get('string_list')]
            values = list(values) + list(SPINNER_RUNTIME_VALUES.get(spinner.get('name').rsplit('/', 1)[-1], ()))
            result.append((spinner, max(len(v) for v in values)))
    return result


def standardize_spinners(documents):
    for spinner, longest in spinner_longest_values(documents):
        width = spinner_width_class(longest)[1]
        width = max(width, SPINNER_WIDTH_FLOOR.get(spinner.get('name').rsplit('/', 1)[-1], 0))
        # Centered on the wide slot's center line (the generator's source left edge plus half the wide width), so the
        # values of every control on a screen share one vertical center and the arrows stagger symmetrically
        wide = max(w for _, w, _ in SPINNER_WIDTHS)
        spinner.set('left', str(int(spinner.get('left', '0')) + (wide - width) // 2))
        spinner.set('width', str(width))
        # The arrows bracket the value box: bounds are relative to the widget's origin, not to the box's left edge
        left = int(spinner.get('left', '0'))
        if spinner.get('type') == 'spinner':
            spinner.set('header_bounds', f'7 {left - 9} 19 {left - 3}')
            spinner.set('footer_bounds', f'7 {left + width + 3} 19 {left + width + 9}')
    # Every control on a screen keeps one common center line. When the widest control would run past its row, the
    # whole screen's controls move left together (by whatever the worst row needs), so nothing clips and the centers
    # stay aligned.
    for root in documents.values():
        widgets = {w.get('name'): w for w in root.iter('widget')}
        parts = []
        for row in root.iter('widget'):
            for part in row.findall('child'):
                spinner = widgets.get(part.get('widget'))
                if spinner is None or not is_value_control(spinner) or not row.get('width'):
                    continue
                reach = int(spinner.get('left', '0')) + int(spinner.get('width')) + 9
                limit = int(row.get('width')) - SPINNER_ROW_MARGIN
                center = int(part.get('x', '0')) + int(spinner.get('left', '0')) + int(spinner.get('width')) / 2
                parts.append((part, center, center - max(0, int(part.get('x', '0')) + reach - limit)))
        if not parts:
            continue
        target = min(allowed for _, _, allowed in parts)
        for part, center, _ in parts:
            shift = math.ceil(center - target)
            if shift:
                part.set('x', str(int(part.get('x', '0')) - shift))


# PC-look rows the Xbox look leaves out; their config keys stay.
XBOX_DROPPED_ROWS = {
    'video_settings': ('op_resolution_scaling', 'resolution_scaling_label', 'resolution_scaling_spinner'),
}


def settings_page(assets, folder, bitmaps):
    root = source(f'{PE.replace("/", ".")}.{folder}.xml')
    pending = None
    prefix = f'{PE}/{folder}'
    base = f'xbox/settings/{folder}'
    removed = ('button_bar', 'button_defaults', 'button_ok') + XBOX_DROPPED_ROWS.get(folder, ())
    if folder == 'about':
        # (the About screen has a BACK button on the PC look, the legend's B here, and the scroll arrows are
        # placed again below for the rows this look fits)
        removed += ('button_back', 'scroll_up_arrow', 'scroll_down_arrow')
        for parent in root.iter():
            for part in list(parent.findall('child')):
                if part.get('widget', '').rsplit('/', 1)[-1] in removed:
                    parent.remove(part)
    for element in list(root):
        if element.get('name', '').rsplit('/', 1)[-1] in removed:
            root.remove(element)
    for parent in root.iter():
        for row in list(parent.findall('child')):
            if row.get('widget', '').rsplit('/', 1)[-1] in XBOX_DROPPED_ROWS.get(folder, ()):
                parent.remove(row)
    for element in root.iter():
        for key, value in list(element.attrib.items()):
            if value.startswith(prefix):
                element.set(key, base + value[len(prefix):])
            elif key in ('bitmap', 'header_bitmap', 'footer_bitmap'):
                element.set(key, bitmap_ref(value, bitmaps))
        # Headers are PNG-only: these PC redraws already match the Xbox face.
        if element.tag == 'widget' and element.get('bitmap', '').startswith(base + '/header_'):
            original = prefix + element.get('bitmap')[len(base):]
            element.set('bitmap', bitmap_ref(original, bitmaps))
        if element.get('string_list') == 'strings/common_button_captions':
            raise ValueError('PC button captions escaped footer replacement')
        if element.get('bitmap') == 'xbox/bitmaps/option_bkds':
            element.set('height', str(ROW_HEIGHT))
        if element.get('name') == f'{base}/help' and folder != 'about':
            element.attrib.update({k: str(v) for k, v in help_box(HELP_TOP).items()})
        if element.get('name') == f'{base}/cloud_saves_status':
            element.set('top', '393')
            element.set('height', '20')
        if element.get('name') == f'{base}/options_menu':
            # Keep the PC platform-sharing slots. A page with more rows than fit
            # above its help scrolls them (visible_rows).
            first, pitch = (128 if folder == 'about' else FIRST_ROW), ROW_PITCH
            place = -1
            previous = None
            for row in element.findall('child'):
                if row.get('widget').endswith('/button_bar'):
                    continue
                old_y = row.get('y')
                if folder == 'about':
                    continue  # (already in this look's places: the rows below SEND LOG have two, by platform)
                if old_y != previous:
                    place += 1
                previous = old_y
                row.set('y', str(first + place * pitch))
            help_top = int(next(w for w in root.iter('widget') if w.get('name') == f'{base}/help').get('top'))
            if element.get('row_pitch'):
                element.set('row_top', str(first))
                element.set('row_pitch', str(pitch))
            pending = fit_window(element, {w.get('name'): w for w in root.iter('widget')}, help_top, root, bitmaps,
                                 always=bool(element.get('row_pitch')))
    if pending:
        pending()
    if folder == 'about':
        legend(root, root[0], base, accept='SELECT', cancel='BACK')
        send_log_confirmation(root, bitmaps)
        notices_screen(root, bitmaps)
        return root
    apply_now_spinners(root)
    if folder == 'controls_setup':
        for on in root.iter('on'):
            if on.get('run') == 'controls screen init':
                on.set('run', 'port controls init')
    if folder == 'video_settings':
        ET.SubElement(root[0], 'on', event='deleted', run='port settings close')
    footer(root, base,
           'port controls defaults' if folder == 'controls_setup' else 'port settings defaults apply')
    return root


def notices_screen(root, bitmaps):
    """The notices list shares settings_help's label-index lookup and the scrolling list's arrows."""
    base = 'xbox/settings/about'
    prefix = base + '/notices'
    screen = widget(root, base + '/notices_screen', width=640, height=480,
                    flags='pass_unhandled_to_focused_child pause_game',
                    bitmap=bitmap_ref('bitmaps/gradient', bitmaps))
    for event in ('b', 'back'):
        ET.SubElement(screen, 'on', event=event, back='true')
    child(screen, prefix + '/header')
    widget(root, prefix + '/header', controller=1, **title(),
           bitmap=bitmap_ref(f'{PE}/about/header_open_source_notices', bitmaps))
    child(screen, prefix + '/options_menu')
    options = widget(root, prefix + '/options_menu', type='column_list', width=640, height=480,
                     visible_rows=7, description=prefix + '/help',
                     flags='pass_unhandled_to_focused_child up_down_tabs_children')
    ET.SubElement(options, 'data', input='port settings help')
    widget(root, prefix + '/help', type='text', **help_box(HELP_TOP),
           string_list=prefix + '/help_strings', font=r'ui\small_ui', color='#FFFFFFFF')
    for index, _ in enumerate(about_data.LICENSES):
        row = prefix + f'/op_{index}'
        child(options, row, x=54, y=73 + index * 39)
        item = widget(root, row, width=512, height=36, bitmap=bitmap_ref('bitmaps/option_bkds', bitmaps),
                      color=BLUE, flags='pass_unhandled_to_focused_child')
        ET.SubElement(item, 'on', event='a')
        ET.SubElement(item, 'on', event='left_mouse', run='mouse emit accept event')
        child(item, row + '/label')
        child(item, row + '/value', x=250)
        widget(root, row + '/label', type='text', width=250, height=36,
               string_list=prefix + '/labels', string_index=index,
               font=r'ui\large_ui', color=BLUE, text_x=13, text_y=7)
        widget(root, row + '/value', type='text', width=252, height=36,
               string_list=prefix + '/values', string_index=index,
               font=r'ui\small_ui', color=BLUE, align='right', text_y=9,
               text_flags='no_focus_test')
    for direction, y in (('up', 73), ('down', 313)):
        name = prefix + f'/scroll_{direction}_arrow'
        child(options, name, x=ARROW_X, y=y)
        widget(root, name, width=16, height=16, bitmap=bitmap_ref(f'bitmaps/arrow_sm_{direction}', bitmaps))
    for name, texts in (
            ('labels', [entry[0] for entry in about_data.LICENSES]),
            ('values', [entry[1] for entry in about_data.LICENSES]),
            ('help_strings', [''] + ['\\n'.join(textwrap.wrap(entry[2], 64, break_long_words=False, break_on_hyphens=False)) for entry in about_data.LICENSES])):
        strings = ET.SubElement(root, 'strings', name=prefix + '/' + name)
        for text in texts:
            ET.SubElement(strings, 'string', text=text)
    legend(root, screen, prefix, accept=None, cancel='BACK')


def send_log_confirmation(root, bitmaps):
    """SEND LOG's question, laid out as the game's own confirmations are (as the Discord unlink one is): the title in
    the alert panel's header band, the message below it, B =CANCEL and A =SEND at its foot. B, CANCEL and Back go
    back with the row unchanged; SEND starts the upload and goes back"""
    base = 'xbox/settings/send_log_confirm'
    modal = widget(root, base, width=640, height=480, flags='pass_unhandled_to_focused_child pause_game',
                   bitmap=bitmap_ref('bitmaps/semi_transparent_grey', bitmaps))
    for event in ('b', 'back'):
        ET.SubElement(modal, 'on', event=event, back='true')
    child(modal, base + '/message', x=DIALOG_PANEL[0], y=DIALOG_PANEL[1])
    widget(root, base + '/message', type='text', width=DIALOG_PANEL[2], height=DIALOG_PANEL[3],
           bitmap=bitmap_ref('bitmaps/alert_bkd', bitmaps), text='Your log goes to the\\nVISR team via iCloud.',
           font=r'ui\large_ui', color=BLUE, text_x=9, text_y=36)
    child(modal, base + '/title', x=DIALOG_HEADER_POSITION[0], y=DIALOG_HEADER_POSITION[1])
    widget(root, base + '/title', type='text', width=170, height=20, text='SEND LOG?', font=r'ui\large_ui', color=BLUE)
    for event in ('a', 'start'):
        ET.SubElement(modal, 'on', event=event, run='port send log', back='true')
    dialog_legend_targets(root, modal, base + '/legend', cancel=True, accept='SEND')


def arrows_needed(options, root, bitmaps, first, fit, pitch=ROW_PITCH):
    """The arrows beside the first and last row that show while rows are
    hidden above or below (ui_widget.c names them). Returned as a call to make
    once the page's bitmap names are settled."""
    def add():
        base = options.get('name')
        for direction, y in (('up', first), ('down', first + (fit - 1) * pitch + 6)):
            name = f'{base}/scroll_{direction}_arrow'
            widget(root, name, width=16, height=16, bitmap=bitmap_ref(f'bitmaps/arrow_sm_{direction}', bitmaps))
            child(options, name, x=ARROW_X, y=y)
    return add


def fit_window(options, widgets, help_top, root=None, bitmaps=None, always=False, pitch=None):
    """Show as many of a list's rows as fit above its help; the rest scroll.
    always: a settings list keeps its window when every row fits, so that
    rows its code hides close up (ui_widget.c)."""
    rows = [c for c in options.findall('child')
            if c.get('y') and 'button_bar' not in c.get('widget') and c.get('widget') in widgets
            and not c.get('widget').rsplit('/', 1)[-1].startswith('scroll_')]
    if not rows:
        return
    slots = sorted({int(c.get('y')) for c in rows})
    if pitch is None:
        pitch = slots[1] - slots[0] if len(slots) > 1 else ROW_PITCH
    height = max(int(widgets[c.get('widget')].get('height', 0)) for c in rows)
    fit = max(1, (help_top - height - slots[0]) // pitch + 1)
    if len(slots) > fit:
        options.set('visible_rows', str(fit))
        if root is not None:
            return arrows_needed(options, root, bitmaps, slots[0], fit, pitch)
    elif always:
        options.set('visible_rows', str(fit))
    else:
        options.attrib.pop('visible_rows', None)


IO = 'main_menu/settings_select/multiplayer_setup/item_options_edit'
# the Item Options rows the PC version's port added (map weapons, loadout and the two weapons) and their labels
IO_ROWS = (('op_spawn_weapons', 'spawn_weapons_label', 'STARTING EQUIPMENT:'),
           ('op_map_weapons', 'map_weapons_label', 'MAP WEAPONS:'),
           ('op_choose_weapons', 'choose_weapons_label', 'CHOOSE WEAPONS'))


# the Player Options row the port adds (RESPAWN, opening RESPAWN OPTIONS: the four respawn rows moved there)
PO = 'main_menu/settings_select/multiplayer_setup/player_options_edit'
PO_ROWS = (('op_respawn', 'respawn_label', 'RESPAWN'),)
PO_HELP = {'op_respawn': 'The respawn time and how it grows, odd man out,\\nand the suicide penalty.'}

WEAPONS = ('no weapon', 'a random weapon', 'the assault rifle', 'the pistol', 'the shotgun', 'the sniper rifle',
           'the rocket launcher', 'the plasma pistol', 'the plasma rifle', 'the needler')
# what the help text shows for each value of a row, in the order of its spinner's values
IO_HELP = {
    'op_map_weapons': 'Weapons are lying around the map\\nto pick up.|There are no weapons lying around\\nthe map.',
    'op_spawn_weapons': "You will start the game with the weapon set that the\\nmap designers custom tailored for it.|Use the generic starting weapons across\\nall maps.|Everyone starts with the primary and\\nsecondary weapons you choose.",
    'op_choose_weapons': 'Choose the primary and secondary weapons\\neveryone starts with.',
}


def item_options_document(assets, bitmaps, folder=None, io_rows=None):
    """The PC port's extra Item Options rows as widgets of their own, which patch rows put in the Xbox's screen.

    The Xbox's functions for the screen know only its own rows, so each row sets and saves its spinners itself.
    """
    folder, io_rows = folder or IO, io_rows or IO_ROWS
    root = source(folder.replace('/', '.') + '.port.xml')
    labels = {f'xbox/{folder}/{label}': caption for _, label, caption in io_rows}
    for element in root.iter():
        for key, value in list(element.attrib.items()):
            if value.startswith(folder) or (key == 'open' and value.startswith(MP + '/')):
                element.set(key, 'xbox/' + value)
            elif key in ('bitmap', 'header_bitmap', 'footer_bitmap'):
                element.set(key, bitmap_ref(value, bitmaps))
        if element.get('name') in labels:
            element.attrib.pop('string_list')
            element.attrib.pop('string_index')
            element.set('text', labels[element.get('name')])
    rows = {f'xbox/{folder}/{name}' for name, _, _ in io_rows}
    for row in root.findall('widget'):
        if row.get('name') in rows:
            ET.SubElement(row, 'on', event='created', run='port option row init')
            ET.SubElement(row, 'on', event='deleted', run='port option row save')
    return root


# The settings pages' rows, in the order both lists show them: key, caption, folder, screen, platform and the
# description shown beside the row (each ends as the Xbox's do, where the profile's name is shown). The profile edit
# screen's patch rows and the in-game hub use the same ones.
CURRENT = '\\n\\nCurrent Profile:'
PAGE_ROWS = (
    ('controls', 'CONTROLS', 'gamepad_setup', 'gamepad_setup_screen', None,
     'Look, sensitivity, vibration and\\nthe button layout.' + CURRENT),
    ('video', 'VIDEO', 'video_settings', 'video_settings_screen', 'mac_app',
     'Frame rate, resolution, and how\\nthe picture is drawn.' + CURRENT),
    # (the Mac's VIDEO screen has this mode as its first row; in the hub, the panel's status line names the mode
    # while this row is chosen: HUB_DESCRIPTIONS, "port hub status")
    ('graphics', 'GRAPHICS', 'graphics', 'graphics_screen', '!mac_app',
     # (a blank line more than the one-line text needs: the profile's name sits beside "Current Profile:" as on the
     # other rows, whose text is two lines)
     "The game's look.\\n" + CURRENT),
    ('audio', 'AUDIO', 'audio_settings', 'audio_settings_screen', None,
     'Volume and sound for the\\ngame on this device.' + CURRENT),
    ('network', 'NETWORK', 'network_setup', 'network_settings_screen', None,
     'Online play, custom maps\\nand player names.' + CURRENT),
    ('keyboard', 'KEYBOARD & MOUSE', 'controls_setup', 'controls_settings_screen', 'keyboard',
     'The keys and mouse buttons\\nfor each action.' + CURRENT),
)
# The picture slot. Each row names its picture once, by its key: the shipped redraw art/<key>.png where there is
# one, else the player's own copy (the profile editor's picture, saved from their ui.map into the menus folder at
# CACHE/<key>.png), else nothing: NONE_PNG, which is transparent, so the panel is drawn text-only. menu_files.c
# tries a frame's cache before its png, so a frame is "cache + png" only where no redraw is shipped; a redraw added
# later takes the place by regenerating (test_generation_is_deterministic_and_checked_in).
ART = 'xbox/art'
# the title icon's bitmap (its name is menu_tags.c's MENU_TITLE_ICON) and its size, the picture's size in menu units (its drawn helmet is as tall as the
# titles' capitals, 22 units)
TITLE_ICON, TITLE_ICON_SIZE = 'xbox/bitmaps/visr_title_icon', 36
# Bump this directory to rebuild saved pictures after the decode or save format changes.
CACHE = 'cache/xbox/v1'
NONE_PNG = f'{ART}/none.png'
PICTURE_SIZE = (512, 256)


# where ui.map has each row's picture (the profile editor's own frames), which the menus save into the cache the
# first main-menu update (pc_menu_art_extract): the profile editor's Controller Setup and Change Name
# pictures, and the multiplayer type screen's diagrams
PICTURE_SOURCES = {
    'controls': (r'ui\shell\main_menu\settings_select\player_setup\player_profile_edit\profile_options', 1),
    'video': (r'ui\shell\main_menu\multiplayer_type_select\mp_options', 0),
    'network': (r'ui\shell\main_menu\multiplayer_type_select\mp_options', 2),
    'keyboard': (r'ui\shell\main_menu\settings_select\player_setup\player_profile_edit\profile_options', 0),
}


# the rows whose picture ships as our own art (xbox/art/<name>.png, from tools/xbox_menu_art.py), by the name it has:
# NETWORK shows the INTERNET picture
REDRAW_NAMES = {'video': 'video', 'audio': 'audio', 'network': 'internet', 'graphics': 'none', 'profiles': 'none'}


def picture_frame(assets, key):
    """The attributes of a row's picture frame: the redraw if shipped, else the map's own (saved to the cache where
    ui.map is loaded) over the cache and the transparent png."""
    width, height = PICTURE_SIZE
    redraw = f'{ART}/{REDRAW_NAMES.get(key, key)}.png'
    if (assets / redraw).is_file() or key not in PICTURE_SOURCES:
        return {'png': redraw, 'width': str(width), 'height': str(height)}
    tag, index = PICTURE_SOURCES[key]
    return {'map': tag, 'index': str(index), 'cache': f'{CACHE}/{key}.png', 'png': NONE_PNG,
            'width': str(width), 'height': str(height)}


# the hub rows' places (a row filtered out by its platform leaves its slot to the row after)
HUB_SLOTS = {'controls': 0, 'video': 1, 'graphics': 1, 'audio': 2, 'network': 3, 'profiles': 4, 'keyboard': 5}
# PROFILES is a hub-only row: it opens the profile picker (menu_tags.c leaves it unredirected while the hub is up), which keeps no row of its own for it
PROFILES_ROW = ('profiles', 'PROFILES', None, None, None, 'Choose, create, and edit\\nplayer profiles.' + CURRENT)
HUB_ROWS = PAGE_ROWS[:5] + (PROFILES_ROW,) + PAGE_ROWS[5:]
PROFILES_OPEN = 'ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_select_screen'
# a hub row whose panel text ends with another status line than "Current Profile:": GRAPHICS names its mode there
# (menu_functions.c's "port hub status" shows the mode's word, in place of the profile's name, while it is chosen)
HUB_DESCRIPTIONS = {'graphics': "The game's look.\\n\\n\\nGraphics:"}
GRAPHICS_STATUS_X = 384


def hub_documents(assets):
    """The hub's descriptions and pictures, which are each filtered by platform like the rows are, so a hidden row
    leaves the strings and frames in the same places as the rows."""
    strings = ET.Element('strings', name='xbox/settings/hub_descriptions')
    bitmap = ET.Element('bitmap', name='xbox/settings/hub_pictures')
    for key, _, _, _, platform, description in HUB_ROWS:
        attrs = {'platform': platform} if platform else {}
        ET.SubElement(strings, 'string', text=HUB_DESCRIPTIONS.get(key, description), **attrs)
        ET.SubElement(bitmap, 'frame', **picture_frame(assets, key), **attrs)
    return strings, bitmap


def bitmap_ref(name, bitmaps):
    bitmaps.add(name)
    return f'xbox/{name}'


def settings_hub(bitmaps, assets=MENUS):
    """The in-game settings hub: the profile editor's layout (the list, and a framed panel with the focused row's
    picture and description, as the main menu's profile editor has it), from the PC profile editor's widgets."""
    root = ET.Element('menus')
    base = 'xbox/settings'
    strings, pictures = hub_documents(Path(assets))
    root.append(strings)
    root.append(pictures)
    screen = widget(root, f'{base}/settings_screen', width=640, height=480,
                    flags='pass_unhandled_to_focused_child pause_game',
                    bitmap=bitmap_ref('bitmaps/gradient', bitmaps))
    for event in ('b', 'back'):
        ET.SubElement(screen, 'on', event=event, back='true')
    child(screen, f'{base}/header')
    child(screen, f'{base}/settings_list')
    child(screen, f'{base}/profile_label', x=432, y=353)
    child(screen, f'{base}/graphics_label', x=GRAPHICS_STATUS_X, y=353, platform='!mac_app')
    header = f'{PE}/header_edit_profile_settings'
    widget(root, f'{base}/header', **title(),
           bitmap=bitmap_ref(header, bitmaps))
    label = widget(root, f'{base}/profile_label', type='text', width=200, height=22, font=r'ui\large_ui',
                   color='#FFFFFFFF')
    ET.SubElement(label, 'data', input='get edit plyr profile name')
    # the mode's word beside "Graphics:", in the profile name's place while GRAPHICS is chosen
    status = widget(root, f'{base}/graphics_label', type='text', width=200, height=22, font=r'ui\large_ui',
                    color='#FFFFFFFF')
    ET.SubElement(status, 'data', input='port graphics state')
    # The panel: the frame, then (in this order, which the list's description update expects) the text and the
    # picture, at the Xbox's places
    description = widget(root, f'{base}/description', **panel(PANEL_A),
                   bitmap=PANEL_BITMAPS['a'])
    child(description, f'{base}/description_text')
    child(description, f'{base}/description_picture', x=4)
    widget(root, f'{base}/description_text', type='text', left=294, top=290, width=296, height=95,
           string_list=f'{base}/hub_descriptions', font=r'ui\large_ui', color=BLUE)
    widget(root, f'{base}/description_picture', left=288, top=85, width=279, height=202,
           bitmap=f'{base}/hub_pictures')
    options = widget(root, f'{base}/settings_list', type='column_list', width=640, height=480,
                     flags='pass_unhandled_to_focused_child up_down_tabs_children', visible_rows=6,
                     description=f'{base}/description')
    ET.SubElement(options, 'data', input='plyr prof edit select menu upd8')
    ET.SubElement(options, 'data', input='port hub status')
    for key, caption, folder, screen_name, platform, _ in HUB_ROWS:
        attrs = {'platform': platform} if platform else {}
        # VIDEO, GRAPHICS and AUDIO share a slot so filtering them preserves the row pitch.
        child(options, f'{base}/{key}', x=PANEL_A['list'][0], y=PANEL_A['list'][1] + HUB_SLOTS[key] * PANEL_A['list'][2], **attrs)
        row = text_widget(root, f'{base}/{key}', caption, width=232, height=32,
                          bitmap=bitmap_ref('bitmaps/list_item_bkd', bitmaps), text_x=13, text_y=4)
        for event in ('a', 'start'):
            ET.SubElement(row, 'on', event=event,
                          open=PROFILES_OPEN if key == 'profiles' else f'xbox/settings/{folder}/{screen_name}')
    legend(root, screen, base, cancel='BACK')
    return root


# The gametype editor (EDIT MULTIPLAYER GAMETYPE): the screen the game's gametype list and SERVER SETUP open to edit
# one gametype, in place of the game's own (SCREEN_ALWAYS). Its rows, in order: key, caption, the screen the row
# opens, the function run first, platform, the picture (the map's bitmap and frame) and the description. The
# game's rows keep their pictures and descriptions (the map's playlist_edit_list_extended_dsc_text, whose last
# line names the gametype beside gametype_label); GAME OPTIONS, the game's GAME TYPE row, opens the shared game
# settings page, whose TYPE rows reach each gametype's rules.
GAMETYPE_EDIT = 'xbox/main_menu/settings_select/multiplayer_setup/playlist_edit'
GAMETYPE_EDIT_SCREEN = f'{GAMETYPE_EDIT}/gametype_edit_screen'
MP_SETUP = 'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\'
GAMETYPE_OPTIONS_PICTURES = MP_SETUP + 'playlist_edit\\gametype_options'
GAMETYPE_PICTURE = ('ui\\shell\\main_menu\\multiplayer_type_select\\mp_options', 3)
THIS_GAMETYPE = '\\n\\nThis gametype:'
GAMETYPE_EDIT_ROWS = (
    ('name', 'CHANGE NAME', MP_SETUP + 'name_edit\\gametype_name_screen',
     'mp profile change name', None, (GAMETYPE_OPTIONS_PICTURES, 0),
     'Rename this gametype using\\nthe virtual keyboard.\\n' + THIS_GAMETYPE),
    ('game_options', 'GAME OPTIONS', 'xbox/main_menu/multiplayer_type_select/game_options_edit/game_options_screen',
     None, None, (GAMETYPE_OPTIONS_PICTURES, 1),
     'Specify the type of game you\\nwant to play and the rules\\nthat apply to it.' + THIS_GAMETYPE),
    ('player_options', 'PLAYER OPTIONS', MP_SETUP + 'player_options_edit\\player_options_screen',
     None, None, (GAMETYPE_OPTIONS_PICTURES, 2),
     'Choose the player settings and\\nrestrictions that apply to\\nthis gametype.' + THIS_GAMETYPE),
    ('item_options', 'ITEM OPTIONS', MP_SETUP + 'item_options_edit\\item_options_screen',
     None, None, (GAMETYPE_OPTIONS_PICTURES, 3),
     'Choose the weapons, vehicles\\nand other items that appear in\\nthis gametype.' + THIS_GAMETYPE),
    ('vehicles', 'VEHICLES', 'xbox/main_menu/settings_select/multiplayer_setup/vehicle_options_edit/vehicle_options_screen',
     None, None, GAMETYPE_PICTURE, 'Which vehicles are on the map.\\n\\n' + THIS_GAMETYPE),
    ('save', 'SAVE CHANGES', 'ui\\shell\\error\\warning_saving_file_screen',
     'mp profile save changes', None, (GAMETYPE_OPTIONS_PICTURES, 5),
     "Save changes you've made to\\nthis gametype and return to\\nthe previous screen." + THIS_GAMETYPE),
    # (the Apple TV's: its Siri Remote has no Y button for the gametype list's DELETE)
    ('delete', 'DELETE GAMETYPE', 'xbox/error/confirm_delete_gametype_modal_fullscreen',
     'port delete request', 'tvos', GAMETYPE_PICTURE, 'Delete this gametype.\\n\\n' + THIS_GAMETYPE),
)


def gametype_editor(bitmaps):
    """The gametype editor: the settings hub's layout (a list of tall picture rows, and the framed panel with the
    focused row's picture and description, the gametype's name on its last line), with the game's title."""
    root = ET.Element('menus')
    base = GAMETYPE_EDIT
    strings = ET.SubElement(root, 'strings', name=f'{base}/descriptions')
    pictures = ET.SubElement(root, 'bitmap', name=f'{base}/pictures')
    width, height = PICTURE_SIZE
    for _, _, _, _, platform, (tag, index), description in GAMETYPE_EDIT_ROWS:
        attrs = {'platform': platform} if platform else {}
        ET.SubElement(strings, 'string', text=description, **attrs)
        ET.SubElement(pictures, 'frame', map=tag, index=str(index), png=NONE_PNG, width=str(width),
                      height=str(height), **attrs)
    screen = widget(root, GAMETYPE_EDIT_SCREEN, width=640, height=480, flags='pass_unhandled_to_focused_child',
                    bitmap=bitmap_ref('bitmaps/gradient', bitmaps))
    # (the game's: the screen closes at once unless a gametype is being edited)
    ET.SubElement(screen, 'on', event='created', run='close if not editing profile')
    for event in ('b', 'back'):
        ET.SubElement(screen, 'on', event=event, back='true')
    child(screen, f'{base}/header')
    child(screen, f'{base}/gametype_list')
    child(screen, f'{base}/gametype_label', x=433, y=360)
    # (the game's title, 512 by 32, at the top of the standard title box's picture)
    widget(root, f'{base}/header', **title(),
           bitmap=bitmap_ref('main_menu/settings_select/multiplayer_setup/playlist_edit/header_edit_multiplayer_gametype',
                             bitmaps))
    label = widget(root, f'{base}/gametype_label', type='text', width=160, height=22, font=r'ui\large_ui',
                   color='#FFFFFFFF')
    ET.SubElement(label, 'data', input='get edit game settings name')
    # The panel: the frame, then the text and the picture (the order the list's description update expects), at
    # the game's places for this screen, whose descriptions run a line longer than the hub's
    description = widget(root, f'{base}/description', **panel(PANEL_A), bitmap=PANEL_BITMAPS['a'])
    child(description, f'{base}/description_text')
    child(description, f'{base}/description_picture')
    widget(root, f'{base}/description_text', type='text', left=294, top=276, width=296, height=124,
           string_list=f'{base}/descriptions', font=r'ui\large_ui', color=BLUE)
    widget(root, f'{base}/description_picture', left=290, top=80, width=284, height=208, bitmap=f'{base}/pictures')
    options = widget(root, f'{base}/gametype_list', type='column_list', width=640, height=480,
                     flags='pass_unhandled_to_focused_child up_down_tabs_children', description=f'{base}/description')
    ET.SubElement(options, 'data', input='plyr prof edit select menu upd8')
    x, y, pitch = PANEL_A['list']
    for index, (key, caption, opens, run, platform, _, _) in enumerate(GAMETYPE_EDIT_ROWS):
        attrs = {'platform': platform} if platform else {}
        child(options, f'{base}/{key}', x=x, y=y + index * pitch, **attrs)
        art = 'bitmaps/list_item_bkd_top' if index == 0 else 'bitmaps/list_item_bkd'
        row = text_widget(root, f'{base}/{key}', caption, width=232, height=32, bitmap=bitmap_ref(art, bitmaps),
                          text_x=13, text_y=4)
        for event in ('a', 'start'):
            handler = ET.SubElement(row, 'on', event=event)
            if run:
                handler.set('run', run)
            handler.set('open', opens)
    legend(root, screen, base)
    return root


def bitmap_document(assets, names):
    source_bitmaps = {b.get('name'): b for b in source('bitmaps.xml')}
    root = ET.Element('menus')
    for name in sorted(names | {f'pause/pausebox_{side}' for side in ('left', 'center', 'right')} | {'pause/pause_row'}):
        if '\\' in name:
            bitmap = ET.SubElement(root, 'bitmap', name='xbox/map/' + name.replace('\\', '/'))
            ET.SubElement(bitmap, 'frame', map=name, index='0',
                          png='pictures/shell/bitmaps/game_type_grafix__5.png', width='256', height='128')
            continue
        bitmap = deepcopy(source_bitmaps[name])
        if not name.startswith('pause/'):
            bitmap.set('name', 'xbox/' + name)
        for index, frame in enumerate(bitmap):
            if name.startswith('bitmaps/') and name.split('/')[-1] in MAP_BITMAPS:
                frame.set('map', 'ui\\shell\\' + name.replace('/', '\\'))
                frame.set('index', str(index))
        root.append(bitmap)
    for key, panel_box in (('a', PANEL_A['panel']), ('b', PANEL_B['panel'])):
        bitmap = ET.SubElement(root, 'bitmap', name=PANEL_BITMAPS[key])
        width, height = panel_frame_size(panel_box)
        ET.SubElement(bitmap, 'frame', map='ui\\shell\\bitmaps\\list_field', index='0',
                      png='pictures/shell/bitmaps/list_field__0.png', width=str(width), height=str(height))
    # Borrow the Multiplayer diagrams and gametype figure through single-frame bitmaps.
    for name, index in (('gametype', 3),):
        bitmap = ET.SubElement(root, 'bitmap', name=f'xbox/pictures/{name}')
        ET.SubElement(bitmap, 'frame', map=r'ui\shell\main_menu\multiplayer_type_select\mp_options',
                      index=str(index), png=NONE_PNG,
                      width='460', height='230')
    # The VISR helmet left of every screen's title (drawn by tools/xbox_menu_art.py; placed by the game,
    # menu_title_icon_draw in port/linux/game/menu_tags.c, beside each title picture of both looks)
    bitmap = ET.SubElement(root, 'bitmap', name=TITLE_ICON)
    ET.SubElement(bitmap, 'frame', png=f'{ART}/visr_title_icon.png', width=str(TITLE_ICON_SIZE),
                  height=str(TITLE_ICON_SIZE))
    for name, width, height, count in [('dropdown_chevron', 12, 8, 1),
                                        ('dropdown_check', 12, 12, 1),
                                        ('dropdown_panel', 165, 0, 7),
                                        ('slider_track', 1, 5, 1),
                                        ('slider_fill', 1, 5, 1),
                                        ('slider_thumb', 25, 25, 1)]:
        bitmap = ET.SubElement(root, 'bitmap', name=f'xbox/bitmaps/{name}')
        for frame in range(count):
            suffix = f'__{frame}' if count > 1 else ''
            ET.SubElement(bitmap, 'frame', png=f'xbox/art/{name}{suffix}.png', width=str(width),
                          height=str((frame + 1) * DROPDOWN_PITCH + 3 if count > 1 else height))
    bitmap = ET.SubElement(root, 'bitmap', name='xbox/bitmaps/toggle_switch')
    for on in (0, 1):
        ET.SubElement(bitmap, 'frame', png=f'xbox/art/toggle_switch__{on}.png',
                      width=str(menu_geometry.SWITCH_WIDTH), height=str(menu_geometry.SWITCH_HEIGHT))
    return root


def multiplayer_documents(assets, bitmaps):
    """Copy wired PC definitions and their XML dependencies into the Xbox set.

    Keep leaf names: menu_functions and menu tap find rows and values by name.
    Native editor opens are the only map-widget references (never children).
    """
    definitions = {}
    for name in source_names():
        for element in source(name):
            if element.get('name'):
                definitions[element.tag, element.get('name')] = element
    documents = {}
    owned = set()
    for filename in MP_PAGES:
        root = source(filename + '.xml')
        documents[filename + '.xml'] = root
        owned.update((e.tag, e.get('name')) for e in root)
    # new_select.port supplies the chooser, not a screen. The lobby borrows the PC solo list's map picture and
    # name widgets.
    campaign = documents['main_menu.new_select.port.xml']
    for e in source('main_menu.solo_level_select.xml'):
        if e.get('name') in LOBBY_MAP_WIDGETS:
            campaign.append(deepcopy(e))
            owned.add((e.tag, e.get('name')))
    # The Map screen (hosting an Internet, Direct Link or system link game, or a co-op one): the PC list with its
    # SINGLEPLAYER or MULTIPLAYER chooser in the first row, in the Xbox list's look, inside the wrapper the
    # browser's CREATE GAME opens (the game's own list has no chooser, so no campaign level could be hosted)
    maps = documents['main_menu.multiplayer_type_select.mp_map_select.xml']
    wrapper = definitions['widget', MP + '/connected/connected_map_select_wrapper']
    maps.append(deepcopy(wrapper))
    # (the legend's A and B replace the PC list's OK and BACK bar, which would draw under it)
    for e in maps:
        if e.get('name', '').endswith('/mp_map_select_list_2'):
            for c in e.findall('child'):
                if c.get('widget', '').endswith('mp_map_select_button_bar'):
                    e.remove(c)
            # (left and right turn the chooser from any row: it is the first row, scrolled away from)
            ET.SubElement(e, 'on', event='left', run='mp map kind previous')
            ET.SubElement(e, 'on', event='right', run='mp map kind next')
    owned.add(('widget', wrapper.get('name')))
    shared = ET.Element('menus')
    shared.append(deepcopy(definitions['widget', 'error/header_saving']))
    owned.add(('widget', 'error/header_saving'))
    documents['multiplayer.shared.xml'] = shared
    # Xbox item options combine vehicles with weapons; these two PC-only
    # editors still need their wired XML screens until routing is integrated.
    for folder in ('vehicle_options_edit',
                   'playlist_edit.ctf_edit', 'playlist_edit.koth_edit', 'playlist_edit.oddball_edit',
                   'playlist_edit.race_edit', 'playlist_edit.slayer_edit'):
        filename = 'main_menu.settings_select.multiplayer_setup.' + folder + '.xml'
        for e in source(filename):
            shared.append(deepcopy(e))
            owned.add((e.tag, e.get('name')))
    # Match the Xbox modal: fixed button art inside the alert, with A handled by the modal itself. The join
    # warning is one of them: its legend shows A beside OK and B beside CANCEL, so A always joins and B (or Back)
    # always cancels, whatever was focused (a focusable CANCEL took A as cancel).
    for kind, message, accept, cancel in (
            ('delete_profile', 24, {'run': 'port delete finish'}, {'run': 'cancel profile delete', 'back': 'true'}),
            ('delete_gametype', 25, {'run': 'port delete finish'}, {'run': 'cancel profile delete', 'back': 'true'}),
            ('join_big', 60, {'run': 'port browser join big confirm'}, {'run': 'port browser join big back'})):
        base = 'error/confirm_' + kind
        modal = widget(shared, base + '_modal_fullscreen', width=640, height=480,
                       flags='pass_unhandled_to_focused_child')
        for event in ('a', 'start'):
            ET.SubElement(modal, 'on', event=event, **accept, sound=r'sound\sfx\ui\forward')
        for event in ('b', 'back'):
            ET.SubElement(modal, 'on', event=event, **cancel, sound=r'sound\sfx\ui\back')
        panel = widget(shared, base, width=640, height=480, bitmap='bitmaps/semi_transparent_grey')
        child(modal, base)
        child(panel, base + '_frame', x=DIALOG_PANEL[0], y=DIALOG_PANEL[1])
        child(panel, base + '_text_box', x=DIALOG_PANEL[0], y=DIALOG_PANEL[1])
        child(panel, 'error/header_warning', x=DIALOG_HEADER_POSITION[0], y=DIALOG_HEADER_POSITION[1])
        widget(shared, base + '_frame', width=DIALOG_PANEL[2], height=DIALOG_PANEL[3], bitmap='bitmaps/alert_bkd')
        widget(shared, base + '_text_box', type='text', width=DIALOG_PANEL[2], height=DIALOG_PANEL[3],
               string_list='strings/displayed_error_messages', string_index=message,
               font=r'ui\large_ui', color=BLUE, text_x=9, text_y=36)
        owned.update((e.tag, e.get('name')) for e in shared)
    # dialogs.xml defines the Xbox wait dialogs; screens that show one (the profile manager) only refer to it.
    owned.update(('widget', 'error/' + name) for name in WAIT_DIALOGS)
    pending = [e for root in documents.values() for e in root]
    while pending:
        element = pending.pop()
        for e in element.iter():
            for key, tag in (('widget', 'widget'), ('description', 'widget'), ('string_list', 'strings')):
                name = e.get(key)
                if not name or '\\' in name or (tag, name) in owned:
                    continue
                copied = deepcopy(definitions[tag, name])
                shared.append(copied)
                pending.append(copied)
                owned.add((tag, name))
    for root in documents.values():
        for e in root.iter():
            for key in ('name', 'widget', 'description', 'string_list'):
                if e.get(key) and '\\' not in e.get(key):
                    e.set(key, 'xbox/' + e.get(key))
            if e.get('open'):
                target = e.get('open')
                e.set('open', 'xbox/' + target if ('widget', target) in owned else
                      'ui\\shell\\' + target.replace('/', '\\'))
            for key in ('bitmap', 'header_bitmap', 'footer_bitmap'):
                if e.get(key):
                    value = e.get(key)
                    if value.startswith('xbox/browser/'):
                        continue
                    if '\\' in value:
                        # Map previews have no shipped redraw. Reuse the shipped
                        # unknown-gametype card when that map tag is absent.
                        name = 'map/' + value.replace('\\', '/')
                        bitmaps.add(value)
                        e.set(key, 'xbox/' + name)
                    else:
                        e.set(key, bitmap_ref(value, bitmaps))
            if e.tag == 'widget':
                if e.get('color'):
                    e.set('color', '#FFFFFFFF' if 'help' in e.get('name') else BLUE)
                if e.get('bitmap') == 'xbox/bitmaps/text_button_background':
                    e.attrib.pop('bitmap')
                    e.set('color', BLUE)
                if e.get('bitmap') == 'xbox/bitmaps/gradient_big':
                    e.set('bitmap', bitmap_ref('bitmaps/gradient', bitmaps))
    widgets = {e.get('name'): e for root in documents.values() for e in root if e.tag == 'widget'}

    for page in ('server_settings', 'coop_options'):
        hosting_layout(documents[f'main_menu.multiplayer_type_select.{page}.xml'], widgets, page)

    # A static option list with more rows than fit above its help scrolls
    # them (visible_rows); its height is the room above the help.
    for w in widgets.values():
        if w.get('type') == 'column_list' and 'up_down_tabs_children' in w.get('flags', ''):
            limit = 400
            desc = widgets.get(w.get('description'))
            if desc is not None and desc.get('type') == 'text':
                if desc.get('top') == '366':
                    desc.set('top', '350')
                    desc.set('height', '60')
                limit = int(desc.get('top'))
            w.set('height', str(limit))
            if w.get('name') in HOSTED_LISTS:
                for c in w.findall('child'):
                    if c.get('y') and 'button_bar' not in c.get('widget'):
                        # Preserve the co-op/competitive shared slots, using
                        # advanced_controls' 73 origin and 30-unit pitch.
                        c.set('y', str(FIRST_ROW + (int(c.get('y')) - FIRST_ROW) // 24 * ROW_PITCH))
                pending = fit_window(w, widgets, limit, next(r for r in documents.values() if w in list(r)), bitmaps)
                if pending:
                    pending()
        for c in w.findall('child'):
            leaf = c.get('widget', '').rsplit('/', 1)[-1]
            if 'button_bar' in leaf:
                c.set('y', '449')

    browser_layout(documents['main_menu.multiplayer_type_select.join_game.xml'], widgets, bitmaps)
    quit_layout(documents['main_menu.quit_select.xml'], widgets)
    map_screen_layout(documents['main_menu.multiplayer_type_select.mp_map_select.xml'], widgets,
                      documents['multiplayer.shared.xml'])
    # The PC roster needs eleven slots. Use the advanced-controls row pitch
    # with the server-list column, and the connected pregame's right column.
    # Unlike static options, the campaign's data function scrolls its entries.
    selection_layout(widgets, bitmaps)
    gametype_select_layout(documents['main_menu.settings_select.multiplayer_setup.playlist_select.xml'], widgets, bitmaps)
    for filename in ('main_menu.multiplayer_type_select.lobby.xml', 'main_menu.new_select.port.xml',
                     'main_menu.multiplayer_type_select.mp_map_select.xml'):
        root = documents[filename]
        for w in list(root.findall('widget')):
            if w.get('name', '').endswith('_screen'):
                legend(root, w, w.get('name') + '/legend', cancel='BACK')
        # The legend already carries SELECT / BACK, so the shared OK / BACK bar
        # that the profile list places under it would draw a second row.
        for parent in root.iter('widget'):
            for c in list(parent.findall('child')):
                if c.get('widget', '').endswith('/player_2_button_bar'):
                    parent.remove(c)
    for page in ('server_settings', 'coop_options'):
        root = documents['main_menu.multiplayer_type_select.' + page + '.xml']
        apply_now_spinners(root)
        if page == 'coop_options':
            base = 'xbox/' + MP + '/coop_options'
            for w in list(root.findall('widget')):
                if w.get('name') in (base + '/button_bar', base + '/button_ok'):
                    root.remove(w)
            # The merged rows span config keys, so "coop options changed" writes them (menu_functions.c).
            for leaf in ('friendly_fire_spinner', 'extra_enemies_spinner'):
                spinner = root.find(f"widget[@name='{base}/{leaf}']")
                for event in ('left', 'right', 'stick_left', 'stick_right'):
                    spinner.append(ET.fromstring(f'<on event="{event}" run="coop options changed"/>'))
            for w in root.iter('widget'):
                for c in list(w.findall('child')):
                    if c.get('widget') == base + '/button_bar':
                        w.remove(c)
    for filename in ('join_game.filters', 'join_game.port', 'direct_ip', 'server_settings', 'coop_options'):
        root = documents['main_menu.multiplayer_type_select.' + filename + '.xml']
        for w in list(root.findall('widget')):
            name = w.get('name', '')
            if name.endswith(('/password_screen', '/code_screen')):
                # The screen's own button bar is hidden; its declared bar below
                # carries CANCEL and JOIN GAME (Enter in the field joins too).
                kind = 'password' if name.endswith('/password_screen') else 'code'
                # The bar: A follows the selected row (menu_functions.c's
                # legend_accept_caption): EDIT on the field, JOIN GAME on the
                # JOIN GAME row, like SERVER SETUP's START GAME; the focused row
                # takes it. B is the screen's own back (port <kind> back). The
                # password's X hides or shows the text (plain by default); its
                # key is a letter while typing, so only a tap or a click runs it
                # then.
                actions = [('b', 'back', 'CANCEL', {'focus': 'true'}),
                           ('a', 'select', ['EDIT', 'JOIN GAME'], {'focus': 'true', 'index': '1'})]
                if kind == 'password':
                    actions.insert(0, ('x', 'global', ['HIDE', 'SHOW'],
                                       {'run': 'port password togglehide', 'while_typing': 'pointer'}))
                bar(w, actions)
                # JOIN GAME is a row below the field (SERVER SETUP's START GAME
                # style); A activates whichever row is selected.
                dir_ = name.rsplit('/', 1)[0]
                options = next(t for t in root.iter('widget') if t.get('name') == dir_ + '/options_menu')
                field = options.find('child')
                join = next(t for t in root.iter('widget') if t.get('name') == dir_ + '/button_ok')
                for key in ('align',):
                    join.attrib.pop(key, None)
                join.attrib.update(width='512', height=str(ROW_HEIGHT), bitmap='xbox/bitmaps/option_bkds',
                                   font=r'ui\large_ui', color=BLUE, text_x='13', text_y='4')
                field.set('y', str(FIELD_Y))
                options.set('height', str(FIELD_Y + 39 + 36))
                child(options, dir_ + '/button_ok', x=54, y=FIELD_Y + 39)
                help_ = next(t for t in root.iter('widget') if t.get('name') == dir_ + '/help')
                help_.set('top', str(FIELD_Y + 39 + 36))
                for parent in root.iter('widget'):
                    for c in list(parent.findall('child')):
                        if c.get('widget', '').endswith('/button_bar') and \
                                c.get('widget', '').startswith(name.rsplit('/', 1)[0]):
                            parent.remove(c)
                continue
            if name.endswith('_screen') and filename == 'direct_ip':
                legend(root, w, name + '/legend', accept='SELECT', cancel='CANCEL')
                direct_ip_layout(root, name.rsplit('/', 1)[0])
                continue
            if name.endswith('_screen'):
                # (co-op options apply as they change: no A action, so no A in the legend)
                # (the filters keep the original bar's lower line, y 430: they convert with the dense bars)
                legend(root, w, name + '/legend',
                       accept=None if filename == 'coop_options' else 'SELECT',
                       cancel='BACK' if filename in ('server_settings', 'coop_options') else 'CANCEL',
                       declared=filename != 'join_game.filters')
    lobby_action_layout(documents['main_menu.multiplayer_type_select.lobby.xml'])
    return documents


def direct_ip_layout(root, dir_):
    """DIRECT IP: SERVER ADDRESS, SERVER PORT (numeric, default prefilled) and a JOIN GAME row.
    There is no password row: the invite link is already a token.
    A activates the selected row, like SERVER SETUP's START GAME."""
    by = {t.get('name'): t for t in root.iter('widget')}
    menu = by[dir_ + '/direct_ip_menu']
    # The PC row set has only SERVER ADDRESS; SERVER PORT is a copy of it.
    port = deepcopy(by[dir_ + '/op_server_address'])
    port.set('name', dir_ + '/op_server_port')
    for c in port.findall('child'):
        c.set('widget', c.get('widget').replace('server_address_', 'server_port_'))
    label = deepcopy(by[dir_ + '/server_address_label'])
    label.set('name', dir_ + '/server_port_label')
    label.attrib.pop('string_list', None)
    label.attrib.pop('string_index', None)
    label.set('text', 'SERVER PORT')
    value = deepcopy(by[dir_ + '/server_address_value_label'])
    value.set('name', dir_ + '/server_port_value_label')
    value.set('text', '5150')
    anchor = by[dir_ + '/server_address_value_label']
    index = list(root).index(anchor) + 1
    for offset, element in enumerate((port, label, value)):
        root.insert(index + offset, element)
    join = by[dir_ + '/direct_ip_button_join']
    for key in ('string_list', 'string_index', 'align', 'text_y', 'width', 'height', 'font'):
        join.attrib.pop(key, None)
    join.attrib.update(width='512', height=str(ROW_HEIGHT), bitmap='xbox/bitmaps/option_bkds',
                       font=r'ui\large_ui', color=BLUE, text_x='13', text_y='4', text='JOIN GAME')
    for c in list(menu.findall('child')):
        menu.remove(c)
    child(menu, dir_ + '/op_server_address', x=54, y=FIELD_Y)
    child(menu, dir_ + '/op_server_port', x=54, y=FIELD_Y + 39)
    child(menu, dir_ + '/direct_ip_button_join', x=54, y=FIELD_Y + 78)
    for t in list(root.findall('widget')):
        if t.get('name', '').startswith(dir_ + '/direct_ip_button_bar'):
            root.remove(t)


# The option lists hosting_layout lays out: their rows share slots between the co-op and competitive variants
HOSTED_LISTS = set()


def hosting_layout(root, widgets, page):
    """Use one option-row geometry while preserving the PC runtime bindings."""
    base = 'xbox/' + MP + '/' + page
    options = widgets[base + '/options_menu']
    HOSTED_LISTS.add(options.get('name'))
    for c in list(options.findall('child')):
        if c.get('widget') == base + '/button_bar':
            options.remove(c)
    # Keep the old button definitions for their named bindings, but draw only
    # the list action. Co-op options apply each change as it is made, so that
    # page has no action row. The shared CANCEL widget still serves other screens.
    if page == 'server_settings':
        action = widgets[base + '/op_start_game']
        for key in ('string_list', 'string_index', 'align'):
            action.attrib.pop(key, None)
        action.attrib.update(width='512', height=str(ROW_HEIGHT), bitmap='xbox/bitmaps/option_bkds',
                             font=r'ui\large_ui', color=BLUE, text_x='13', text_y='4', text='START GAME')
        widgets[base + '/invite_value'].attrib.update(width='238', height='53', font=r'ui\large_ui', color='#FFFFFFFF')
        widgets[base + '/x_butn_invite'].attrib.update(width='238', height='53')
    for c in options.findall('child'):
        row = widgets[c.get('widget')]
        row.set('height', str(ROW_HEIGHT))
        for inner in row.findall('child'):
            item = widgets[inner.get('widget')]
            leaf = item.get('name').rsplit('/', 1)[-1]
            if leaf.endswith('_label'):
                inner.set('y', '0')
                item.attrib.update(font=r'ui\large_ui', text_y='4')
            elif leaf.endswith(('_spinner', '_value')):
                # (text values, such as the join code, are wider than a spinner's 206 units: centered on
                # the same line, in a 250-unit field)
                wide = item.get('type') != 'spinner'
                inner.attrib.update(x='264' if wide else '286', y='1')
                item.attrib.update(font=r'ui\large_ui', color='#FFFFFFFF', width='250' if wide else '206',
                                   height='20', left='3', top='2', text_y='1')
                if item.get('type') == 'spinner':
                    item.attrib.update(header_bounds='7 -6 19 0', footer_bounds='7 209 19 215')


def map_screen_layout(root, widgets, shared):
    """The Map screen's long names (campaign levels such as "The Truth and Reconciliation") fit: this screen's
    rows and its campaign title use the small font, in copies of the shared widgets (the other screens keep
    theirs), and the description sits in the one panel the screen draws, as the other Xbox lists' do (the PC
    layout's own dark block beside it is gone)."""
    base = 'xbox/' + MP + '/mp_map_select'
    small = r'ui\small_ui'
    # Multiplayer prose includes placeholder text. Keep only player counts
    # in this screen's own strings; campaign descriptions keep their source.
    data = widgets[base + '/mp_map_right_data']
    source = next(e for e in shared if e.tag == 'strings' and e.get('name') == data.get('string_list'))
    counts = deepcopy(source)
    counts.set('name', base + '/player_counts')
    for entry in counts:
        entry.set('text', next((line.strip() for line in entry.get('text').split(r'\n')
                                if line.strip().endswith(' players')), ''))
    root.append(counts)
    data.set('string_list', counts.get('name'))

    def copy(source, name, **attributes):
        element = deepcopy(widgets[source])
        element.set('name', name)
        element.attrib.update({k: str(v) for k, v in attributes.items()})
        root.append(element)
        widgets[name] = element
        return element

    copy('xbox/main_menu/new_select/list_item_text', base + '/list_item_text', font=small, text_y=6)
    rows = widgets[base + '/mp_map_select_list_2']
    for c in rows.findall('child'):
        leaf = c.get('widget').rsplit('/', 1)[-1]
        if leaf.startswith('list_item_') and leaf[10:].isdigit() and leaf != 'list_item_0':
            row = copy(c.get('widget'), base + '/' + leaf)
            for inner in row.findall('child'):
                if inner.get('widget').endswith('/list_item_text'):
                    inner.set('widget', base + '/list_item_text')
            c.set('widget', row.get('name'))
    # (the campaign names carry a line break for the PC panel's narrow column: one line here)
    names = next(e for e in shared if e.tag == 'strings' and e.get('name') == 'xbox/main_menu/map_list')
    one_line = deepcopy(names)
    one_line.set('name', base + '/map_list_one_line')
    for string in one_line:
        string.set('text', ' '.join(string.get('text').replace('\\n', ' ').replace('\n', ' ').split()))
    root.append(one_line)
    title = copy('xbox/main_menu/solo_level_select/replay_level_right_name', base + '/replay_level_right_name',
                 font=small, height=22, text_y=3, string_list=one_line.get('name'))
    right = widgets[base + '/mp_map_right_item']
    right.attrib.update({k: str(v) for k, v in panel(PANEL_B).items()})
    right.attrib.pop('bitmap', None)
    for c in right.findall('child'):
        if c.get('widget').endswith('/solo_level_select/replay_level_right_name'):
            c.set('widget', title.get('name'))
    for e in root.findall('widget'):
        if e.get('name', '').endswith('/mp_map_extended_desc'):
            for c in e.findall('child'):
                c.set('x', '0')
                c.set('y', '0')


def lobby_action_layout(root):
    """Keep the live lobby shortcuts and touch controls on one bottom row."""
    base = 'xbox/main_menu/multiplayer_type_select/lobby'
    widgets = {w.get('name'): w for w in root.findall('widget')}
    screen = widgets[base + '/lobby_screen']
    bar = widgets[base + '/lobby_button_bar']
    for c in list(screen.findall('child')):
        if '/legend/' in c.get('widget'):
            screen.remove(c)
    for declared in screen.findall('bar'):
        screen.remove(declared)
    for w in list(root.findall('widget')):
        if w.get('name').startswith(base + '/lobby_screen/legend/'):
            root.remove(w)
    # The roster's bands end at 408, above the shared legend.
    rows = widgets[base + '/lobby_list']
    rows.find(f"child[@widget='{bar.get('name')}']").set('y', str(LEGEND_Y))
    bar.set('height', '53')
    for c in list(bar):
        bar.remove(c)
    for e in list(screen.findall('on')):
        if e.get('event') in ('x', 'y', 'white', 'black', 'left_thumb'):
            screen.remove(e)
    # Button roles: A the main action, B back, Y create, X miscellaneous, and
    # the left stick click the fifth action. Never Black or White.
    actions = (
        ('start', 'a', 'START NOW', 'net game speed start'),
        ('team', 'x', 'SWITCH TEAM', 'swap player team'),
        ('add', 'y', 'ADD PLAYER', 'port lobby add player'),
        ('leave', 'b', 'LEAVE', 'port lobby leave'),
        ('share', 'left_thumb', 'SHARE', 'port lobby share'),
    )
    widths = {button: legend_caption_width(caption) for _, button, caption, _ in actions}
    positions = legend_positions(('x', 'y', 'left_thumb', 'b', 'a'), widths)
    # Touch draws SHARE in B's standard slot (`ui_touch_legend_slot_for_button`: B's LEAVE is the title's chevron
    # there), wherever the controller legend packs it.
    for action, button, caption, run in actions:
        x, width = positions[button], widths[button]
        name = base + '/lobby_button_' + action
        target = widgets.get(name)
        if target is None:
            target = widget(root, name)
        target.attrib.clear()
        target.attrib.update(name=name, type='container', width=str(CAPTION_OFFSET + width), height='53')
        for c in list(target):
            target.remove(c)
        child(bar, name, x=x)
        icon = name + '/' + button + '_butn'
        child(target, icon)
        child(target, name + '/caption', x=CAPTION_OFFSET)
        widget(root, icon, width=21, height=26, bitmap=f'xbox/browser/{button}_butn')
        text_widget(root, name + '/caption', caption, width=width, height=26).set('font', r'ui\small_ui')
        attrs = {'run': run}
        if action == 'leave':
            attrs['back'] = 'true'
        if action == 'share':
            attrs['platform'] = 'apple'
        for event in dict.fromkeys(('a', button, 'start') if action != 'share' else ('a', button)):
            ET.SubElement(target, 'on', event=event, **attrs)
        ET.SubElement(target, 'on', event='left_mouse', run='mouse emit accept event')
        if button not in ('a', 'b'):  # A acts on the focused button; B already leaves.
            ET.SubElement(screen, 'on', event=button, **attrs)
    # Runtime controller symbols supply the left stick click, including maps
    # without Xbox art. No SF Symbol images are packaged with the menus.
    bitmap = ET.SubElement(root, 'bitmap', name='xbox/browser/left_thumb_butn')
    ET.SubElement(bitmap, 'frame', png=NONE_PNG, width='512', height='256')


def action_bar_legends(documents):
    """Decorate the remaining visible action bars, retaining their handlers."""
    owners = {w.get('name'): root for root in documents.values() for w in root.findall('widget')}
    widgets = {w.get('name'): w for root in documents.values() for w in root.findall('widget')}
    parents = {}
    for name, w in widgets.items():
        for c in w.findall('child'):
            parents.setdefault(c.get('widget'), []).append(name)

    def screens(name, seen):
        if name in seen:
            return set()
        seen.add(name)
        if name.endswith('screen'):
            return {name}
        result = set()
        for parent in parents.get(name, []):
            result |= screens(parent, seen)
        return result

    for name, bar in list(widgets.items()):
        targets = screens(name, set())
        if 'button_bar' not in name.rsplit('/', 1)[-1] or not targets:
            continue
        legacy = any(t.endswith(('/direct_ip_screen', '/filters_screen')) for t in targets)
        placements = bar.findall('child')
        actions = [widgets[c.get('widget')] for c in placements]
        if any(a.get('name').endswith('_butn') or a.findall('child') for a in actions):
            continue
        root = owners[name]
        assigned = []
        used, primary_used = set(), False
        for original in actions:
            cancel = original.get('name').rsplit('/', 1)[-1].endswith(('_cancel', '_back'))
            leaf_name = original.get('name').rsplit('/', 1)[-1]
            # Button roles: A the main action, Y creating something, X anything
            # else, the left stick click a fifth action. Never Black or White.
            if cancel:
                button = 'b'
            elif any(w in leaf_name for w in ('create', 'add', 'new')):
                button = 'y'
            elif not primary_used and any(w in leaf_name for w in ('ok', 'join', 'start')):
                button, primary_used = 'a', True
            elif 'x' not in used:
                button = 'x'
            else:
                button = 'left_thumb'
            used.add(button)
            assigned.append(button)
        order = [b for b in ('x', 'y', 'left_thumb', 'b', 'a') if b in used]
        # A remaining SELECT legend acts on the focused list row.
        keep_accept = 'a' not in used and any(
            '/legend/' in c.get('widget') and c.get('widget').endswith('/a_butn')
            for t in targets for c in widgets[t].findall('child')) or 'a' not in used and any(
            action.get('slot') == 'a' for t in targets for action in widgets[t].iterfind('bar/action'))
        if keep_accept:
            order.append('a')
        # Captions come from the originals; text read only at runtime keeps 90.
        widths = {button: legend_caption_width(original.get('text'))
                  for original, button in zip(actions, assigned)}
        positions = legend_positions(order, widths)
        for placement, original, button in zip(placements, actions, assigned):
            cancel = button == 'b'
            # Keep the original action name and event context under a unique
            # bar prefix; shared common-button definitions stay untouched.
            action_name = name + '/' + original.get('name').rsplit('/', 1)[-1]
            action = deepcopy(original)
            action.set('name', action_name)
            action.set('type', 'container')
            action.set('width', '121' if legacy else str(CAPTION_OFFSET + widths[button]))
            action.set('height', '26' if legacy else '53')
            caption = widget(root, action_name + '/caption', type='text', width=100 if legacy else widths[button], height=26,
                             font=r'ui\small_ui', color=BLUE)
            for key in ('text', 'string_list', 'string_index'):
                if original.get(key) is not None:
                    caption.set(key, original.get(key))
                action.attrib.pop(key, None)
            action.attrib.pop('bitmap', None)
            icon_name = action_name + '/' + button + '_butn'
            child(action, icon_name)
            child(action, caption.get('name'), x=21 if legacy else CAPTION_OFFSET)
            widget(root, icon_name, width=21, height=26, bitmap=f'xbox/browser/{button}_butn')
            handlers = [e for e in original.findall('on') if e.get('event') == 'a']
            # X/Y shortcuts invoke the same actions as accepting the focused
            # button. B retains the engine's standard back behavior.
            if not cancel and button != 'a':  # A already accepts the focused button.
                shortcuts = []
                for handler in handlers:
                    shortcut = deepcopy(handler)
                    shortcut.set('event', button)
                    action.append(shortcut)
                    shortcuts.append(shortcut)
                for target_name in targets:
                    target = widgets[target_name]
                    if target.find(f"on[@event='{button}']") is None:
                        target.extend(deepcopy(shortcuts))
            root.append(action)
            placement.set('widget', action_name)
            placement.set('x', str(60 + placements.index(placement) * 200 if legacy and len(actions) <= 3 else
                                   20 + placements.index(placement) * 150 if legacy else positions[button]))
            placement.set('y', '0')
        for parent_name in parents.get(name, []):
            for c in widgets[parent_name].findall('child'):
                if c.get('widget') == name:
                    c.set('y', '430' if legacy else str(LEGEND_Y))
        for target_name in targets:
            screen = widgets[target_name]
            # Replace the duplicate B caption with the actual Back/Cancel
            # action; keep SELECT only when a two-action row has room.
            for c in list(screen.findall('child')):
                leaf = c.get('widget').rsplit('/', 1)[-1]
                if '/legend/' in c.get('widget'):
                    if leaf in ('b_butn', 'cancel') or (len(actions) > 2 if legacy else not keep_accept):
                        screen.remove(c)
                    else:
                        c.set('y', '430' if legacy else str(LEGEND_Y))
            # a declared bar loses the same actions: the action bar holds the screen's back, and its select
            # unless the screen keeps one
            for declared in list(screen.findall('bar')):
                for action in list(declared):
                    if action.get('slot') == 'b' or (len(actions) > 2 if legacy else not keep_accept):
                        declared.remove(action)
                if not len(declared):
                    screen.remove(declared)
        referenced = {c.get('widget') for w in widgets.values() for c in w.findall('child')}
        for target_name in targets:
            for candidate in list(owners[target_name].findall('widget')):
                candidate_name = candidate.get('name')
                if candidate_name.startswith(target_name + '/legend/') and candidate_name not in referenced:
                    owners[target_name].remove(candidate)




def selection_layout(widgets, bitmaps):
    for w in widgets.values():
        for c in w.findall('child'):
            leaf = c.get('widget').rsplit('/', 1)[-1]
            if (leaf.startswith('list_item_') and c.get('y') and
                    w.get('type') == 'column_list'):
                c.set('x', '48')
                w.set('height', '406')
                if '/lobby/' in w.get('name'):
                    c.set('y', str(78 + int(leaf[10:]) * 30))
            if leaf in ('lobby_right_item', 'replay_level_right_item'):
                c.set('x', '0')
                c.set('y', '0')
        leaf = w.get('name').rsplit('/', 1)[-1]
        if leaf.startswith('list_item_') and leaf[10:].isdigit():
            w.set('width', '256')
            w.set('bitmap', bitmap_ref('bitmaps/list_item_bkd_top' if leaf == 'list_item_0'
                                       else 'bitmaps/list_item_bkd', bitmaps))
        elif leaf == 'list_item_text':
            w.set('width', '230')
        elif leaf == 'list_item_arrows':
            w.set('width', '256')
        elif leaf in ('lobby_right_item', 'replay_level_right_item'):
            w.attrib.update({k: str(v) for k, v in panel(PANEL_B).items()},
                            bitmap=PANEL_BITMAPS['b'])
        elif leaf == 'sel_list_desc_bkd':
            w.attrib.update({k: str(v) for k, v in panel(PANEL_B).items()},
                            bitmap=PANEL_BITMAPS['b'])
        elif leaf in ('lobby_map_pic', 'replay_level_right_pic', 'mp_map_right_pic'):
            # connected_pregame remote column + xbox_graphic offset (12,27).
            w.attrib.update(left='335', top='142')
        elif leaf in ('lobby_map_name', 'replay_level_right_name', 'mp_map_right_name'):
            w.attrib.update(left='323', top='115', width='254', height='22')
        elif leaf in ('lobby_game_data', 'replay_level_right_data', 'mp_map_right_data'):
            # The pregame local-player offset (13,138); server info bounds.
            w.attrib.update(left='336', top='253', width='260', height='136')
        elif leaf == 'lobby_join_help':
            # The original bottom-right help shares the action bar's line.
            # The Xbox pregame message field is above the roster instead.
            w.attrib.update(left='323', top='78', width='254', height='31')

GT = 'xbox/main_menu/settings_select/multiplayer_setup/playlist_select'
# rows that fit between the header and the action bar, at the profile list's 33-unit pitch
GT_ROWS = 9
GT_LIST_X, GT_LIST_Y, GT_PITCH = PANEL_A['list']
GT_PANEL = PANEL_A['panel']


def gametype_select_layout(root, widgets, bitmaps):
    """SELECT GAMETYPE TO EDIT in the Xbox look: a vertical list of tabs and a
    panel, laid out as Edit Profile Settings' (the same row art, pitch and
    panel). The rows and the panel are filled by the PC screen's data
    function, "gt edit list update"; the game's own three-card carousel (the
    Xbox's screen) is not used."""
    screen = widgets[GT + '/gametype_select_screen']
    rows = widgets[GT + '/playlist_select_list']
    bitmap = bitmap_ref('bitmaps/list_item_bkd', bitmaps)
    bitmap_ref('bitmaps/list_field', bitmaps)
    pc_row = widgets['xbox/main_menu/new_select/list_item_1']
    for c in list(rows.findall('child')):
        rows.remove(c)
    for c in list(screen.findall('child')):
        screen.remove(c)
    for existing in list(root):
        if existing.get('name') in (GT + '/playlist_button_bar',):
            root.remove(existing)
    header = GT + '/header_select_gametype_to_edit'
    child(screen, header)
    child(screen, rows.get('name'))
    # (the text the data function fills, named as it looks for it)
    widget(root, GT + '/list_item_text', type='text', controller=1, width=214, height=28,
           font=r'ui\large_ui', color=BLUE, text_flags='no_focus_test')
    for index in range(GT_ROWS):
        row = widget(root, f'{GT}/list_item_{index}', controller=1, width=232, height=32,
                     bitmap=bitmap, font=r'ui\large_ui', color=BLUE)
        for event in pc_row.findall('on'):
            row.append(deepcopy(event))
        child(row, GT + '/list_item_text', x=13, y=4)
        child(rows, row.get('name'), x=GT_LIST_X, y=GT_LIST_Y + index * GT_PITCH)
    # (the arrows that show while gametypes are above or below the rows: ui_widget.c names them)
    for direction, y in (('up', GT_LIST_Y + 8), ('down', GT_LIST_Y + (GT_ROWS - 1) * GT_PITCH + 8)):
        arrow = f'{GT}/scroll_{direction}_arrow'
        widget(root, arrow, width=16, height=16, bitmap=bitmap_ref(f'bitmaps/arrow_sm_{direction}', bitmaps))
        child(rows, arrow, x=GT_LIST_X + 204, y=y)
    # With the first row chosen its tab's top line stops 6 units short of the panel's, which starts after its
    # rounded corner (the tab covers the corner): a short line across the seam, with a patch of the panel's dark under it that hides the corner's remnant, shown only then
    seam = ET.SubElement(root, 'bitmap', name='xbox/bitmaps/gametype_seam')
    ET.SubElement(seam, 'frame', png=f'{ART}/gametype_seam.png', width='10', height='6')
    widget(root, GT + '/first_row_seam', width=10, height=6, bitmap='xbox/bitmaps/gametype_seam')
    child(rows, GT + '/first_row_seam', x=GT_PANEL[0] + 1, y=GT_LIST_Y)
    rows.set('height', '400')
    rows.set('description', GT + '/gametype_extended_desc')
    # Create New and Delete are the Y and X buttons, as in the game's own
    # screen; the PC buttons' events are reused on the list and their captions
    ET.SubElement(rows, 'on', event='y', run='create&edit playlist profile',
                  open=r'ui\shell\main_menu\settings_select\multiplayer_setup\create_and_edit_new_gametype_screen')
    for event in ('b', 'back'):
        ET.SubElement(screen, 'on', event=event, back='true')
    # the panel: the gametype's picture, lock, name and what it plays like
    left, top, width, height = GT_PANEL
    description = widgets[GT + '/gametype_extended_desc']
    description.attrib.update(left=str(left), top=str(top), width=str(width), height=str(height),
                              bitmap=PANEL_BITMAPS['a'])
    for c in description.findall('child'):
        c.set('x', '0')
        c.set('y', '0')
    card = widgets['xbox/main_menu/gametype_select/gametype_right_item']
    card.attrib.update(left=str(left), top=str(top), width=str(width), height=str(height))
    card.attrib.pop('bitmap', None)
    picture = widgets['xbox/main_menu/gametype_select/gametype_right_pic']
    # (the picture is drawn at its own 140 by 114, centered over the panel)
    picture.attrib.update(left=str(left + (width - 140) // 2), top=str(top + 14), width='140', height='114')
    for c in card.findall('child'):
        leaf = c.get('widget').rsplit('/', 1)[-1]
        if leaf == 'locked_gametype_icon':
            c.attrib.update(x=str(left + (width - 140) // 2 + 6), y=str(top + 14 + 114 - 20))
    name = widgets['xbox/main_menu/gametype_select/gametype_right_name']
    name.attrib.update(left=str(left + 19), top=str(top + 130), width=str(width - 38), height='30',
                       font=r'ui\large_ui', color=BLUE)
    # (the PC rules lines, empty here: the rules text below replaces them)
    data = widgets['xbox/main_menu/gametype_select/gametype_right_data']
    data.attrib.update(left=str(left + 19), top=str(top + 158), width=str(width - 38), height='150',
                       font=r'ui\large_ui', color=BLUE)
    # the action bar, as the server browser's: X, Y, B and A with their captions
    items = (('y_butn', 'CREATE NEW', 132, 'playlist_button_new'),
             ('x_butn', 'DELETE', 80, 'playlist_button_delete'),
             ('b_butn', 'BACK', 52, None),
             ('a_butn', 'SELECT', 70, None))
    widths = {icon[0]: legend_caption_width(caption) for icon, caption, _, _ in items}
    positions = legend_positions(('y', 'x', 'b', 'a'), widths)
    for icon, caption, caption_width, button in items:
        x, caption_width = positions[icon[0]], widths[icon[0]]
        art = f'{GT}/legend/{icon}'
        child(screen, art, x=x, y=414)
        widget(root, art, type='container', width=21, height=26, bitmap=f'xbox/browser/{icon}')
        label = f'{GT}/legend/{caption.lower().replace(" ", "_")}'
        if button:
            source = widgets[f'{GT}/{button}']
            source.attrib.pop('string_list', None)
            source.attrib.pop('string_index', None)
            source.attrib.update(text=caption, width=str(caption_width), height='22', font=r'ui\small_ui',
                                 align='left', text_y='0', color=BLUE)
            source.attrib.pop('bitmap', None)
            if button == 'playlist_button_new':
                # (a click on the caption creates, as Y does; the PC button's click posted A)
                for event in source.findall("on[@event='left_mouse']"):
                    event.attrib.pop('run')
                    event.set('run', 'create&edit playlist profile')
                    event.set('open', r'ui\shell\main_menu\settings_select\multiplayer_setup\create_and_edit_new_gametype_screen')
            child(screen, source.get('name'), x=x + CAPTION_OFFSET, y=LEGEND_Y)
        else:
            text = text_widget(root, label, caption, width=caption_width, height=22)
            text.set('font', r'ui\small_ui')
            ET.SubElement(text, 'on', event='left_mouse',
                          run='mouse emit back event' if icon == 'b_butn' else 'mouse emit accept event')
            child(screen, label, x=x + CAPTION_OFFSET, y=LEGEND_Y)
        # The slot width is shared with every other multi-button legend.
    # (the PC bar's OK and BACK are not used: this bar has its own)
    for existing in list(root):
        if existing.get('name') in (GT + '/playlist_button_ok',):
            root.remove(existing)


# the browser's status box: its drawing's size (xbox/art/browser_status__0.png), which the box takes too
BROWSER_STATUS = (544, 64)


def browser_layout(root, widgets, bitmaps):
    base = 'xbox/' + MP + '/join_game'
    get = lambda leaf: widgets[base + '/' + leaf]
    screen = get('join_game_screen')
    rows = get('join_game_items_list')
    rows.attrib.pop('bitmap', None)
    rows.attrib.update(height='402', visible_rows='9')
    # These textures include transparent padding: the panel's outline is
    # about 315x322 of 512x512. Scale that visible artwork to the description
    # bounds, leaving its transparent padding clipped.
    for art, width, height, frames in (('list_field', 884, 102, 1),):
        bitmap = ET.SubElement(root, 'bitmap', name=f'xbox/browser/{art}')
        for index in range(frames):
            ET.SubElement(bitmap, 'frame', map='ui\\shell\\bitmaps\\' + art, index=str(index),
                          png=f'pictures/shell/bitmaps/{art}__{index}.png',
                          width=str(width), height=str(height))

    # The Xbox's nine-row list needs a wider column for the PC browser's
    # five live fields. Keep the updater's row names and all its children.
    # Region ends 36 pixels before the cap, matching the Server Name inset.
    columns = {'locked': (0, 16), 'dedicated': (16, 16),
               'server_name': (36, 138), 'map': (178, 132),
               'classic': (310, 10), 'type': (322, 62),
               'players': (386, 60), 'ping': (450, 58)}
    for c in list(rows.findall('child')):
        leaf = c.get('widget').rsplit('/', 1)[-1]
        if leaf.startswith('server_item_') and leaf[12:].isdigit():
            index = int(leaf[12:]) - 1
            if index >= 9:
                rows.remove(c)
                continue
            c.attrib.update(x='48', y=str(108 + FIELD_GAP + 24 * index))
        elif leaf in ('scroll_up_button', 'scroll_down_button', 'join_game_button_bar'):
            rows.remove(c)
        elif leaf == 'join_game_header':
            c.attrib.update(x='48', y=str(86 + FIELD_GAP))
        elif leaf.startswith('item_ticker_') or leaf == 'op_browser_mode':
            # Nested lists are excluded from the viewport's row detection.
            # Keep these descendants where gamespy screen update finds them.
            get(leaf).set('type', 'column_list')
            if leaf.startswith('item_ticker_'):
                c.attrib.update(x='61', y='338' if leaf.endswith('player_info') else '362')
    # Retain even the unused row definitions and their handlers so the PC
    # browser's named-widget contract stays intact.
    for index in range(1, 16):
        row = get(f'server_item_{index}')
        row.attrib.update(width='544', height='24', color=SERVER_BLUE,
                          bitmap='xbox/browser/row')
        for c in row.findall('child'):
            field = c.get('widget').rsplit('/', 1)[-1].removeprefix('server_item_')
            c.set('x', str(columns[field][0]))
    for field, (_, width) in columns.items():
        get('server_item_' + field).set('width', str(width))
    get('server_item_server_name').set('color', SERVER_BLUE)
    headers = get('join_game_header')
    headers.attrib.update(width='544', height='20')
    for c in headers.findall('child'):
        field = c.get('widget').rsplit('/', 1)[-1].removeprefix('op_sort_header_')
        x, width = columns[field]
        c.attrib.update(x=str(x), y='0')
        header = widgets[c.get('widget')]
        header.set('width', str(width))
        header.attrib.pop('bitmap', None)
        get('sort_header_' + field).set('width', str(width))
    for leaf in ('item_ticker_player_info', 'item_ticker_rules_info'):
        item = get(leaf)
        item.attrib.update(width='518', height='24')
        item.attrib.pop('bitmap', None)
        for c in item.findall('child'):
            c.attrib.update(x='0', y='0')
    for leaf in ('ticker_player_info_label', 'ticker_rules_info_label'):
        for item in (get('item_ticker_player_info'), get('item_ticker_rules_info')):
            for c in list(item.findall('child')):
                if c.get('widget') == base + '/' + leaf:
                    item.remove(c)
    for leaf in ('ticker_player_info', 'ticker_rules_info'):
        get(leaf).attrib.update(width='518', height='24', color='#FFFFFFFF')
    panel = get('screen_stats_container')
    # (the box is as large as its drawing: a widget cuts its picture to its own size, and a shorter box lost the
    # drawing's bottom border)
    panel.attrib.update(left='48', top='328', width=str(BROWSER_STATUS[0]), height=str(BROWSER_STATUS[1]),
                        bitmap='xbox/browser/browser_status')
    # Bounds do not translate children: stats stay over the column headers,
    # while the description background sits below the ninth server row.
    for c, x, width in zip(panel.findall('child'), (326, 48, 226), (266, 176, 98)):
        c.attrib.update(x=str(x), y=str(68 + FIELD_GAP))
        widgets[c.get('widget')].attrib.update(width=str(width), height='22')
    # The PC actions inspect the invoking widget's leaf name. This adapter
    # gives X the refresh action and Y the Internet hosting action while
    # sending A and navigation only to the focused server row.
    actions = widget(root, base + '/button_refresh_create_inter', width=640, height=402,
                     flags='pass_unhandled_to_focused_child')
    ET.SubElement(actions, 'on', event='x', run='gamespy select button')
    ET.SubElement(actions, 'on', event='y', run='join controller to mp game')
    ET.SubElement(actions, 'on', event='y', run='mp type set mode',
                  open=r'ui\shell\main_menu\multiplayer_type_select\connected\connected_map_select_wrapper')
    child(actions, rows.get('name'))
    for c in screen.findall('child'):
        if c.get('widget') == rows.get('name'):
            c.set('widget', actions.get('name'))
    # the title sits at the header's own position, like every other Xbox-look screen
    for c in screen.findall('child'):
        if c.get('widget', '').endswith('/header_join_game'):
            c.attrib.pop('y', None)
    # a four-button bar whose X is the list's own refresh widget: converts with the dense bars
    legend(root, screen, base + '/legend', cancel='BACK', declared=False)
    # ui.map's 32x32 button textures include transparent padding; button_key
    # draws them at native size and advances 21 pixels to each caption.
    # The PNG redraws are needed only in maps without these Xbox bitmaps.
    # (the _sm buttons are the delete confirmations' legend, at their size)
    for button in ('a', 'b', 'x', 'y', 'a_sm', 'b_sm'):
        art = button[0] + '_butn' + ('_sm' if button.endswith('_sm') else '')
        size = dict(width='21', height='26') if button.endswith('_sm') else dict(width='32', height='32')
        bitmap = ET.SubElement(root, 'bitmap', name=f'xbox/browser/{art}')
        ET.SubElement(bitmap, 'frame', map='ui\\shell\\bitmaps\\' + art, index='0',
                      png=f'xbox/art/{button[0]}_butn.png', **size)
    bitmap = ET.SubElement(root, 'bitmap', name='xbox/browser/browser_status')
    ET.SubElement(bitmap, 'frame', png='xbox/art/browser_status__0.png', width=str(BROWSER_STATUS[0]),
                  height=str(BROWSER_STATUS[1]))
    # A shared, inset redraw keeps the cap and right edge inside every row.
    bitmap = ET.SubElement(root, 'bitmap', name='xbox/browser/row')
    for index in (0, 1):
        ET.SubElement(bitmap, 'frame', png=f'xbox/art/browser_row__{index}.png', width='544', height='28')
    child(screen, base + '/y_butn', x=180, y=414)
    child(screen, base + '/create', x=202, y=414)
    widget(root, base + '/y_butn', width=21, height=26, bitmap='xbox/browser/y_butn')
    text_widget(root, base + '/create', 'CREATE GAME', width=144, height=22)
    child(screen, base + '/x_butn', x=48, y=414)
    widget(root, base + '/x_butn', width=21, height=26, bitmap='xbox/browser/x_butn')
    # Reuse the refresh action as the legend's clickable caption.
    refresh = get('join_game_button_refresh')
    refresh.attrib.pop('string_list', None)
    refresh.attrib.pop('string_index', None)
    refresh.attrib.update(text='REFRESH', width='104', height='22', font=r'ui\large_ui',
                          align='left', text_y='0')
    child(screen, refresh.get('name'), x=70, y=414)
    # button_key advances 21 pixels from icon to caption. Leave the same
    # 27-pixel gap after each caption, including the longer CREATE GAME.
    items = (('x_butn', 'join_game_button_refresh', 104),
             ('y_butn', 'create', 132),
             ('legend/b_butn', 'legend/cancel', 52),
             ('legend/a_butn', 'legend/accept', 70))
    # A reads JOIN GAME once a game is selected, so its box fits that caption.
    labels = {'x': 'REFRESH', 'y': 'CREATE GAME', 'b': 'CANCEL', 'a': 'JOIN GAME'}
    widths = {b: legend_caption_width(label) for b, label in labels.items()}
    positions = legend_positions(('x', 'y', 'b', 'a'), widths)
    for icon, caption, _ in items:
        button = icon.rsplit('/', 1)[-1][0]
        x = positions[button]
        for leaf, offset in ((icon, 0), (caption, CAPTION_OFFSET)):
            screen.find(f"child[@widget='{base}/{leaf}']").set('x', str(x + offset))
        label = root.find(f"widget[@name='{base}/{caption}']")
        label.attrib.update(width=str(widths[button]), font=r'ui\small_ui')


ERROR_DIALOGS = tuple(f'error_{mode}_{size}screen' for mode in ('modal', 'nonmodal')
                      for size in ('full', 'half', 'qtr')) + ('error_modal', 'error_non_modal')
ABORT_DIALOGS = ('error_abort_to_dashboard', 'error_abort_to_dashboard_you_have_no_choice')
WAIT_DIALOGS = ('warning_creating_player_profile_screen', 'warning_saving_file_screen')


# Every generated screen names its layout. Native layouts declare their kind
# here too, but their map-tag geometry is owned by the game.
LAYOUT_KINDS = {
    'K1': {'legend_y': LEGEND_Y, 'help_top': HELP_TOP, 'row_pitch': ROW_PITCH},
    'K2-A': {'legend_y': LEGEND_Y, 'row_pitch': ROW_PITCH, 'panel': PANEL_A},
    'K2-B': {'legend_y': LEGEND_Y, 'row_pitch': ROW_PITCH, 'panel': PANEL_B},
    'K6': {'legend_y': LEGEND_Y, 'row_pitch': ROW_PITCH},
    'K8': {'panel': DIALOG_PANEL, 'header': DIALOG_HEADER, 'button': DIALOG_BUTTON,
           # Targets fit the panel's width; their lower 12 units extend below it so the message keeps its room.
           'target_overhang_below': 12},
    **{kind: {'native': True} for kind in ('K3', 'K4', 'K9', 'K10', 'K11')},
}
SCREEN_LAYOUTS = {
    **{f'xbox/settings/{page}/{screen}': {'kind': 'K1', 'options': f'xbox/settings/{page}/options_menu'}
       for page, screen in zip(PAGES, ('video_settings_screen', 'audio_settings_screen', 'network_settings_screen',
                                      'controls_settings_screen', 'mouse_settings_screen', 'gamepad_setup_screen',
                                      'about_screen', 'graphics_screen', 'maps_screen'))},
    'xbox/settings/settings_screen': {'kind': 'K2-A', 'options': 'xbox/settings/settings_list'},
    GAMETYPE_EDIT_SCREEN: {'kind': 'K2-A', 'options': f'{GAMETYPE_EDIT}/gametype_list'},
    # ABOUT keeps its version line and taller legal help below the scrolling actions.
    'xbox/settings/about/about_screen': {'kind': 'K1', 'options': 'xbox/settings/about/options_menu',
                                         'help': (286, 120), 'first_row': 102},
    # Notices scroll without a focus box; the license prose remains read-only.
    'xbox/settings/about/notices_screen': {'kind': 'K1', 'options': 'xbox/settings/about/notices/options_menu',
                                           'scroll_only': True, 'help': (350, 60)},
    'xbox/settings/send_log_confirm': {'kind': 'K8'},
    **{f'xbox/{MP}/{page}/{page}_screen': {'kind': 'K1', 'options': f'xbox/{MP}/{page}/options_menu'}
       for page in ('server_settings', 'coop_options')},
    # Hosting reserves the leftmost slot for the 238-wide invite field; the
    # captions are sized to their text so the A touch pill stays on screen.
    f'xbox/{MP}/server_settings/server_settings_screen': {
        'kind': 'K1', 'options': f'xbox/{MP}/server_settings/options_menu',
        'legend_targets': {'x': 294, 'b': 421, 'a': 520},
        'legend_widths': {'x': 85, 'b': 57, 'a': 85}, 'invite_width': 238},
    # The gametype editor's own screens: one settings list each (menu_functions.c fills the rows)
    **{f'xbox/{MP}/{page}/{screen}': {'kind': 'K1', 'options': f'xbox/{MP}/{page}/options_menu'}
       for page, screen in (('game_options_edit', 'game_options_screen'),
                            ('respawn_options_edit', 'respawn_options_screen'),
                            ('teams_hud_edit', 'teams_hud_screen'),
                            ('vehicle_types_edit', 'vehicle_types_screen'),
                            ('weapons_edit', 'weapons_screen'))},
    # (the per-type screens keep the PC version's own lists, as the gametype editor's did)
    **{f'xbox/main_menu/settings_select/multiplayer_setup/playlist_edit/{kind}_edit/{kind}_options_screen':
       {'kind': 'K1', 'legacy': True} for kind in ('ctf', 'koth', 'oddball', 'race', 'slayer')},
    # DIRECT IP and filters stay unchanged until their owning branches land.
    f'xbox/{MP}/direct_ip/direct_ip_screen': {'kind': 'K1', 'legacy': True},
    f'xbox/{MP}/join_game/filters/filters_screen': {'kind': 'K1', 'legacy': True},
    # Entry fields and the password's Y EDIT affordance belong to the join-form branch.
    **{f'xbox/{MP}/join_game/{page}/{page}_screen': {'kind': 'K1', 'entry_legend': True}
       for page in ('password', 'code')},
    # The browser's header, filter fields, and 338-unit list limit belong to its redesign.
    f'xbox/{MP}/join_game/join_game_screen': {'kind': 'K6', 'list_limit': 328},
    f'xbox/{MP}/join_game/gamespy_error_fullscreen': {'kind': 'K8'},
    f'xbox/{MP}/lobby/lobby_screen': {'kind': 'K2-B', 'options': f'xbox/{MP}/lobby/lobby_list'},
    f'xbox/{MP}/lobby/preview_screen': {'kind': 'K2-B'},
    # This page's short player-profile help is beside its native profile panel.
    f'xbox/{MP}/lobby/player_profile_screen': {'kind': 'K2-B', 'profile_help': (20, 416, 350, 24)},
    f'xbox/{MP}/mp_map_select/mp_map_select_screen': {'kind': 'K2-B'},
    f'xbox/{MP}/connected/connected_map_select_wrapper': {'kind': 'K2-B'},
    GT + '/gametype_select_screen': {'kind': 'K2-A', 'options': GT + '/playlist_select_list'},
    'xbox/main_menu/quit_select/quit_screen': {'kind': 'K8'},
    **{'xbox/error/' + name: {'kind': 'K8', 'buttonless': name in WAIT_DIALOGS}
       for name in ERROR_DIALOGS + ABORT_DIALOGS + WAIT_DIALOGS},
    **{f'xbox/error/confirm_{name}_modal_fullscreen': {'kind': 'K8'}
       for name in ('delete_profile', 'delete_gametype', 'join_big')},
    # VEHICLE OPTIONS: its rows are the standard rows of an option list (the PC version's spacer before SIDE is gone).
    'xbox/main_menu/settings_select/multiplayer_setup/vehicle_options_edit/vehicle_options_screen': {
        'kind': 'K1', 'options': 'xbox/main_menu/settings_select/multiplayer_setup/vehicle_options_edit/vehicle_options_menu'},
}
# These settings parameters preserve supplemental content rather than moving it into help.
SCREEN_LAYOUTS['xbox/settings/video_settings/video_settings_screen']['frame_rate_row'] = 307  # VIDEO's Mac frame-rate row.
SCREEN_LAYOUTS['xbox/settings/gamepad_setup/gamepad_setup_screen']['ingame_help_row'] = 346  # Gamepad's in-game help row.
NATIVE_SCREEN_LAYOUTS = {
    # System Link's server-list legend is in ui.map, whose positions require native-code changes.
    'server_list': {'kind': 'K2-B', 'native': True, 'legend': legend_positions(('y', 'b', 'a'))},
    **{name: {'kind': 'K2-A', 'native': True} for name in ('profile_manager', 'profile_edit', 'profile_create',
                                                          'mp_type', 'gametype_new', 'gametype_edit')},
    # These gametype editors receive generated rows, but their frame remains native.
    **{name: {'kind': 'K1', 'native': True} for name in ('gt_ctf', 'gt_koth', 'gt_slayer', 'gt_oddball', 'gt_race',
                                                        'gt_player', 'gt_item', 'gt_indicator', 'advanced_controls')},
    **{name: {'kind': 'K1', 'controller_picture': True}  # Controller art replaces the help box.
       for name in ('controller_edit', 'controller_options')},
    **{name: {'kind': 'K3'} for name in ('profile_select', 'level_select', 'difficulty', 'split_map', 'split_gametype',
                                         'gametype_select', 'gametype_type', 'conn_gametype', 'conn_postgame_map',
                                         'split_postgame_map', 'coop_p1', 'coop_p2', 'settings_profiles', 'profile_color')},
    **{name: {'kind': 'K4'} for name in ('split_pregame', 'split_profiles', 'conn_profiles', 'conn_pregame')},
    **{name: {'kind': 'K9'} for name in ('main', 'custom_campaign', 'pause_sp_top', 'pause_sp_hub', 'pause_mp_top',
                                         'pause_mp_hub', 'pause_mp_scores')},
    **{name: {'kind': 'K10'} for name in ('profile_name', 'gametype_name', 'xdemo_wait')},
    'mp_map_select_grid': {'kind': 'K11'},
}
for page in PAGES:
    if page != 'about':
        # DEFAULTS is a global shortcut; the contextual pair retains the two-button spacing.
        next(spec for name, spec in SCREEN_LAYOUTS.items() if name.startswith(f'xbox/settings/{page}/'))['defaults_x'] = 40

LIST_LAYOUTS = {spec['options']: spec for spec in SCREEN_LAYOUTS.values() if 'options' in spec}
OPTION_LIST_LAYOUTS = {name: spec for name, spec in LIST_LAYOUTS.items() if spec['kind'] == 'K1'}


def layout_geometry(documents):
    """Apply shared help and title bounds before fitting the rows to the viewport."""
    for root in documents.values():
        widgets = {w.get('name'): w for w in root.findall('widget')}
        for screen_name, spec in SCREEN_LAYOUTS.items():
            if screen_name not in widgets or spec.get('legacy'):
                continue
            screen = widgets[screen_name]
            if spec['kind'] != 'K8':
                for c in screen.findall('child'):
                    w = widgets.get(c.get('widget'))
                    if w is not None and w.get('name').rsplit('/', 1)[-1].startswith('header'):
                        w.attrib.update({k: str(v) for k, v in title().items()})
                        c.attrib.pop('x', None)
                        c.attrib.pop('y', None)
            if spec['kind'] != 'K1' or not spec.get('options'):
                continue
            base = spec['options'].rsplit('/', 1)[0]
            for w in widgets.values():
                if w.get('name').rsplit('/', 1)[0] != base:
                    continue
                leaf = w.get('name').rsplit('/', 1)[-1]
                if leaf in ('help', 'row_help', 'graphics_help', 'vehicle_options_help', 'teamplay_options_help'):
                    geometry = help_box(HELP_TOP)
                    if 'help' in spec:
                        geometry.update(top=spec['help'][0], height=spec['help'][1])
                    w.attrib.update({k: str(v) for k, v in geometry.items()})
            for c in screen.findall('child'):
                if c.get('widget').endswith('/x_butn_invite'):
                    c.set('y', str(LEGEND_Y))


def browser_error_layout(documents):
    root = documents['main_menu.multiplayer_type_select.join_game.xml']
    shared = documents['multiplayer.shared.xml']
    widgets = {w.get('name'): w for r in (root, shared) for w in r.findall('widget')}
    base = f'xbox/{MP}/join_game'
    screen = widgets[base + '/gamespy_error_fullscreen']
    # gamespy screen update needs the panel's first child to remain its text box.
    pane = widgets['xbox/error/browser_error_panel']
    pane.attrib.update(width='640', height='480')
    text = widgets['xbox/error/error_text_box']
    text.attrib.update(width=str(DIALOG_PANEL[2]), height=str(DIALOG_PANEL[3]), text_x='9', text_y='36',
                       bitmap='xbox/bitmaps/alert_bkd')
    screen.set('bitmap', 'xbox/bitmaps/semi_transparent_grey')
    for c in pane.findall('child'):
        position = DIALOG_HEADER_POSITION if c.get('widget').endswith('/header_error') else DIALOG_PANEL[:2]
        c.attrib.update(x=str(position[0]), y=str(position[1]))
    for c in list(screen.findall('child')):
        if 'button_bar' in c.get('widget'):
            screen.remove(c)
        elif c.get('widget') == pane.get('name'):
            c.attrib.update(x='0', y='0')
    dialog_legend_targets(root, screen, base + '/error_legend')


def dialog_legend_slots(labels):
    """Slots (panel-relative x, width) for a dialog's legend buttons, in the order given.

    Each slot is as wide as its glyph and caption plus padding (never under the tap minimum), and the
    row is centered as a group on the panel, so a short OK beside a long CANCEL is not left-heavy.
    """
    widths = [max(DIALOG_SLOT_MIN, DIALOG_GLYPH + DIALOG_GLYPH_GAP + dialog_caption_width(label) + 2 * DIALOG_SLOT_PAD)
              for label in labels]
    x = (DIALOG_PANEL[2] - sum(widths) - DIALOG_SLOT_GAP * (len(widths) - 1)) // 2
    slots = []
    for width in widths:
        slots.append((x, width))
        x += width + DIALOG_SLOT_GAP
    return slots


def dialog_caption_width(label):
    return math.ceil(len(label) * DIALOG_CAPTION_CHAR)


def dialog_legend_targets(root, screen, base, x=None, y=None, cancel=False, accept='OK'):
    """Reuse the legend bindings, with full touch buttons and adaptive glyphs.

    x is the panel's left edge plus DIALOG_BUTTON[0] and y its top plus DIALOG_BUTTON[1] (the pill's place);
    the buttons are laid out from the panel, not from those constants.
    """
    left = (DIALOG_PANEL[0] if x is None else x - DIALOG_BUTTON[0])
    if y is None:
        y = DIALOG_PANEL[1] + DIALOG_BUTTON[1]
    top = y - DIALOG_BUTTON[1]
    legend(root, screen, base, accept=accept, declared=False)
    order = [('b', 'CANCEL'), ('a', accept)] if cancel else [('a', accept)]
    slots = dict(zip((b for b, _ in order), dialog_legend_slots([label for _, label in order])))
    for button, label in (('a', accept), ('b', 'CANCEL')):
        icon, caption = f'{base}/{button}_butn', f'{base}/' + ('accept' if button == 'a' else 'cancel')
        if button == 'b' and not cancel:
            for name in (icon, caption):
                root.remove(root.find(f"widget[@name='{name}']"))
                screen.remove(screen.find(f"child[@widget='{name}']"))
            continue
        screen.remove(screen.find(f"child[@widget='{caption}']"))
        slot_x, slot_w = slots[button]
        pill_dy = DIALOG_BUTTON[1] - DIALOG_HIT[1]
        screen.find(f"child[@widget='{icon}']").attrib.update(x=str(left + slot_x), y=str(top + DIALOG_HIT[1]))
        target = root.find(f"widget[@name='{icon}']")
        target.attrib.update(width=str(slot_w), height=str(DIALOG_HIT[3]))
        target.attrib.pop('bitmap')
        pill_w = slot_w - 4
        caption_width = dialog_caption_width(label)
        content_x = (slot_w - (DIALOG_GLYPH + DIALOG_GLYPH_GAP + caption_width)) // 2
        root.find(f"widget[@name='{caption}']").set('width', str(caption_width))
        ET.SubElement(target, 'data', input='port dialog legend update')
        touch_base = icon + '/'
        child(target, touch_base + 'touch_background', x=2, y=pill_dy)
        widget(root, touch_base + 'touch_background', width=pill_w, height=DIALOG_BUTTON[3], bitmap='xbox/dialog_touch_button')
        child(target, touch_base + 'touch_caption', x=2, y=(DIALOG_BUTTON[3] - 24) // 2 + pill_dy)
        text_widget(root, touch_base + 'touch_caption', label, width=pill_w, height=24, align='center')
        # The glyph and caption sit centered in the footer band under the rule, as one unit in the slot.
        child(target, icon + '_glyph', x=content_x, y=DIALOG_GLYPH_Y - DIALOG_HIT[1])
        widget(root, icon + '_glyph', width=DIALOG_GLYPH, height=26, bitmap=f'xbox/browser/{button}_butn')
        child(target, caption, x=content_x + DIALOG_GLYPH + DIALOG_GLYPH_GAP, y=DIALOG_GLYPH_Y + 2 - DIALOG_HIT[1])
        ET.SubElement(target, 'on', event='left_mouse',
                      run='mouse emit accept event' if button == 'a' else 'mouse emit back event')


def dialog_document():
    """Keep display_error's first two children stable while sharing the Xbox art."""
    root = ET.Element('menus')
    bitmap = ET.SubElement(root, 'bitmap', name='xbox/dialog_touch_button')
    ET.SubElement(bitmap, 'frame', png='pictures/shell/bitmaps/text_button_background__1.png',
                  width='128', height='32')  # the png is 512x128; the widget scales it to the drawn pill
    for name in ERROR_DIALOGS + ABORT_DIALOGS + WAIT_DIALOGS:
        base = 'xbox/error/' + name
        width = 320 if 'qtrscreen' in name else 640
        height = 240 if any(size in name for size in ('halfscreen', 'qtrscreen')) else 480
        left, top = DIALOG_PANEL[:2] if (width, height) == (640, 480) else ((width - DIALOG_PANEL[2]) // 2, (height - 212) // 2)
        waiting = name in WAIT_DIALOGS
        abort = name in ABORT_DIALOGS
        screen = widget(root, base, width=width, height=height,
                        flags='pass_unhandled_to_focused_child' +
                              (' pause_game' if name == WAIT_DIALOGS[1] else ''))
        # The abort API requires a text-box root; its small port hook copies
        # the index into the visible child, after the backdrop has drawn.
        if abort:
            screen.set('type', 'text')
        # The dim covers a whole 640x480 screen only; a split view gets no dim box. It is the
        # screen's own bitmap. display_error needs the pane's first child to be the text box, so
        # the frame art is the pane's bitmap and the text box is as wide as the frame and the
        # runtime wraps to its width less the text offset on each side (equal padding).
        if width == 640 and height == 480:
            screen.set('bitmap', 'xbox/bitmaps/semi_transparent_grey')
        panel = widget(root, base + '/panel', width=DIALOG_PANEL[2], height=DIALOG_PANEL[3],
                       bitmap='xbox/bitmaps/alert_bkd')
        child(screen, panel.get('name'), x=left, y=top)
        text = widget(root, base + '/error_text_box', type='text', width=DIALOG_PANEL[2], height=DIALOG_PANEL[3],
                      string_list='xbox/strings/displayed_error_messages',
                      font=r'ui\large_ui', color=BLUE, text_x=9, text_y=36)
        ET.SubElement(text, 'replace', search='%controller', function='controller')
        child(panel, text.get('name'), x=0, y=0)
        child(panel, 'xbox/error/header_saving' if waiting else 'xbox/error/header_error',
              x=DIALOG_HEADER[0], y=DIALOG_HEADER[1])
        if waiting:
            screen.attrib.update(auto_close='1000', auto_close_fade='500')
            text.set('string_index', '16' if name == WAIT_DIALOGS[0] else '15')
            for event in ('b', 'back'):
                ET.SubElement(screen, 'on', event=event, run='NULL')
            continue
        for event in ('a', 'start'):
            ET.SubElement(screen, 'on', event=event, close='current')
        dialog_legend_targets(root, screen, base, left + DIALOG_BUTTON[0], top + DIALOG_BUTTON[1])
    return root


def quit_layout(root, widgets):
    base = 'xbox/main_menu/quit_select'
    bar = widgets[base + '/quit_button_bar']
    bar.set('width', '640')
    bar.set('height', '480')
    bar.attrib.pop('type', None)
    bar.attrib.pop('flags', None)
    bar[:] = []
    screen = widgets[base + '/quit_screen']
    # The message uses the delete confirmation's panel and padding. Keep the
    # legend and title in screen coordinates, outside the offset text widget.
    screen.set('name', base + '/quit_panel')
    screen.attrib.update({k: str(v) for k, v in panel(DIALOG_PANEL).items()},
                         text_x='9', text_y='36', align='left')
    # The PC prompt's first line runs past this narrower Xbox panel's inner edge.
    screen.attrib.pop('string_list', None)
    screen.attrib.pop('string_index', None)
    screen.set('text', r'Are you sure you\nwish to exit Halo?')
    screen[:] = []
    modal = widget(root, base + '/quit_screen', width=640, height=480, controller=1,
                   flags='pass_unhandled_to_focused_child pass_unhandled_to_all_children',
                   bitmap='xbox/bitmaps/semi_transparent_grey')
    for event in ('a', 'start'):
        ET.SubElement(modal, 'on', event=event, run='main menu quit game')
    for event in ('b', 'back'):
        ET.SubElement(modal, 'on', event=event, back='true')
    child(modal, base + '/quit_panel')
    child(modal, 'xbox/error/header_are_you_sure', x=DIALOG_HEADER_POSITION[0], y=DIALOG_HEADER_POSITION[1])
    dialog_legend_targets(root, modal, base + '/legend', cancel=True)


# ui.map's tag paths the patch table names (ui\\shell\\ is left out of each) and the screens it opens
MM = 'ui\\shell\\main_menu\\'
MPT = MM + 'multiplayer_type_select\\'
PEP = MM + 'settings_select\\player_setup\\player_profile_edit\\'
GTE = MM + 'settings_select\\multiplayer_setup\\playlist_edit\\'


def patch(root, list_path, screen_path):
    return ET.SubElement(root, 'patch', {'list': list_path, 'screen': screen_path, 'footer': '414'})


# The advanced controls page: (patch row name, the original's row tag, the controls page's key, help text)
ADVANCED_ROWS = (
    ('op_invert_joystick', 'op_invert_joystick', 'invert_look',
     'Pushing the thumbstick up looks down, and\\npulling it down looks up.|Pushing the thumbstick up looks up.'),
    ('op_look_sensitivity', 'op_controller_sensitivity', 'look_sensitivity',
     'How fast the thumbstick turns your view.'),
    ('op_controller_vibration', 'op_controller_vibration', 'vibration',
     'The controller vibrates during play.|The controller does not vibrate.'),
    ('op_flight_inversion', 'op_flight_stick_controls', 'flight_inversion',
     'Flight controls are inverted in aircraft.|Flight controls are not inverted.'),
    ('op_autocenter', 'op_auto_center', 'autocenter',
     'The view returns to center when you let\\ngo of the thumbstick.|The view stays where you leave it.'),
    ('op_ingame_help', 'op_ingame_help', 'ingame_help',
     'The game shows hints about its controls\\nas you play.|The game does not show hints about its\\ncontrols.'),
)


def patch_row(parent, tag='row', **attributes):
    return ET.SubElement(parent, tag, {k: str(v) for k, v in attributes.items()})


def add_advanced_clones(documents):
    """IN-GAME HELP on the Xbox's advanced setup page (a patch row): the row of the controls page, its spinner where
    the Xbox's advanced rows have theirs. Copied after the density pass so each clone has its original's row height
    and art (a copy made earlier kept the 28-unit pill)."""
    gamepad = documents['settings.gamepad_setup.xml']
    for _, _, key, _ in ADVANCED_ROWS:
        row = deepcopy(next(w for w in gamepad if w.get('name') == f'xbox/settings/gamepad_setup/op_{key}'))
        row.set('name', f'xbox/settings/gamepad_setup/op_{key}_advanced')
        spinner = deepcopy(next(w for w in gamepad if w.get('name') == f'xbox/settings/gamepad_setup/{key}_spinner'))
        spinner.set('name', f'xbox/settings/gamepad_setup/{key}_spinner_advanced')
        # a switch's state word is ON or OFF, as on the Xbox's advanced rows; the words live here, in the catalog
        if spinner.get('strings') == 'YES|NO':
            spinner.set('strings', 'ON|OFF')
        for part in row.findall('child'):
            if part.get('widget', '').endswith(f'{key}_spinner'):
                part.set('widget', f'xbox/settings/gamepad_setup/{key}_spinner_advanced')
        gamepad.append(spinner)
        gamepad.append(row)


def patch_document(names):
    """The patch table (port/linux/game/menu_tags.c): VISR's rows in the Xbox's own lists."""
    root = ET.Element('menus')
    # Multiplayer type: SYSTEM LINK sets the mode (LAN) before the Xbox's own flow; INTERNET and DIRECT LINK open
    # the Xbox-look browser, which shows each mode's header, rows and buttons
    mp = patch(root, MPT + 'multiplayer_type_select_list', MPT + 'multiplayer_type_select_screen')
    patch_row(mp, 'replace', child=MPT + 'multiplayer_type_conn_item', name='multiplayer_type_join_lan',
              text='SYSTEM LINK PLAY', run='mp type set mode',
              open=MPT + 'connected\\4way_profile_select\\4way_start2join_screen')
    browser = 'xbox/main_menu/multiplayer_type_select/join_game/join_game_screen'
    patch_row(mp, after=MPT + 'multiplayer_type_conn_item', name='multiplayer_type_internet', text='INTERNET',
              run='mp type set mode', open=browser, picture=f'png:{ART}/internet.png',
              description='Browse games on the Internet,\\njoin one, or host your own.')
    # The panel for INTERNET while internet play is off: a row added only for its description and picture (the
    # entry right after INTERNET's), then removed; ui_widget_game_data_input_functions.c shows it for INTERNET
    patch_row(mp, after='multiplayer_type_internet', name='multiplayer_type_internet_off', text='INTERNET',
              run='mp type set mode', open=browser, picture=f'png:{ART}/internet_off.png',
              description='Internet play is off.\\nTurn it on in Network Settings.')
    patch_row(mp, 'remove', child='multiplayer_type_internet_off')
    patch_row(mp, after='multiplayer_type_internet', name='multiplayer_type_join_direct', text='DIRECT LINK',
              run='mp type set mode', open=browser, picture=f'png:{ART}/internet.png',
              description="Join a friend's game with\\nits join code or invite link.")
    # The panel for SPLIT SCREEN on an iPhone with no game controller: added last, so its number follows
    # DIRECT LINK's, then removed; ui_widget_game_data_input_functions.c shows it for the split row
    patch_row(mp, after=MPT + 'multiplayer_type_split_item', name='multiplayer_type_split_need_pad', text='SPLIT SCREEN',
              run='mp type set mode', open=browser, picture=1,
              description='Split screen on iPhone needs\\ncontrollers for extra players.')
    patch_row(mp, 'remove', child='multiplayer_type_split_need_pad')
    # Insert settings while the map's SAVE CHANGES anchor still exists, then remove it.
    # Apple TV keeps a DELETE PROFILE row because its remote has no X.
    edit = patch(root, PEP + 'profile_edit_select_list', PEP + 'player_profile_edit_screen')
    current = CURRENT
    pages = tuple(row for row in PAGE_ROWS if row[0] != 'controls')
    # Multiplayer's split-screen and system-link diagrams; profile's tuning and keyboard art.
    pictures = {'video': f'png:{ART}/video.png', 'audio': f'png:{ART}/audio.png',
                'network': f'png:{ART}/internet.png', 'keyboard': 0, 'graphics': f'png:{NONE_PNG}'}
    anchor = None
    for key, caption, folder, screen, platform, description in pages:
        attributes = {'name': f'profile_item_{key}', 'text': caption,
                      'open': f'xbox/settings/{folder}/{screen}', 'description': description, 'picture': pictures[key]}
        if platform:
            attributes['platform'] = platform
        if anchor and key not in ('audio', 'graphics'):
            attributes['after'] = anchor
        else:
            attributes['before'] = PEP + 'save_changes_profile_item'
            attributes['model'] = PEP + 'color_profile_item'
        patch_row(edit, **attributes)
        anchor = f'profile_item_{key}'
    # ABOUT: last of the settings pages, before SAVE CHANGES. With the keyboard's row and VIDEO's too (the Mac) the
    # list would pass its footer (431 > 414), so a build with a keyboard has ABOUT in the PC look alone
    patch_row(edit, name='profile_item_about', text='ABOUT', open='xbox/settings/about/about_screen',
              description="An unofficial port of Halo:\\nCombat Evolved." + current,
              picture=f'png:{ART}/about.png', platform='!keyboard', before=PEP + 'save_changes_profile_item',
              model=PEP + 'color_profile_item')
    patch_row(edit, after=PEP + 'save_changes_profile_item', platform='tvos', pitch=COMFORTABLE_PITCH,
              name='profile_item_delete', text='DELETE PROFILE', run='port delete request',
              open='xbox/error/confirm_delete_profile_modal_fullscreen', picture=3,
              description='Delete this profile and\\neverything saved with it.' + current)
    patch_row(edit, 'remove', child=PEP + 'save_changes_profile_item')
    # The original's OK / CANCEL bar has no buttons left in the Xbox look (B leaves, rows save as they change);
    # its empty frame would draw a rule and a band under the list
    patch_row(edit, 'remove', child=PEP + 'profile_edit_button_bar')
    # Advanced setup: IN-GAME HELP, which the game keeps in the profile
    advanced = patch(root, PEP + 'advanced_controls\\advanced_controls_menu',
                     PEP + 'advanced_controls\\advanced_controls_screen')
    # The Xbox look draws the controls page's own rows here (tall rows, ON/OFF switches): the original's five
    # rows are replaced by clones of it, in the original's order
    first = PEP + 'advanced_controls\\' + ADVANCED_ROWS[0][1]
    previous = None
    for name, original, key, help in ADVANCED_ROWS:
        position = {'before': first} if previous is None else {'after': previous}
        patch_row(advanced, name=name, widget=f'xbox/settings/gamepad_setup/op_{key}_advanced',
                  pitch=COMFORTABLE_PITCH, help=help, **position)
        previous = name
    for name, original, key, _ in ADVANCED_ROWS:
        if name != 'op_ingame_help':
            patch_row(advanced, 'remove', child=PEP + 'advanced_controls\\' + original)
    # Item Options (Server Setup's, and a gametype's): the port's rows after the Xbox's four, above the help text
    options = patch(root, MM + 'settings_select\\multiplayer_setup\\item_options_edit\\item_options_menu',
                    MM + 'settings_select\\multiplayer_setup\\item_options_edit\\item_options_screen')
    options.set('footer', '321')
    anchor = MM + 'settings_select\\multiplayer_setup\\item_options_edit\\op_starting_equipment'
    for name, _, _ in IO_ROWS:
        patch_row(options, after=anchor, name=name, widget=f'xbox/{IO}/{name}', help=IO_HELP[name])
        anchor = name
    # Player Options: the four respawn rows leave for RESPAWN OPTIONS, whose row stands in their place
    player = patch(root, MM + 'settings_select\\multiplayer_setup\\player_options_edit\\player_options_menu',
                   MM + 'settings_select\\multiplayer_setup\\player_options_edit\\player_options_screen')
    player.set('footer', '321')
    poe = MM + 'settings_select\\multiplayer_setup\\player_options_edit\\'
    patch_row(player, after=poe + 'op_invisible_players', name='op_respawn', widget=f'xbox/{PO}/op_respawn',
              help=PO_HELP['op_respawn'])
    for key in ('op_respawn_time', 'op_respawn_time_growth', 'op_odd_man_out', 'op_suicide_penalty'):
        patch_row(player, 'remove', child=poe + key)
    # (STARTING EQUIPMENT's rows replace the Xbox's own)
    patch_row(options, 'remove', child=MM + 'settings_select\\multiplayer_setup\\item_options_edit\\op_starting_equipment')
    return root


# The tall copies of the disc's row art for each row kind the engine makes tall (written with the first generated
# row that uses them): the art every row draws, and the first row's where the kind has a top variant.
TALL_ROW_ART = 'xbox/bitmaps/row_36/option_bkds'
TALL_ART = {'option': (TALL_ROW_ART, None),
            'picture': ('xbox/bitmaps/row_36/list_item_bkd', 'xbox/bitmaps/row_36/list_item_bkd_top')}
# Disc lists of a standard row kind that keep the disc's rows, with the reason.
DISC_LIST_EXEMPTIONS = {
    # (the System Link server list: 9 rows that do not fit tall and have no scrolling window; the 0.7.0 browser
    # redesign replaces the screen)
    MM + 'multiplayer_type_select\\connected\\server_list\\server_list':
        'System Link server list: 9 rows, no scrolling window; replaced by the 0.7.0 browser',
}


def tall_disc_rows(documents):
    """Every row of every screen the game shows is the standard tall row, the disc's own included: each disc list
    of a standard row kind (disc_contract.row_kind: option rows, picture-list rows) on a screen the game can show
    (disc_contract.reachable_screens: not one a redirect replaces) that fits with tall rows is marked rows="tall"
    with its row kind and tall art (the game restacks it at the standard pitch and restyles the disc's rows,
    port/linux/game/menu_tags.c), on its patch, or on an empty patch where it had none. A list that would pass its
    limit is left as the disc has it (tools/test_disc_contract.py lists those), and so is one in
    DISC_LIST_EXEMPTIONS."""
    import disc_contract
    import ui_map_tags

    root = documents['patches.xml']
    layout = ui_map_tags.load_layout()
    ours = {w.get('name'): w for document in documents.values() for w in document.iter('widget')}
    bitmaps = {b.get('name') for document in documents.values() for b in document.iter('bitmap')}
    redirects = {source.replace('/', '\\'): target.replace('/', '\\') for source, target in SCREEN_ALWAYS + SCREEN_REPLACED}
    shown = disc_contract.reachable_screens(layout, root, ours, redirects, disc_contract.screens_named_in_the_game())
    patches = {p.get('list'): p for p in root.iter('patch')}
    for path, (_, bottom, limit, screen, kind) in sorted(disc_contract.standard_lists_tall(layout, root, ours).items()):
        if bottom > limit or screen not in shown or path in DISC_LIST_EXEMPTIONS:
            continue
        element = patches.get(path)
        if element is None:
            element = ET.SubElement(root, 'patch', {'list': path, 'screen': screen, 'footer': str(limit)})
        art, top_art = TALL_ART[kind]
        assert {art, top_art} - {None} <= bitmaps, (path, art, top_art)
        element.set('rows', 'tall')
        element.set('row_kind', kind)
        element.set('tall_art', art)
        if top_art:
            element.set('tall_top_art', top_art)


def header_frames(assets):
    """each bitmap's first frame, by name, from the PC look's bitmaps"""
    return {b.get('name'): b.find('frame') for b in source('bitmaps.xml')}


def native_header_size(name, bitmap, widget_type, frames):
    """The (width, height) a header widget is drawn at, or None to leave it.
    Every header widget is as large as its bitmap's frame. The engine scales a
    bitmap to its widget, and the PC screens' headers are 605 or 363 wide and
    59 high for 512x64 pictures: drawn so they stretch the title 1.18 times
    wide (or squeeze it to 0.71), where the Xbox's own widgets show the
    picture as it is. Both looks draw their titles through this rule
    (the PC look's files get it from tools/ce_menus.py)."""
    leaf = name.rsplit('/', 1)[-1]
    bitmap = bitmap.removeprefix('xbox/')
    if widget_type is not None or not (leaf == 'header' or leaf.startswith('header_')):
        return None
    if bitmap in frames:
        frame = frames[bitmap]
        if (frame.get('width'), frame.get('height')) != ('512', '64'):
            return None  # (a wider picture, as the PC's 1024-wide gametype header, is drawn squeezed on purpose)
        return 512, 64
    if leaf == 'header_join_game' and not bitmap:
        return 512, 64  # (holds the browser's four titles)
    return None


def native_header_sizes(documents, assets):
    frames = header_frames(assets)
    for root in documents.values():
        for w in root.iter('widget'):
            size = native_header_size(w.get('name', ''), w.get('bitmap', ''), w.get('type'), frames)
            if size:
                w.set('width', str(size[0]))
                w.set('height', str(size[1]))


def native_header_text(text, frames):
    """the same rule for a menus file's text (the PC look's, written by
    tools/ce_menus.py): each <widget> opening tag keeps its other attributes"""
    def fix(match):
        tag = match.group(0)
        attrs = dict(re.findall(r'(\w+)="([^"]*)"', tag))
        size = native_header_size(attrs.get('name', ''), attrs.get('bitmap', ''), attrs.get('type'), frames)
        if not size:
            return tag
        tag = re.sub(r'\bwidth="[^"]*"', f'width="{size[0]}"', tag, count=1)
        return re.sub(r'\bheight="[^"]*"', f'height="{size[1]}"', tag, count=1)
    return re.sub(r'<widget\b[^>]*>', fix, text)


SCREEN_REPLACED = (('ui/shell/main_menu/settings_select/multiplayer_setup/playlist_select/gametype_select_screen',
                    'pc/xbox/main_menu/settings_select/multiplayer_setup/playlist_select/gametype_select_screen'),)


# (the game's own screens that ours replaces on every platform, the Apple TV too)
SCREEN_ALWAYS = (('ui/shell/' + MP + '/connected/connected_map_select_wrapper',
                  'pc/xbox/' + MP + '/connected/connected_map_select_wrapper'),
                ('ui/shell/' + PE + '/player_profile_edit_screen', 'pc/xbox/settings/settings_screen'),
                ('ui/shell/main_menu/settings_select/player_setup/player_profile_select_screen',
                 'pc/xbox/settings/settings_screen')) + tuple(
    # (one set of rule screens: the game's own, which the type list and SERVER SETUP's GAME TYPE row open, hold
    # the score and team play rows that GAME has; ours are the same rules without them)
    (f'ui/shell/main_menu/settings_select/multiplayer_setup/playlist_edit/{kind}_edit/{kind}_options_screen',
     f'pc/xbox/main_menu/settings_select/multiplayer_setup/playlist_edit/{kind}_edit/{kind}_options_screen')
    for kind in ('ctf', 'koth', 'oddball', 'race', 'slayer')) + (
    # (the game's own list of the five gametypes, "step 1": GAME OPTIONS holds the same five TYPE rows, so
    # whatever still asks for the list, as SERVER SETUP's GAME TYPE row does, gets that page)
    ('ui/shell/main_menu/settings_select/multiplayer_setup/playlist_edit/game_type_select/gametype_select_screen',
     'pc/xbox/main_menu/multiplayer_type_select/game_options_edit/game_options_screen'),
    # (the game's gametype editor, and CREATE NEW's screen, which holds only that editor as its child: ours has
    # its rows as tall picture rows, GAME OPTIONS in GAME TYPE's place)
    ('ui/shell/main_menu/settings_select/multiplayer_setup/playlist_edit/gametype_edit_screen',
     'pc/' + GAMETYPE_EDIT_SCREEN),
    ('ui/shell/main_menu/settings_select/multiplayer_setup/create_and_edit_new_gametype_screen',
     'pc/' + GAMETYPE_EDIT_SCREEN),) + tuple(
    (f'ui/shell/error/{name}', f'pc/xbox/error/{name}')
    for name in ERROR_DIALOGS + ABORT_DIALOGS + WAIT_DIALOGS + ('confirm_delete_profile_modal_fullscreen',
                                                      'confirm_delete_gametype_modal_fullscreen'))


def equivalence_header():
    lines = ['/* Generated by tools/xbox_menus.py from the Xbox menu design inventory. */',
             '#ifndef MENU_EQUIVALENCE_H', '#define MENU_EQUIVALENCE_H', '#include <string.h>',
             '#include <stdlib.h>',
             # (the game's own screens that an Xbox-look screen of ours replaces: not on the Apple TV, whose
             # Siri Remote has no X and Y buttons for CREATE NEW and DELETE)
             'static char const *const menu_screen_replaced[][2] = {']
    for original, ours in SCREEN_REPLACED:
        lines.append('\t{' + json.dumps(original.replace('/', '\\')) + ', ' + json.dumps(ours.replace('/', '\\')) + '},')
    lines += ['};', 'static char const *const menu_screen_always[][2] = {']
    for original, ours in SCREEN_ALWAYS:
        lines.append('\t{' + json.dumps(original.replace('/', '\\')) + ', ' + json.dumps(ours.replace('/', '\\')) + '},')
    lines += ['};', 'static char const *menu_screen_equivalent(char const *name)', '{',
              '\tunsigned int index;',
              '\tfor (index = 0; index < sizeof(menu_screen_always) / sizeof(menu_screen_always[0]); index++)',
              '\t\tif (!strcmp(name, menu_screen_always[index][0]))',
              '\t\t\treturn menu_screen_always[index][1];',
              '\tchar const *tv = getenv("HALO_HOST_TVOS");',
              '\tfor (index = 0; !(tv && !strcmp(tv, "1")) && index < sizeof(menu_screen_replaced) / sizeof(menu_screen_replaced[0]); index++)',
              '\t\tif (!strcmp(name, menu_screen_replaced[index][0]))',
              '\t\t\treturn menu_screen_replaced[index][1];', '\treturn NULL;', '}', '#endif', '']
    return '\n'.join(lines)


# Generated option lists that keep their own row geometry, with the reason. Every other generated list whose
# rows are option rows (the option_bkds row art) takes the standard row: tall (36 units of art on the 39 pitch),
# so a row is tall because it is a generated row, never because its screen asked.
OPTION_LIST_EXEMPTIONS = {
    # the browser's server rows: its own row art, pitch and pager (browser_layout)
    f'xbox/{MP}/join_game/join_game_items_list': 'server rows, laid out by browser_layout',
    # the entry forms: their help sits right under the field rows; tall rows need the help moved first
    f'xbox/{MP}/join_game/password/options_menu': 'entry form; help box at 156 needs moving',
    f'xbox/{MP}/join_game/code/options_menu': 'entry form; help box at 156 needs moving',
    f'xbox/{MP}/direct_ip/direct_ip_menu': 'entry form; help box needs moving',
}


def is_option_row(row):
    """A list child drawn as an option row: the option_bkds art, or a comfortable copy of it."""
    return row is not None and row.get('bitmap', '').rsplit('/', 1)[-1] in ('option_bkds', 'option_bkds_sm')


def is_picture_row(row):
    """A list child drawn as a picture-list row (the list_item_bkd art of lists with a picture panel beside them)."""
    return row is not None and row.get('bitmap', '').rsplit('/', 1)[-1] in ('list_item_bkd', 'list_item_bkd_top')


def is_option_list(options, widgets):
    """A generated column list of option rows, not exempted."""
    # (a bar of buttons side by side, left_right_tabs_items, is not a list of rows)
    return (options.get('type') == 'column_list' and options.get('name') not in OPTION_LIST_EXEMPTIONS
            and 'left_right_tabs_items' not in options.get('flags', '')
            and any(is_option_row(widgets.get(c.get('widget'))) for c in options.findall('child')
                    if 'button_bar' not in c.get('widget', '') and '/scroll_' not in c.get('widget', '')))


def density_rows(options, widgets):
    option_list = is_option_list(options, widgets)
    return [c for c in options.findall('child')
            if c.get('y') is not None and c.get('widget') in widgets
            and (re.fullmatch(r'(?:list_item_|server_item_)\d+(?:_map_kind)?',
                              c.get('widget').rsplit('/', 1)[-1])
                 or ((options.get('name') in OPTION_LIST_LAYOUTS or option_list)
                     and '/scroll_' not in c.get('widget') and 'button_bar' not in c.get('widget')
                     and int(c.get('y')) < LEGEND_Y)
                 or (is_picture_row(widgets.get(c.get('widget'))) and '/scroll_' not in c.get('widget'))
                 or (options.get('name', '').startswith('xbox/settings/')
                     and options.get('name', '').endswith('/options_menu')
                     and 'button_bar' not in c.get('widget') and '/scroll_' not in c.get('widget')))]


def make_row_tall(row, bitmaps, documents, selected):
    """Draw a row 36 units tall inside its 39-unit tap band (its art swapped for the row_36 variant, its text and
    parts moved down to match). Every row that sits on the comfortable pitch takes this, whichever list or patch
    row puts it there."""
    drawn = 36 if row.get('height') == str(COMFORTABLE_PITCH) else 28
    # The game's own option_bkds art (128 px, transparent padding) has a 96 px
    # band when unselected and 112 px when selected: draw the frame taller so the
    # visible band is 36 and 39 units (59 and 53 units drawn, scaled from the measured 94 px band against 102 px), centered on the pitch.
    fitted = (drawn == 36 and row.get('bitmap') is not None
              and bitmaps[row.get('bitmap')].find('frame') is not None
              and all(f.get('map') and f.get('png', '').endswith(('option_bkds__0.png', 'option_bkds__1.png'))
                      for f in bitmaps[row.get('bitmap')].findall('frame')))
    small = (drawn == 36 and row.get('bitmap') is not None
             and bitmaps[row.get('bitmap')].find('frame') is not None
             and all(not f.get('map') and f.get('png', '').endswith(('option_bkds_sm__0.png', 'option_bkds_sm__1.png'))
                     for f in bitmaps[row.get('bitmap')].findall('frame')))
    shift = 5 if fitted else (drawn - 28) // 2
    if not shift:
        return
    if row.get('type') == 'text' or row.get('text_y') is not None:
        row.set('text_y', str(int(row.get('text_y', 0)) + shift))
    for part in row.findall('child'):
        part.set('y', str(int(part.get('y', 0)) + shift))
    name = row.get('bitmap')
    if name:
        # Keep the leaf name so ui_redraw recognizes the frame.
        prefix, leaf = name.rsplit('/', 1)
        variant = f'{prefix}/row_{drawn}/{leaf}'
        owner = next((r for r in selected if any(
            b.get('name') == name for b in r.findall('bitmap'))), documents['bitmaps.xml'])
        if not any(b.get('name') == variant for b in owner.findall('bitmap')):
            bitmap = deepcopy(bitmaps[name])
            bitmap.set('name', variant)
            for index, frame in enumerate(bitmap.findall('frame')):
                if fitted or small:
                    # Heights span the measured visible band (36 unselected, 39
                    # selected), so the padding around it is transparent overlap.
                    height = (59, 53)[index] if fitted else (56, 52)[index]
                    frame.set('height', str(height))
                    frame.set('y', str((-10, -7)[index] if fitted else (-9, -7)[index]))
                else:
                    frame.set('height', str(drawn))
            owner.append(bitmap)
        row.set('bitmap', variant)
        if drawn == 36:
            # (said to the engine outright: it draws the art 36 units tall in the 39-unit band)
            row.set('row', 'tall')


def tall_patch_widgets(documents, bitmaps):
    """The widgets patch rows put into the disc's lists sit on no list of ours, so the density pass never reaches
    them: each is made the standard tall row, as the rows of the lists that do. The game restacks the list itself
    (rows="tall" on the patch), so the disc's rows and ours stand a standard pitch apart together."""
    selected = list(documents.values())
    widgets = {w.get('name'): w for root in selected for w in root.iter('widget')}
    for row in documents['patches.xml'].iter('row'):
        widget = widgets.get(row.get('widget'))
        if widget is None or widget.get('row') == 'tall':
            continue
        widget.set('height', str(COMFORTABLE_PITCH))
        widget.set('top', '0')
        make_row_tall(widget, bitmaps, documents, selected)


def apply_density(documents, pitch, bitmaps, settings=False):
    widgets = {w.get('name'): w for root in documents.values() for w in root.iter('widget')}
    for root in documents.values():
        for options in list(root.iter('widget')):
            if options.get('type') != 'column_list':
                continue
            placements = density_rows(options, widgets)
            if not placements:
                continue
            is_settings = (options.get('name', '').startswith('xbox/settings/')
                           and options.get('name', '').endswith('/options_menu'))
            if is_settings != settings:
                continue
            list_spec = LIST_LAYOUTS.get(options.get('name'), {})
            row_pitch = list_spec.get('row_pitch', pitch)
            first = min(int(c.get('y')) for c in placements)
            slots = sorted({int(c.get('y')) for c in placements})
            if options.get('row_pitch'):
                options.set('row_pitch', str(row_pitch))
            positions = list(rows(0, first, row_pitch, len(slots)))
            for c in placements:
                rank = slots.index(int(c.get('y')))
                c.set('y', str(positions[rank][1]))
                row = widgets[c.get('widget')]
                row.set('height', str(row_pitch))
                row.set('top', '0')
            # The browser updater pages through its actual child slots; other
            # lists retain their slots and use the widget viewport scroller.
            browser = options.get('name', '').endswith('/join_game_items_list')
            limit = SCREEN_LAYOUTS[f'xbox/{MP}/join_game/join_game_screen']['list_limit'] if browser else LEGEND_Y
            desc = widgets.get(options.get('description'))
            if desc is None and is_settings:
                # ABOUT's legal help is fixed text; its rows still stop above it.
                desc = widgets.get(options.get('name').rsplit('/', 1)[0] + '/help')
            if desc is not None and desc.get('top') is not None and (
                    is_settings or options.get('name') in OPTION_LIST_LAYOUTS or is_option_list(options, widgets)):
                limit = int(desc.get('top'))
            fit = max(1, (limit - first) // row_pitch)
            for c in list(options.findall('child')):
                if '/scroll_' in c.get('widget', ''):
                    options.remove(c)
                    old = widgets[c.get('widget')]
                    if old in list(root):
                        root.remove(old)
            if not is_settings:
                if browser:
                    for c in placements:
                        if int(c.get('y')) >= first + fit * row_pitch:
                            options.remove(c)
                options.attrib.pop('visible_rows', None)
                platforms = {c.get('platform') for c in placements}
                if any(platforms) and all(is_picture_row(widgets.get(c.get('widget'))) for c in placements):
                    # Pack platform-filtered rows even when every row fits: the window is the most rows one
                    # platform shows, and a row and its '!platform' twin never show together.
                    twins = sum(1 for p in platforms if p and p.startswith('!') and p[1:] in platforms)
                    options.set('visible_rows', str(len(placements) - twins))
                elif browser:
                    options.set('visible_rows', str(min(fit, len(slots))))
                elif len(slots) > fit:
                    options.set('visible_rows', str(fit))
                    arrows_needed(options, root, bitmaps, first, fit, row_pitch)()
            else:
                pending = fit_window(options, widgets, limit, root, bitmaps, always=bool(options.get('row_pitch')),
                                     pitch=row_pitch)
                if pending:
                    pending()
            options.set('height', str(limit))


def lead_kept_rows(document):
    """The game packs the rows a platform keeps from the first kept row's place at the pitch between the
    first two kept rows, and the rows other platforms drop are gone by then, so the rows every platform keeps
    that follow platform-only rows take the leading places (VIDEO SETUP's V-SYNC)."""
    for options in document.iter('widget'):
        if options.get('type') != 'column_list' or not options.get('name', '').startswith('xbox/settings/'):
            continue
        rows = [r for r in options.findall('child') if r.get('y') and not r.get('widget').endswith('/button_bar')
                and '/scroll_' not in r.get('widget')]
        slots = sorted({int(r.get('y')) for r in rows})
        if len(slots) < 2:
            continue
        # each platform keeps the platform-less rows and its own; "desktop" keeps its own leading rows already
        for platform in sorted({r.get('platform') for r in rows if r.get('platform') not in (None, 'desktop')}) + [None]:
            kept = [r for r in rows if r.get('platform') in (None, platform)]
            if len(kept) < 2 or kept[0] is rows[0]:
                continue
            for index, row in enumerate(kept[:2]):
                row.set('y', str(slots[0] + index * (slots[1] - slots[0])))


def generate(assets=MENUS):
    assets = Path(assets)
    names = set()
    documents = {f'settings.{page}.xml': settings_page(assets, page, names) for page in PAGES}
    documents['settings.xml'] = settings_hub(names, assets)
    documents['main_menu.settings_select.multiplayer_setup.playlist_edit.xml'] = gametype_editor(names)
    # Keyboard and mouse form one entry from the hub and profile edit screen.
    keyboard = documents['settings.controls_setup.xml']
    base = 'xbox/settings/controls_setup'
    options = next(w for w in keyboard if w.get('name') == f'{base}/options_menu')
    mouse_row = ET.Element('child', widget=f'{base}/mouse_setup', x='54', y=str(FIRST_ROW + 8 * COMFORTABLE_PITCH))
    options.insert(len(options) - 1, mouse_row)
    mouse = text_widget(keyboard, f'{base}/mouse_setup', 'MOUSE SETUP', width=512, height=28,
                        bitmap=bitmap_ref('bitmaps/option_bkds', names), text_x=13, text_y=4)
    for event in ('a', 'start'):
        ET.SubElement(mouse, 'on', event=event, open='xbox/settings/mouse_settings/mouse_settings_screen')
    documents.update(multiplayer_documents(assets, names))
    documents['dialogs.xml'] = dialog_document()
    shared = documents['multiplayer.shared.xml']
    for kind in ('profile', 'gametype', 'join_big'):
        base = 'xbox/error/confirm_' + (kind if kind == 'join_big' else 'delete_' + kind)
        panel = shared.find(f"widget[@name='{base}']")
        # Frame, text, and warning title keep the same order.
        for part in list(panel.findall('child'))[3:]:
            panel.remove(part)
        modal = shared.find(f"widget[@name='{base}_modal_fullscreen']")
        dialog_legend_targets(shared, modal, base + '/legend', cancel=True)
        if kind == 'gametype':
            # These PC button definitions are no longer referenced by a confirmation.
            for leaf in ('a_butn', 'a_caption', 'b_butn', 'b_caption'):
                stale = shared.find(f"widget[@name='xbox/error/confirm_delete_gametype/{leaf}']")
                if stale is not None:
                    shared.remove(stale)
    # The browser's private error panel predates the engine error screens.
    # Keep its child topology when error_modal becomes a standalone screen.
    shared.find("widget[@name='xbox/error/error_modal']").set('name', 'xbox/error/browser_error_panel')
    for document in documents.values():
        for part in document.iter('child'):
            if part.get('widget') == 'xbox/error/error_modal':
                part.set('widget', 'xbox/error/browser_error_panel')
    documents['main_menu.settings_select.multiplayer_setup.item_options_edit.port.xml'] = item_options_document(
        assets, names)
    documents['main_menu.settings_select.multiplayer_setup.player_options_edit.port.xml'] = item_options_document(
        assets, names, PO, PO_ROWS)
    documents['patches.xml'] = patch_document(names)
    documents['bitmaps.xml'] = bitmap_document(assets, names)
    browser_error_layout(documents)
    action_bar_legends(documents)
    layout_geometry(documents)
    standardize_spinners(documents)
    apply_density(documents, COMFORTABLE_PITCH, names, settings=True)
    apply_density(documents, COMFORTABLE_PITCH, names)
    for name, document in documents.items():
        if name.startswith('settings.'):
            lead_kept_rows(document)
    # The shared row art stays 28 units; a tall row draws its own 36-unit copy inside the full 39-unit tap band.
    bitmaps = {b.get('name'): b for root in documents.values() for b in root.findall('bitmap')}
    selected = list(documents.values())
    widgets = {w.get('name'): w for root in selected for w in root.iter('widget')}
    row_art = {widgets[c.get('widget')].get('bitmap')
               for root in selected for options in root.iter('widget')
               if options.get('type') == 'column_list' for c in density_rows(options, widgets)}
    for root in documents.values():
        for bitmap in root.findall('bitmap'):
            if bitmap.get('name') in row_art:
                for frame in bitmap.findall('frame'):
                    frame.set('height', '28')
    adjusted = set()
    for root in selected:
        for options in root.iter('widget'):
            if options.get('type') != 'column_list':
                continue
            for c in density_rows(options, widgets):
                row = widgets[c.get('widget')]
                if id(row) in adjusted:
                    continue
                adjusted.add(id(row))
                make_row_tall(row, bitmaps, documents, selected)
    add_advanced_clones(documents)
    tall_disc_rows(documents)
    tall_patch_widgets(documents, bitmaps)
    native_header_sizes(documents, assets)
    # every screen's title as text (tools/menu_titles.py): the generated screens' beside their widgets, the disc's
    # in a file of their own
    documents['titles.xml'] = ET.Element('menus')
    menu_titles.declare(documents, documents['titles.xml'])
    for root in documents.values():
        validate(root, assets)
    output = assets / 'xbox'
    output.mkdir(parents=True, exist_ok=True)
    for filename, root in documents.items():
        (output / filename).write_text(menu_catalog.add_ids(serialize(root), Path(filename).stem, 'xbox'), encoding='utf-8')
    manifest_path = assets / 'menus.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    manifest['comment'] = "The menus' files the game embeds (tools/xbox_menus.py, tools/embed_assets.py)."
    manifest['files'] = manifest_files(assets)
    manifest_path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--menus', type=Path, default=MENUS, help='menu assets folder')
    args = parser.parse_args()
    generate(args.menus)
    (ROOT / 'port/linux/include/menu_equivalence.h').write_text(equivalence_header(), encoding='utf-8')
    menu_geometry.HEADER.write_text(menu_geometry.header(), encoding='utf-8')


if __name__ == '__main__':
    main()
