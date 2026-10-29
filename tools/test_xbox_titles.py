"""The Xbox look's titles are drawn from high-resolution pictures this project ships, none stretched."""
import json
from pathlib import Path
import struct
import sys
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'
TITLES = ROOT / 'port/assets/titles'
sys.path.insert(0, str(ROOT / 'tools'))

import pytest


def title_assets_module():
    """`tools/title_assets.py` (it needs numpy and Pillow, which the portable CI job does not install)"""
    pytest.importorskip('numpy')
    pytest.importorskip('PIL')
    import title_assets
    return title_assets

SCALE = 4


def png_size(path):
    header = path.read_bytes()[:24]
    assert header[:8] == b'\x89PNG\r\n\x1a\n', path
    return struct.unpack('>II', header[16:24])


def frames():
    """the Xbox look's bitmaps (name -> first frame)"""
    return {b.get('name'): b.find('frame') for b in ET.parse(MENUS / 'xbox/bitmaps.xml').getroot()}


def header_widgets():
    for file in sorted((MENUS / 'xbox').glob('*.xml')):
        for w in ET.parse(file).iter('widget'):
            leaf = w.get('name', '').rsplit('/', 1)[-1]
            if (leaf == 'header' or leaf.startswith('header_')) and w.get('bitmap') and w.get('type') is None:
                yield file.name, w


def test_every_xbox_xml_title_is_a_shipped_high_resolution_picture():
    bitmaps = frames()
    seen = 0
    for file, w in header_widgets():
        frame = bitmaps[w.get('bitmap')]
        # (each title is a PNG of its own, not the map's bitmap)
        assert frame.get('map') is None, (file, w.get('name'))
        png = MENUS / frame.get('png')
        assert png.is_file(), (file, w.get('name'), png)
        width, height = png_size(png)
        assert width >= SCALE * int(frame.get('width')) and height >= SCALE * int(frame.get('height')), \
            (png.name, width, height)
        seen += 1
    assert seen >= 20


def test_no_title_widget_scales_its_picture_unevenly():
    bitmaps = frames()
    for file, w in header_widgets():
        frame = bitmaps[w.get('bitmap')]
        picture = (int(frame.get('width')), int(frame.get('height')))
        widget = (int(w.get('width')), int(w.get('height')))
        if picture == (512, 64):
            assert widget == picture, (file, w.get('name'), widget)


def test_every_title_the_maps_hold_has_a_shipped_picture():
    title_assets = title_assets_module()
    listed = json.loads((TITLES / 'titles.json').read_text())['assets']
    by_tag = {}
    for entry in listed:
        by_tag.setdefault(entry['tag'], []).append(entry)
        assert (TITLES / (entry['name'] + '.png')).is_file(), entry['name']
        width, height = png_size(TITLES / (entry['name'] + '.png'))
        assert (width, height) == (entry['width'] * entry['scale'], entry['height'] * entry['scale']), entry['name']
        assert entry['scale'] >= 2
    for tag, texts in title_assets.TEXTS.items():
        assert len(by_tag[tag]) == len(texts), tag
        assert [e['text'] for e in sorted(by_tag[tag], key=lambda e: e['bitmap'])] == texts
    # the name-entry screen's ENTER NAME is in its backdrop's picture
    assert [e['text'] for e in by_tag['ui\\bkd_virtual_keyboard']] == ['ENTER NAME']
    assert 'bkd_virtual_keyboard.svg' in title_assets.BACKGROUNDS.values()
