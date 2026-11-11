"""Density variants keep tap bands contiguous and settings inside their viewport."""
import json
import re
from pathlib import Path
from xml.etree import ElementTree as ET

import pytest

from test_xbox_menus import generator

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus'


def selected_documents(compact):
    files = list((MENUS / 'xbox').glob('*.xml'))
    return [p for p in files if ('.compact.' in p.name if compact else '.compact.' not in p.name)
            or (compact and '.compact.' not in p.name
                and not p.with_suffix('.compact.xml').exists())]


def test_pitch_constants():
    module = generator()
    assert module.COMFORTABLE_PITCH == 39
    assert module.COMPACT_PITCH == 30
    assert module.DENSITY_PITCH == {'compact': 30, 'comfortable': 39}


def test_every_long_list_has_a_manifest_twin():
    expected = {'main_menu.multiplayer_type_select.join_game',
                'main_menu.multiplayer_type_select.lobby', 'main_menu.new_select.port',
                'main_menu.multiplayer_type_select.mp_map_select',
                'main_menu.settings_select.multiplayer_setup.playlist_select',
                'settings.controls_setup'}
    manifest = json.loads((MENUS / 'menus.json').read_text())['files']
    for name in expected:
        for suffix in ('.xml', '.compact.xml'):
            path = 'xbox/' + name + suffix
            assert (MENUS / path).is_file()
            assert path in manifest


@pytest.mark.parametrize('compact', [False, True])
def test_selected_density_has_unique_definitions_and_tiled_tap_bands(compact):
    documents = [ET.parse(p).getroot() for p in selected_documents(compact)]
    definitions = [(e.tag, e.get('name')) for root in documents for e in root if e.get('name')]
    assert len(definitions) == len(set(definitions))
    widgets = {w.get('name'): w for root in documents for w in root.iter('widget')}
    bitmaps = {b.get('name'): b for root in documents for b in root.iter('bitmap')}
    pitch = 30 if compact else 39
    seen = 0
    for options in widgets.values():
        if options.get('type') != 'column_list':
            continue
        rows = [c for c in options.findall('child')
                if re.fullmatch(r'(?:list_item_|server_item_)\d+(?:_map_kind)?',
                                c.get('widget').rsplit('/', 1)[-1])]
        if not rows:
            continue
        seen += 1
        slots = sorted({int(c.get('y', 0)) for c in rows})
        assert all(b - a == pitch for a, b in zip(slots, slots[1:])), options.get('name')
        for child in rows:
            row = widgets[child.get('widget')]
            assert int(row.get('top', 0)) == 0
            assert int(row.get('height')) == pitch
            if row.get('bitmap'):
                heights = {f.get('height') for f in bitmaps[row.get('bitmap')].findall('frame')}
                # Padded option art is drawn taller so its band is 36/39 units.
                assert heights <= ({'28'} if compact else {'36', '59', '53', '56', '52'})
        visible = int(options.get('visible_rows', len(slots)))
        assert slots[0] + visible * pitch <= (338 if options.get('name').endswith('/join_game_items_list') else 414)
    assert seen >= 5


def test_fixed_settings_fit_and_use_comfortable_tap_bands():
    module = generator()
    for page in module.PAGES:
        root = ET.parse(MENUS / 'xbox' / f'settings.{page}.xml')
        widgets = {w.get('name'): w for w in root.iter('widget')}
        options = next(w for w in widgets.values() if w.get('name').endswith('/options_menu'))
        rows = [c for c in options.findall('child')
                if 'button_bar' not in c.get('widget') and '/scroll_' not in c.get('widget')]
        slots = sorted({int(c.get('y')) for c in rows})
        # (a row that leads the list on a platform that drops the rows before it may skip past a place)
        assert all((b - a) % 39 == 0 for a, b in zip(slots, slots[1:]))
        slots = list(range(slots[0], slots[-1] + 1, 39))
        assert all(widgets[c.get('widget')].get('height') == '39' for c in rows)
        help_top = int(next(w for w in widgets.values() if w.get('name').endswith('/help')).get('top'))
        assert slots[0] + int(options.get('visible_rows', len(slots))) * 39 <= help_top


def test_comfortable_art_centers_labels_and_controls():
    root = ET.parse(MENUS / 'xbox/settings.audio_settings.xml')
    widgets = {w.get('name').rsplit('/', 1)[-1]: w for w in root.iter('widget')}
    row = widgets['op_volume']
    children = {c.get('widget').rsplit('/', 1)[-1]: c for c in row.findall('child')}
    # The art's band is centered in the 39-unit row: a 20-unit text line has
    # nine units above it.
    label = widgets['volume_label']
    assert int(children['volume_label'].get('y')) + int(label.get('text_y')) == 9
    spinner = widgets['volume_spinner']
    assert (int(children['volume_spinner'].get('y')) + int(spinner.get('top'))
            + int(spinner.get('text_y'))) == 9
    assert row.get('height') == '39'
    assert '/row_36/' in row.get('bitmap')


