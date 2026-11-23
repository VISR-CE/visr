"""Every screen's title is declared by data (`<title>`, tools/menu_titles.py), and the engine finds titles by that
role alone: never by a widget's name, its picture's path or its picture's size."""

import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
XBOX = ROOT / 'port/assets/menus/xbox'


def declared():
    titles, strings = {}, {}
    for path in sorted(XBOX.glob('*.xml')):
        root = ET.parse(path).getroot()
        for element in root.findall('strings'):
            strings[element.get('name')] = [s.get('text') for s in element.findall('string')]
        for element in root.findall('title'):
            assert element.get('widget') not in titles, f'{element.get("widget")} is declared twice'
            titles[element.get('widget')] = element.get('strings')
    return titles, strings


def test_every_title_picture_is_a_declared_title():
    titles, strings = declared()
    for path in sorted(XBOX.glob('*.xml')):
        for widget in ET.parse(path).getroot().iter('widget'):
            leaf = widget.get('bitmap', '').rsplit('/', 1)[-1]
            if leaf.startswith('header') and widget.get('type') != 'text_box':
                assert widget.get('name') in titles, f'{path.name}: {widget.get("name")} draws a title picture'
    layout = json.loads((ROOT / 'tools/fixtures/disc_menu_layout.json').read_text())
    for name in layout:
        leaf = name.rsplit('\\', 1)[-1]
        if leaf.startswith('header') and '\\error\\' not in name:
            assert name in titles, f'the disc title {name} is not declared'
    for widget, name in titles.items():
        assert strings.get(name) and all(strings[name]), f'{widget}: its title has no text'


def test_each_title_text_has_a_catalog_id():
    for path in sorted(XBOX.glob('*.xml')):
        root = ET.parse(path).getroot()
        names = {t.get('strings') for t in root.findall('title')}
        for element in root.findall('strings'):
            if element.get('name') in names:
                assert all(s.get('id') for s in element.findall('string')), element.get('name')


def test_the_engine_finds_titles_by_role_only():
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    start = source.index('static struct bitmap_data *menu_title_icon(')
    body = source[start:source.index('\n}\n', start)]
    assert 'HALO_MENU_ROLE_TITLE' in body
    for guess in ('header', 'shell', '->width', '->height', 'tag_get_name', 'strncmp'):
        assert guess not in body, f'menu_title_icon guesses a title by {guess!r}'
    assert not re.search(r'menu_title_(?:icon|touch_shift|icon_draw)\(\s*definition->', source)


def test_each_title_has_one_string_for_each_frame_of_its_picture():
    titles, strings = declared()
    frames, bitmaps = {}, {}
    for path in sorted(XBOX.glob('*.xml')):
        root = ET.parse(path).getroot()
        for bitmap in root.findall('bitmap'):
            frames[bitmap.get('name')] = len(bitmap.findall('frame'))
        for widget in root.iter('widget'):
            bitmaps[widget.get('name')] = widget.get('bitmap')
    layout = json.loads((ROOT / 'tools/fixtures/disc_menu_layout.json').read_text())
    disc_frames = {}
    for asset in json.loads((ROOT / 'port/assets/titles/titles.json').read_text())['assets']:
        disc_frames.setdefault(asset['tag'], set()).add(asset['bitmap'])
    for widget, name in titles.items():
        if '\\' in widget:
            count = len(disc_frames.get(layout[widget]['bitmap'], ()))
        else:
            count = frames.get(bitmaps.get(widget), 0)
        assert count and len(strings[name]) == count, f'{widget}: {len(strings[name])} strings, {count} frames'
