"""Xbox settings generation preserves wiring and needs no extracted assets."""
import importlib.util
import json
from pathlib import Path
import shutil
import sys
from xml.etree import ElementTree as ET

import pytest

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'
sys.path.insert(0, str(ROOT / 'tools'))
from ui_map_tags import check_menus


def generator():
    spec = importlib.util.spec_from_file_location('xbox_menus', ROOT / 'tools/xbox_menus.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def snapshot(path):
    return {p.relative_to(path).as_posix(): p.read_bytes() for p in path.rglob('*') if p.is_file()}


def test_generation_is_deterministic_and_checked_in(tmp_path):
    assets = tmp_path / 'menus'
    shutil.copytree(MENUS / 'ce', assets / 'ce')
    shutil.copy(MENUS / 'menus.json', assets / 'menus.json')
    module = generator()
    module.generate(assets)
    first = snapshot(assets)
    module.generate(assets)
    assert snapshot(assets) == first
    assert snapshot(assets / 'xbox') == snapshot(MENUS / 'xbox')
    assert (assets / 'menus.json').read_bytes() == (MENUS / 'menus.json').read_bytes()
    assert not check_menus(assets / 'xbox')
    manifest = json.loads((assets / 'menus.json').read_text())['files']
    assert len(manifest) == len(set(manifest))
    for file in (assets / 'xbox').glob('*.xml'):
        assert file.relative_to(assets).as_posix() in manifest
        for frame in ET.parse(file).iter('frame'):
            assert frame.get('png') in manifest
    # ce_menus passes its newly drawn PNG list through the same retention step.
    retained = module.retain_xbox_files(assets, ['ce/bitmaps.xml'])
    assert set(manifest) & {p for p in manifest if p.startswith('xbox/')} <= set(retained)
    assert 'ce/port/pause/pausebox_center__1.png' in retained


def test_settings_keep_pc_values_and_handlers():
    module = generator()
    for folder in module.PAGES:
        source = ET.parse(MENUS / 'ce' / f'{module.PE.replace("/", ".")}.{folder}.xml')
        output = ET.parse(MENUS / 'xbox' / f'settings.{folder}.xml')
        expected = {w.get('setting'): (w.get('values'), w.get('strings'))
                    for w in source.iter('widget') if w.get('setting')}
        actual = {w.get('setting'): (w.get('values'), w.get('strings'))
                  for w in output.iter('widget') if w.get('setting')}
        assert actual == expected
        assert {e.get('run') for e in source.iter('on') if e.get('run')} <= {
            e.get('run') for e in output.iter('on') if e.get('run')}
        names = {w.get('name').rsplit('/', 1)[-1] for w in output.iter('widget')}
        assert {'a_butn', 'b_butn', '=accept', '=cancel'} <= names
        assert all('\\' not in w.get('name') for w in output.iter('widget'))
    hub = ET.parse(MENUS / 'xbox/settings.xml')
    assert {w.get('text') for w in hub.iter('widget') if w.find('on[@open]') is not None} == {
        'CONTROLS', 'VIDEO', 'AUDIO', 'NETWORK', 'KEYBOARD & MOUSE'}
    keyboard = next(w for w in hub.iter('child') if w.get('widget') == 'xbox/settings/keyboard')
    assert keyboard.get('platform') == 'keyboard'


def test_missing_fallback_is_rejected(tmp_path):
    module = generator()
    root = ET.fromstring('<menus><bitmap name="bad"><frame map="ui\\bad"/></bitmap></menus>')
    with pytest.raises(ValueError, match='fallback'):
        module.validate(root, tmp_path)


def test_generator_rejects_map_widget_definitions(tmp_path):
    module = generator()
    root = ET.fromstring('<menus><widget name="ui\\shell\\screen" /></menus>')
    with pytest.raises(ValueError, match='map widget'):
        module.validate(root, tmp_path)


def test_save_is_a_focusable_action_separate_from_a_legend():
    for path in (MENUS / 'xbox').glob('settings.*.xml'):
        widgets = {w.get('name'): w for w in ET.parse(path).iter('widget')}
        save = next(w for n, w in widgets.items() if n.endswith('/button_ok'))
        # ui_mouse_key_button posts A to the existing focus; a SAVE item must
        # get the normal item target, which focuses it before posting A.
        assert save.get('type') == 'text'
        assert save.get('text') == 'SAVE'
        assert not save.findall('child')
        assert save.find('on[@event="a"][@back="true"]') is not None
        accept = next(w for n, w in widgets.items() if n.endswith('/=accept'))
        assert accept.get('text') == 'SELECT'