def test_runtime_draws_row_art_inside_the_tap_band(tmp_path):
    import shutil
    import subprocess

    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required for the row drawing probe')
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    start = source.index('const char *row_bitmap_name =')
    end = source.index('\n\n', start)
    clamp = source[start:end]
    harness = r'''
#include <assert.h>
#include <string.h>
#define MIN(a, b) ((a) < (b) ? (a) : (b))
typedef struct { short x0, y0, x1, y1; } rectangle2d;
static const char *bitmap_name;
const char *tag_get_name(int index) { (void)index; return bitmap_name; }
static rectangle2d draw_bounds(const rectangle2d *tap) {
    struct { struct { int index; } background_bitmap; } instance = {{0}};
    const typeof(instance) *definition = &instance;
    rectangle2d bounds = *tap;
    __CLAMP__
    return bounds;
}
int main(void) {
    rectangle2d tap = {0, 0, 512, 39};
    bitmap_name = "xbox/bitmaps/row_36/option_bkds";
    rectangle2d art = draw_bounds(&tap);
    assert(art.y1 - art.y0 == 36 && tap.y1 - tap.y0 == 39);
    bitmap_name = "xbox/browser/row_36/row";
    assert(draw_bounds(&tap).y1 == 36);
    bitmap_name = "ui\\port\\xbox\\bitmaps\\row_36\\list_item_bkd";
    assert(draw_bounds(&tap).y1 == 36);
    tap.y0 = 73; tap.y1 = 112;
    assert(draw_bounds(&tap).y1 == 109);
    tap.y1 = 90;
    assert(draw_bounds(&tap).y1 == 90);
    bitmap_name = "xbox/bitmaps/option_bkds";
    tap.y0 = 0; tap.y1 = 30;
    assert(draw_bounds(&tap).y1 == 30);
    bitmap_name = NULL;
    assert(draw_bounds(&tap).y1 == 30);
    return 0;
}
'''.replace('__CLAMP__', clamp)
    path, binary = tmp_path / 'row.c', tmp_path / 'row'
    path.write_text(harness)
    subprocess.run([compiler, '-Wall', '-Wextra', '-Werror', str(path), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_pause_settings_hub_uses_comfortable_rows():
    module = generator()
    root = ET.parse(MENUS / 'xbox/settings.xml')
    widgets = {w.get('name'): w for w in root.iter('widget')}
    bitmaps = {b.get('name'): b for b in ET.parse(MENUS / 'xbox/bitmaps.xml').iter('bitmap')}
    options = widgets['xbox/settings/settings_list']
    rows = module.density_rows(options, widgets)
    assert len(rows) == len(module.PAGE_ROWS)
    slots = sorted({int(c.get('y')) for c in rows})
    assert slots == [78 + i * 39 for i in range(len(slots))]
    for child in rows:
        row = widgets[child.get('widget')]
        assert row.get('height') == '39'
        assert row.get('text_y') == '8'
        assert '/row_36/' in row.get('bitmap')
        assert all(f.get('height') == '36' for f in bitmaps[row.get('bitmap')].findall('frame'))
    # The short hub has the same rows in both runtime density selections.
    assert not (MENUS / 'xbox/settings.compact.xml').exists()


def test_png_only_row_frames_carry_height_and_offset():
    import re
    xml = (ROOT / 'port/assets/menus/xbox/bitmaps.xml').read_text()
    block = xml[xml.index('name="xbox/bitmaps/row_36/option_bkds_sm"'):]
    frames = re.findall(r'<frame [^>]*/>', block.split('</bitmap>')[0])
    assert len(frames) == 2 and all('map=' not in f for f in frames)
    assert 'height="56"' in frames[0] and 'y="-9"' in frames[0]
    assert 'height="52"' in frames[1] and 'y="-7"' in frames[1]
    reader = (ROOT / 'port/linux/src/menu_files.c').read_text()
    assert '(frame.x || frame.y) && !frame.width' in reader


def test_pause_buttons_are_comfortable_rows():
    source = (ROOT / 'port/linux/game/menu_tags.c').read_text()
    assert '#define PAUSE_ROW_PITCH 39' in source
    assert '#define PAUSE_ROW_BITMAP "pause/pause_row"' in source
    bitmaps = {b.get('name'): b for b in ET.parse(MENUS / 'xbox/bitmaps.xml').iter('bitmap')}
    assert all(f.get('height') == '36' and f.get('width') == '232' for f in bitmaps['pause/pause_row'].findall('frame'))
    # the pause box holds its 39-unit rows plus the stock 89 units of margin and legend
    import port_settings
    assert [port_settings.pause_box_height(n) for n in port_settings.PAUSE_BOX_BUTTONS] == [206, 245]
