"""Runtime row classification and pointer control ownership."""
import ctypes
import json
from pathlib import Path
import shutil
import struct

import pytest
from xml.etree import ElementTree as ET
import subprocess

from tools.test_controller_glyphs import block

ROOT = Path(__file__).resolve().parents[1]


def test_row_classifier(tmp_path):
    source = tmp_path / 'rows.c'
    source.write_text('''#include "halo_menu_row.h"
static wchar_t const *get(void *context, int index) { return ((wchar_t const **)context)[index]; }
int classify(int count, wchar_t const **strings, int *on) {
 struct halo_menu_row row = halo_menu_row_classify(count, get, (void *)strings);
 *on = row.on_index; return row.kind;
}
''')
    library = tmp_path / 'rows.dylib'
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-shared', '-fPIC',
                    '-iquote', str(ROOT / 'port/linux/include'), str(source), '-o', str(library)], check=True)
    classify = ctypes.CDLL(str(library)).classify
    classify.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_wchar_p), ctypes.POINTER(ctypes.c_int)]

    def check(strings, expected, on=-1):
        result = ctypes.c_int()
        got = classify(len(strings), (ctypes.c_wchar_p * len(strings))(*strings), ctypes.byref(result))
        assert (got, result.value) == (expected, on), strings

    for strings, kind, on in [([], 2, -1), (['fixed'], 0, -1), (['on', 'OFF'], 1, 0),
                              (['NO', 'yes'], 1, 1), (['Disabled', 'ENABLED'], 1, 1),
                              (['ON', 'YES'], 2, -1), (['TRUE', 'FALSE'], 2, -1),
                              (['1', '2', '3'], 2, -1), (['-1', '0', '0.5', '2'], 3, -1),
                              (['1', '2', '2', '3'], 2, -1), (['4', '3', '2', '1'], 2, -1),
                              (['1', '2', '3', 'inf'], 2, -1), (['1', '2', '3', '4%'], 2, -1),
                              (['0.5x', '1x', '1.25x', '2X'], 3, -1), (['25%', '50%', '75%', '100%'], 3, -1),
                              (['1x', '2x', '3', '4x'], 2, -1), (['1x', '2%', '3x', '4x'], 2, -1),
                              (['1x', '2x', 'x', '4x'], 2, -1),
                              # a word for nothing below the numbers is the range's zero; any other word is a choice
                              (['NONE', '1', '2', '3', '4'], 3, -1), (['off', '25%', '50%', '75%'], 3, -1),
                              (['NONE', '0', '1', '2'], 2, -1), (['NONE', '2', '1', '3'], 2, -1),
                              (['NONE', '1', '2'], 2, -1), (['NONE', '1', '2', 'NONE'], 2, -1),
                              (['SAME', '1', '2', '3'], 2, -1), (['AUTO', '30', '60', '120', 'NONE'], 2, -1),
                              (['1', 'NONE', '2', '3'], 2, -1), (['NONE', '1x', '2', '3x'], 2, -1)]:
        check(strings, kind, on)
    rows = json.loads((ROOT / 'tools/fixtures/touch_menu_rows.json').read_text())
    actual_rows = {(path.name, widget.get('name').split('/')[-1])
                   for path in (ROOT / 'port/assets/menus/xbox').glob('*.xml')
                   for widget in ET.parse(path).iter('widget')
                   if widget.get('type') == 'spinner' and widget.get('strings') is not None}
    assert {(row['file'], row['name']) for row in rows} == actual_rows
    for row in rows:
        strings = row['strings']
        widgets = ET.parse(ROOT / 'port/assets/menus/xbox' / row['file']).iter('widget')
        widget = next(w for w in widgets if w.get('name').split('/')[-1] == row['name'])
        assert widget.get('strings').split('|') == strings
        expected = row['kind']
        check(strings, expected, row['on_index'])


def test_standard_rows_are_chosen_by_data(tmp_path):
    """Bug class: a row's control chosen by reading a tag's name. A spinner is a standard row's value when it
    comes from the menus' own files or sits in a row made a standard tall row, whatever it is called, and its
    values alone choose switch, dropdown or slider."""
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    program = r"""
#include <assert.h>
#include <stddef.h>
#include <wchar.h>
#include "halo_menu_row.h"
#define TRUE 1
#define FALSE 0
#define NONE -1
typedef int boolean;
struct reference { long index; };
struct block { long count; };
struct ui_widget_definition { struct block child_widgets; struct reference text_label_string_list; };
struct string_list { struct block strings; };
struct widget_instance { long definition_tag_index; struct widget_instance *parent;
 struct { struct { int number_of_items; } list; } parameters; };
/* tags: 0-2 are the map's rows, 3-8 its spinners; 9 is a spinner of the menus' own files */
static wchar_t const *lists[][6] = {
 {L"5", L"10", L"15", L"25", L"50"}, {L"YES", L"NO"}, {L"SLAYER", L"ODDBALL", L"RACE"},
 {L"50%", L"100%", L"150%", L"200%"}, {L"NONE", L"1", L"2", L"3", L"4"}};
static int counts[] = {5, 2, 3, 4, 5};
static struct ui_widget_definition definitions[10];
static struct string_list string_lists[5];
static int own_from = 9, tall[10];
boolean pc_menu_tag(long tag) { return tag >= own_from; }
boolean pc_menu_row_tall(long tag) { return tall[tag]; }
struct ui_widget_definition *ui_widget_definition_get(long tag) { return &definitions[tag]; }
struct string_list *unicode_string_list_definition_get(long list) { string_lists[list].strings.count = counts[list]; return &string_lists[list]; }
static long spinner_string_list_extra_count(long list) { (void)list; return 0; }
static wchar_t const *spinner_string_list_get_string(long list, short index) { return lists[list][index]; }
""" + block(source, 'static wchar_t const *ui_widget_row_string(') + block(source, 'static boolean ui_widget_row_is_standard(') + \
        block(source, 'static struct halo_menu_row ui_widget_row_control(') + \
        block(source, 'static boolean ui_widget_row_has_dropdown(') + r"""
int main(void) {
 struct widget_instance rows[3] = {{0}, {1}, {2}}, spinner = {0};
 int kinds[] = {HALO_ROW_RANGE, HALO_ROW_TOGGLE, HALO_ROW_ENUM, HALO_ROW_RANGE, HALO_ROW_RANGE};
 int list;
 tall[0] = 1;
 for (list = 0; list < 5; list++) {
  definitions[3 + list].text_label_string_list.index = list;
  spinner.definition_tag_index = 3 + list; spinner.parameters.list.number_of_items = counts[list];
  /* the map's spinner in a standard tall row: its values choose the control */
  spinner.parent = &rows[0];
  assert(ui_widget_row_control(&spinner).kind == kinds[list]);
  assert(ui_widget_row_has_dropdown(&spinner) == (kinds[list] == HALO_ROW_ENUM));
  /* the same spinner in a row left as the map has it keeps the map's arrows */
  spinner.parent = &rows[1];
  assert(ui_widget_row_control(&spinner).kind == HALO_ROW_ENUM && !ui_widget_row_has_dropdown(&spinner));
  spinner.parent = NULL;
  assert(ui_widget_row_control(&spinner).kind == HALO_ROW_ENUM && !ui_widget_row_has_dropdown(&spinner));
 }
 /* a spinner of the menus' own files is standard wherever it sits */
 definitions[9].text_label_string_list.index = 4;
 spinner.definition_tag_index = 9; spinner.parent = &rows[2];
 assert(ui_widget_row_control(&spinner).kind == HALO_ROW_RANGE);
 /* a carousel holds its items as children and keeps its own behavior */
 definitions[9].child_widgets.count = 3;
 assert(ui_widget_row_control(&spinner).kind == HALO_ROW_ENUM && !ui_widget_row_has_dropdown(&spinner));
 return 0;
}
"""
    probe, binary = tmp_path / 'standard.c', tmp_path / 'standard'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


# (which input the rows' controls follow, and every event that may or may not change it: tools/test_menu_input_switch.py)


def test_toggle_drawing_and_step_dispatch(tmp_path):
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    program = r'''
#include <assert.h>
#include <string.h>
#include "halo_menu_row.h"
#define NONE -1
#define TRUE 1
typedef int boolean;
typedef struct { short y0, x0, y1, x1; } rectangle2d;
typedef struct { short x, y; } point2d;
struct widget_instance { struct { struct { int selected_index; } list; } parameters; };
struct ui_widget_definition { rectangle2d bounds; };
struct bitmap_data { int frame; };
struct rasterizer_dynamic_screen_geometry_parameters { int unused; };
struct ui_mouse_target { struct widget_instance *widget; };
static int controls, step, draws, last_frame, opened;
static struct halo_menu_row row;
static rectangle2d drawn;
int halo_menu_pointer_controls(void) { return controls; }
struct halo_menu_row ui_widget_row_control(struct widget_instance *widget) { (void)widget; return row; }
void ui_mouse_press(int event) { step = event; }
int ui_widget_row_has_dropdown(struct widget_instance *widget) { (void)widget; return row.kind == HALO_ROW_ENUM; }
void ui_dropdown_open(struct ui_mouse_target *target) { (void)target; opened++; }
int tag_loaded(int group, char const *name) {
 (void)group; assert(!strcmp(name, "pc\\xbox\\bitmaps\\toggle_switch")); return 1;
}
struct bitmap_data *bitmap_group_get_bitmap_from_sequence(int tag, int sequence, int frame) {
 static struct bitmap_data bitmap;
 assert(tag == 1 && sequence == 0); bitmap.frame = frame; return &bitmap;
}
float widget_instance_get_cumulative_alpha_modifier(struct widget_instance *widget) { (void)widget; return .5f; }
#define fast_ftol(x) ((long)(x))
#define csmemset memset
void draw_bitmap_in_rect(struct bitmap_data *bitmap, rectangle2d *bounds, rectangle2d *source,
 rectangle2d *clip, unsigned long color, void *parameters, int no_plasma) {
 (void)parameters; (void)clip;
 assert(no_plasma && color == 0x7fffffff);
 assert(source->x0 == 0 && source->y0 == 0 && source->x1 == 32 && source->y1 == 17);
 drawn = *bounds; last_frame = bitmap->frame; draws++;
}
''' + block(source, 'static void ui_widget_row_draw_toggle(') + '\n' + block(source, 'static void ui_mouse_row_step(') + r'''
int main(void) {
 struct widget_instance widget = {0};
 struct ui_mouse_target target = {&widget};
 struct ui_widget_definition definition = {{0, 100, 28, 180}};
 point2d offset = {10, 20};
 ui_widget_row_draw_toggle(&widget, &definition, offset, 0, 1);
 assert(draws == 1 && last_frame == 1);
 assert(drawn.x0 == 158 && drawn.x1 == 190 && drawn.y0 == 25 && drawn.y1 == 42);
 ui_widget_row_draw_toggle(&widget, &definition, offset, 0, 0); assert(last_frame == 0);
 assert(drawn.x0 == 158 && drawn.x1 == 190);
 row.kind = HALO_ROW_TOGGLE; controls = 2;
 ui_mouse_row_step(&target, 30, 10, 11, 1); assert(step == 11);
 widget.parameters.list.selected_index = 1;
 ui_mouse_row_step(&target, 210, 10, 11, 0); assert(step == 10);
 /* the mouse flips a switch wherever it lands: it has no arrows to hit */
 controls = 1;
 ui_mouse_row_step(&target, 201, 10, 11, 0); assert(step == 10);
 widget.parameters.list.selected_index = 0;
 ui_mouse_row_step(&target, 95, 10, 11, 0); assert(step == 11);
 ui_mouse_row_step(&target, 30, 10, 11, 1); assert(step == 11);
 controls = 0; ui_mouse_row_step(&target, 30, 10, 11, 1); assert(step == 10);
 row.kind = HALO_ROW_STATIC; controls = 2; step = -1;
 ui_mouse_row_step(&target, 30, 10, 11, 1); assert(step == -1);
 row.kind = HALO_ROW_ENUM; step = -1;
 ui_mouse_row_step(&target, 30, 10, 11, 0); assert(opened == 1 && step == -1);
 /* the mouse opens the dropdown wherever it lands, as a finger does */
 controls = 1; step = -1; ui_mouse_row_step(&target, 95, 10, 11, 0); assert(step == -1 && opened == 2);
 ui_mouse_row_step(&target, 201, 10, 11, 1); assert(step == -1 && opened == 3);
 controls = 0; ui_mouse_row_step(&target, 160, 10, 11, 0); assert(step == 11 && opened == 3);
 return 0;
}
'''
    probe, binary = tmp_path / 'toggle.c', tmp_path / 'toggle'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_dropdown_geometry_and_modal_state(tmp_path):
    """Catch offscreen placement, hidden current values, and outside-tap selection."""
    source = tmp_path / 'dropdown.c'
    source.write_text(r'''
#include <assert.h>
#include "halo_menu_dropdown.h"
int main(void) {
 struct halo_menu_dropdown d = {0};
 /* The entire six-row panel fits below the value. */
 halo_menu_dropdown_open(&d, 500, 100, 128, 6, 5, 0, 480, 165, 0, 640);
 assert(d.open && d.x == 475 && d.y == 128 && d.rows == 6);
 assert(d.first == 0 && d.highlight == 5);
 assert(halo_menu_dropdown_option(&d, 477, 130) == 0);
 assert(halo_menu_dropdown_option(&d, 477, 182) == 1);
 assert(halo_menu_dropdown_option(&d, 475, 129) == -1);
 assert(halo_menu_dropdown_option(&d, 640, 130) == -1);
 assert(halo_menu_dropdown_click(&d, 477, 182) == 1 && !d.open);
 /* A bottom row opens above the legend rather than covering CANCEL. */
 halo_menu_dropdown_open(&d, -10, 300, 328, 3, 1, 0, 414, 165, 0, 640);
 assert(d.x == 0 && d.y == 141 && d.rows == 3 && d.first == 0);
 assert(d.y + d.rows * HALO_DROPDOWN_PITCH + 3 <= 414);
 assert(halo_menu_dropdown_click(&d, 200, 400) == -1 && !d.open);
 /* Cropped backdrop margins cannot hide either end of the panel. */
 halo_menu_dropdown_open(&d, 80, 400, 428, 7, 6, 23, 414, 165, 0, 640);
 assert(d.y == 33 && d.rows == 7 && d.first == 0);
 assert(d.y >= 23 && d.y + d.rows * HALO_DROPDOWN_PITCH + 3 <= 414);
 /* Neither side fits a full list: shift it above the legend. */
 halo_menu_dropdown_open(&d, 80, 200, 228, 7, 6, 23, 414, 165, 0, 640);
 assert(d.y == 47 && d.rows == 7 && d.first == 0);
 /* A too-tall list fills the band with complete rows and scrolls the rest. */
 halo_menu_dropdown_open(&d, 80, 200, 228, 100, 99, 60, 350, 165, 0, 640);
 assert(d.y == 87 && d.rows == 5 && d.first == 95 && d.highlight == 99);
 assert(d.y >= 60 && d.y + d.rows * HALO_DROPDOWN_PITCH + 3 == 350);
 assert(halo_menu_dropdown_option(&d, 90, 348) == 99);
 assert(halo_menu_dropdown_option(&d, 90, 349) == -1);
 halo_menu_dropdown_scroll(&d, -2); assert(d.first == 93);
 halo_menu_dropdown_scroll(&d, 50); assert(d.first == 95);
 halo_menu_dropdown_scroll(&d, -100); assert(d.first == 0);
 /* Exactly enough room for a panel still allows it below the value. */
 halo_menu_dropdown_open(&d, 10, 100, 128, 2, 0, 23, 235, 165, 0, 640);
 assert(d.y == 128 && d.rows == 2);
 halo_menu_dropdown_open(&d, 10, 100, 128, 2, 0, 23, 234, 165, 0, 640);
 assert(d.y == 127 && d.rows == 2);
 /* An unusably short band must not create an offscreen one-row panel. */
 halo_menu_dropdown_open(&d, 10, 100, 128, 2, 0, 100, 154, 165, 0, 640);
 assert(!d.open && d.rows == 0);
 halo_menu_dropdown_open(&d, 10, 0, 28, 0, 0, 0, 414, 165, 0, 640); assert(!d.open);
 /* The panel is wider than the label, its checkmark column and an eighth of slack. */
 assert(halo_menu_dropdown_panel_width(100) == 100 + 12 + 32);
 assert(halo_menu_dropdown_panel_width(0) == 32);
 /* A long label widens the popup; it stays inside the safe span. */
 halo_menu_dropdown_open(&d, 500, 100, 128, 3, 0, 0, 480, 230, 0, 640);
 assert(d.width == 230 && d.x == 410 && d.x + d.width <= 640);
 assert(halo_menu_dropdown_option(&d, 637, 130) == 0 && halo_menu_dropdown_option(&d, 639, 130) == -1);
 halo_menu_dropdown_open(&d, 20, 100, 128, 3, 0, 0, 480, 230, 60, 640);
 assert(d.x == 60 && d.width == 230);
 halo_menu_dropdown_open(&d, 20, 100, 128, 3, 0, 0, 480, 900, 60, 600);
 assert(d.width == 540 && d.x == 60);
 return 0;
}
''')
    binary = tmp_path / 'dropdown'
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-std=c89', '-pedantic',
                    '-iquote', str(ROOT / 'port/linux/include'), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)



def test_dropdown_uses_visible_band_and_legend(tmp_path):
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    program = r'''
#include <assert.h>
#include <math.h>
#include "halo_menu_dropdown.h"
#define NONE -1
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define _ui_mouse_target_button 3
#define UI_DROPDOWN_LEGEND_CLEARANCE 6
struct widget_instance {
 int creation_time, key, definition_tag_index;
 struct { struct { int number_of_items, selected_index; } list; } parameters;
};
struct ui_widget_definition { struct { int index; } text_font, text_label_string_list; };
static struct ui_widget_definition test_definition;
struct ui_widget_definition *ui_widget_definition_get(int tag) { (void)tag; return &test_definition; }
#define TRUE 1
struct halo_menu_screen { long x0, y0, x1, y1, safe_x0, safe_y0, safe_x1, safe_y1; };
void halo_screen_menu_screen_rect(int zoomed, struct halo_menu_screen *screen) {
 struct halo_menu_screen whole = {-36, 0, 676, 480, 0, 0, 640, 480};
 (void)zoomed; *screen = whole;
}
void const *spinner_string_list_get_string(int list, short index) { (void)list; (void)index; return 0; }
int ui_widget_port_font_text_width(int font, void const *text) { (void)font; (void)text; return 0; }
struct bounds { int x0, y0, x1, y1; };
struct ui_mouse_target {
 struct widget_instance *widget;
 struct bounds bounds, value_bounds;
 int kind;
};
static struct halo_menu_dropdown ui_dropdown;
static struct widget_instance *ui_dropdown_widget;
static int ui_dropdown_creation_time, ui_dropdown_selection, ui_dropdown_last_selection;
static struct ui_mouse_target ui_mouse_targets[3];
static long ui_mouse_target_count = 3;
static float band_top = 23.25f, band_bottom = 446.75f;
void halo_screen_menu_visible_bounds(float *top, float *bottom) {
 *top = band_top; *bottom = band_bottom;
}
int ui_mouse_key_button(struct widget_instance *widget) { return widget->key; }
''' + block(source, 'static void ui_dropdown_open(') + r'''
int main(void) {
 struct widget_instance row = {42, NONE, 0, {{7, 6}}};
 struct widget_instance legend = {0, 1, 0, {{0, 0}}};
 struct widget_instance arrow = {0, NONE, 0, {{0, 0}}};
 struct ui_mouse_target target = {&row, {0}, {80, 200, 160, 228}, 1};
 ui_mouse_targets[0].widget = &arrow;
 ui_mouse_targets[0].kind = _ui_mouse_target_button;
 ui_mouse_targets[0].bounds.y0 = 73;
 ui_mouse_targets[1].widget = &legend;
 ui_mouse_targets[1].kind = _ui_mouse_target_button;
 ui_mouse_targets[1].bounds.y0 = 414;
 ui_mouse_targets[2] = target;
 ui_dropdown_open(&target);
 /* The panel's bottom border stays 2 units above the legend line (410). */
 assert(ui_dropdown.y + ui_dropdown.rows * HALO_DROPDOWN_PITCH + 3 <= 408);
 assert(ui_dropdown.y == 41 && ui_dropdown.rows == 7);
 assert(ui_dropdown_widget == &row && ui_dropdown_creation_time == 42);
 assert(ui_dropdown_selection == -1 && ui_dropdown_last_selection == -1);
 assert(halo_menu_dropdown_click(&ui_dropdown, 82, 49) == 0);
 /* Clamp the legend limit to the visible bottom, rounding inward. */
 band_top = 60.25f; band_bottom = 350.75f;
 ui_dropdown_open(&target);
 assert(ui_dropdown.rows == 5 && ui_dropdown.y == 87 && ui_dropdown.first == 2);
 /* Without a legend, use the visible band itself. */
 ui_mouse_target_count = 1;
 band_top = 0; band_bottom = 480;
 ui_dropdown_open(&target);
 assert(ui_dropdown.rows == 7 && ui_dropdown.y == 113);
 return 0;
}
'''
    probe, binary = tmp_path / 'dropdown_limits.c', tmp_path / 'dropdown_limits'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar',
                    '-iquote', str(ROOT / 'port/linux/include'), str(probe),
                    '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_dropdown_art_registration(tmp_path):
    """Catch missing packaged art and generation that loses the dropdown assets."""
    from tools import xbox_menus
    bitmaps = xbox_menus.bitmap_document(ROOT / 'port/assets/menus', set())
    checked_in = ET.parse(ROOT / 'port/assets/menus/xbox/bitmaps.xml')
    manifest = json.loads((ROOT / 'port/assets/menus/menus.json').read_text())['files']
    for name, frames in [('dropdown_chevron', 1), ('dropdown_check', 1), ('dropdown_panel', 7)]:
        generated = bitmaps.find(f"bitmap[@name='xbox/bitmaps/{name}']")
        registered = checked_in.find(f"bitmap[@name='xbox/bitmaps/{name}']")
        assert generated is not None and registered is not None
        assert len(generated) == len(registered) == frames
        for frame in registered:
            relative = frame.get('png')
            assert relative in manifest
            artifact = ROOT / 'port/assets/menus' / relative
            png = artifact.read_bytes()
            assert png[:8] == b'\x89PNG\r\n\x1a\n'
            assert struct.unpack('>II', png[16:24]) == (int(frame.get('width')) * 4, int(frame.get('height')) * 4)


def test_dropdown_art_can_be_regenerated(tmp_path):
    pytest.importorskip('PIL')
    if not shutil.which('rsvg-convert'):
        pytest.skip('art regeneration requires rsvg-convert')
    from tools import xbox_menu_art
    xbox_menu_art.dropdown_art(tmp_path)
    for svg in tmp_path.glob('*.svg'):
        assert svg.read_bytes() == (ROOT / 'port/assets/menus/xbox/art' / svg.name).read_bytes()
        png = svg.with_suffix('.png').read_bytes()
        dimensions = ET.parse(svg).getroot()
        assert struct.unpack('>II', png[16:24]) == (int(dimensions.get('width')) * 4,
                                                 int(dimensions.get('height')) * 4)


def test_dropdown_modal_pointer_and_selection(tmp_path):
    """Catch clicks/wheels leaking to the page, Escape leaks, and truncated selection."""
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    program = r'''
#include <assert.h>
#include "halo_menu_dropdown.h"
#include "halo_ui_pointer.h"
#define TRUE 1
#define FALSE 0
#define _event_type_null 0
#define _event_type_button 1
#define _event_type_left_stick 2
#define _event_type_right_stick 3
typedef int boolean;
struct widget_instance { struct { struct { int selected_index, number_of_items; } list; } parameters; };
struct event_record { short type; };
static struct halo_menu_dropdown ui_dropdown;
static struct widget_instance widget, *ui_dropdown_widget = &widget;
static int ui_dropdown_selection = -1, ui_dropdown_last_selection = -1;
static int valid = 1, controls = 2, presses, last_press, focus;
int halo_menu_pointer_controls(void) { return controls; }
int ui_dropdown_owner_valid(void) { return valid; }
void ui_mouse_give_focus(struct widget_instance *w) { assert(w == &widget); focus++; }
void ui_mouse_list_directions(struct widget_instance *w, short *back, short *forward) {
 assert(w == &widget); *back = 10; *forward = 11;
}
void ui_mouse_press(short button) { presses++; last_press = button; }
void halo_ui_pointer_feedback(int kind) { assert(kind == 2); }
''' + '\n'.join(block(source, marker) for marker in (
        'static void ui_dropdown_step_pending(', 'static boolean ui_dropdown_filter_event(',
        'static boolean ui_dropdown_process_pointer(')) + r'''
int main(void) {
 struct halo_ui_pointer pointer = {0};
 struct event_record event = {0};
 int index;
 /* A six-row band keeps this probe focused on modal event dispatch. */
 widget.parameters.list.number_of_items = 30;
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 25, 128, 480, 165, 0, 640);
 pointer.wheel_steps = -1;
 assert(ui_dropdown_process_pointer(&pointer) && ui_dropdown.first == 21 && presses == 0);
 pointer.wheel_steps = 0; pointer.left_clicks = 1; pointer.click_x = 104; pointer.click_y = 132;
 assert(ui_dropdown_process_pointer(&pointer) && !ui_dropdown.open && ui_dropdown_selection == 21);
 /* More than the mouse's sixteen-slot line must still reach the requested index. */
 for (index = 0; index < 21; index++) {
  widget.parameters.list.selected_index = index;
  assert(ui_dropdown_process_pointer(&pointer));
  assert(presses == index + 1 && last_press == 11);
 }
 widget.parameters.list.selected_index = 21;
 assert(ui_dropdown_process_pointer(&pointer) && ui_dropdown_selection == -1 && presses == 21);
 assert(!ui_dropdown_process_pointer(&pointer));
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21, 128, 480, 165, 0, 640);
 pointer.click_x = 5;
 assert(ui_dropdown_process_pointer(&pointer) && !ui_dropdown.open && ui_dropdown_selection == -1);
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21, 128, 480, 165, 0, 640);
 pointer.left_clicks = 0; pointer.right_clicks = 1;
 assert(ui_dropdown_process_pointer(&pointer) && !ui_dropdown.open && presses == 21);
 /* Keyboard ownership changes before Escape is delivered: it must remain modal. */
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21, 128, 480, 165, 0, 640);
 controls = 0; pointer.right_clicks = 0;
 assert(ui_dropdown_process_pointer(&pointer) && ui_dropdown.open);
 assert(!ui_dropdown_filter_event(&event) && ui_dropdown.open);
 event.type = _event_type_button;
 assert(ui_dropdown_filter_event(&event) && !ui_dropdown.open);
 assert(!ui_dropdown_filter_event(&event));
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21, 128, 480, 165, 0, 640);
 event.type = _event_type_left_stick;
 assert(ui_dropdown_filter_event(&event) && !ui_dropdown.open);
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21, 128, 480, 165, 0, 640);
 valid = 0;
 assert(ui_dropdown_process_pointer(&pointer) && !ui_dropdown.open && ui_dropdown_selection == -1);
 valid = 1; controls = 2; ui_dropdown_selection = 4; ui_dropdown_last_selection = -1;
 widget.parameters.list.selected_index = 21;
 assert(ui_dropdown_process_pointer(&pointer) && last_press == 10);
 /* A handler that refuses a step must not leave selection stuck forever. */
 assert(ui_dropdown_process_pointer(&pointer) && ui_dropdown_selection == -1);
 return 0;
}
'''
    probe, binary = tmp_path / 'modal.c', tmp_path / 'modal'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_dropdown_drawing_uses_registered_art_and_menu_font(tmp_path):
    """Catch panel frame, current-check, clipping, and font dispatch mistakes."""
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    program = r'''
#include <assert.h>
#include <string.h>
#include <wchar.h>
#include "halo_menu_dropdown.h"
#define NONE -1
#define TRUE 1
typedef int boolean;
typedef struct { short y0, x0, y1, x1; } rectangle2d;
typedef struct { short x, y; } point2d;
typedef struct { float alpha, red, green, blue; } real_argb_color;
struct widget_instance { int definition_tag_index; struct { struct { int selected_index; } list; } parameters; };
struct ui_widget_definition { rectangle2d bounds; struct { int index; } text_font, text_label_string_list; };
struct bitmap_data { int tag, frame; };
struct rasterizer_dynamic_screen_geometry_parameters { int unused; };
static struct halo_menu_dropdown ui_dropdown;
static struct widget_instance widget, *ui_dropdown_widget = &widget;
static struct ui_widget_definition definition;
static int valid = 1, panels, checks, chevrons, strings, highlights, first_string;
int ui_dropdown_owner_valid(void) { return valid; }
struct ui_widget_definition *ui_widget_definition_get(int tag) { assert(tag == 99); return &definition; }
float widget_instance_get_cumulative_alpha_modifier(struct widget_instance *w) { assert(w == &widget); return .5f; }
#define fast_ftol(x) ((long)(x))
#define csmemset memset
int tag_loaded(int group, char const *name) {
 (void)group;
 if (!strcmp(name, "pc\\xbox\\bitmaps\\dropdown_panel")) return 1;
 if (!strcmp(name, "pc\\xbox\\bitmaps\\dropdown_check")) return 2;
 assert(!strcmp(name, "pc\\xbox\\bitmaps\\dropdown_chevron")); return 3;
}
struct bitmap_data *bitmap_group_get_bitmap_from_sequence(int tag, int sequence, int frame) {
 static struct bitmap_data bitmap; assert(!sequence); bitmap.tag = tag; bitmap.frame = frame; return &bitmap;
}
void draw_bitmap_in_rect(struct bitmap_data *bitmap, rectangle2d *bounds, rectangle2d *source,
 rectangle2d *clip, unsigned long color, void *parameters, int no_plasma) {
 (void)parameters; assert(no_plasma && color == 0x7fffffff && clip);
 assert(!source->x0 && !source->y0);
 assert(source->x1 == bounds->x1 - bounds->x0 && source->y1 == bounds->y1 - bounds->y0);
 if (bitmap->tag == 1) {
  panels++; assert(bitmap->frame == 5 && source->x1 == 165 && source->y1 == 315);
  assert(bounds->x0 == 100 && bounds->y0 == 128);
 } else if (bitmap->tag == 2) {
  checks++; assert(!bitmap->frame && source->x1 == 12 && source->y1 == 12);
  assert(bounds->y0 == 410);
 } else { chevrons++; assert(source->x1 == 12 && source->y1 == 8); }
}
void draw_quad(rectangle2d *bounds, unsigned long color) {
 assert(bounds->y0 == 390 && bounds->y1 == 442 && color == 0x7f1f4fa3); highlights++;
}
void draw_string_set_draw_mode(int font, int style, int align, int flags, real_argb_color *color) {
 assert(font == 42 && style == NONE && align == 0 && !flags && color->alpha == .5f);
}
wchar_t *spinner_string_list_get_string(int tag, short index) {
 static wchar_t text[2]; assert(tag == 43); text[0] = index; text[1] = 0; return text;
}
void rasterizer_draw_unicode_string(rectangle2d *bounds, rectangle2d *clip, void *cursor,
 int flags, wchar_t const *text) {
 assert(bounds == clip && !cursor && !flags);
 assert(bounds->x0 == 108 && bounds->x1 == 261);
 if (!strings) first_string = text[0];
 assert(text[0] == first_string + strings); strings++;
}
''' + '\n'.join(
        block(source, marker) for marker in ('static void ui_dropdown_bitmap(',
                                             'static void ui_widget_row_draw_chevron(',
                                             'static void ui_dropdown_draw(')) + r'''
int main(void) {
 rectangle2d clip = {0, 0, 480, 640}; point2d offset = {0};
 widget.definition_tag_index = 99; widget.parameters.list.selected_index = 18;
 definition.text_font.index = 42; definition.text_label_string_list.index = 43;
 /* A six-row band exercises the matching panel art frame. */
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 20, 18, 128, 480, 165, 0, 640);
 ui_dropdown_draw();
 assert(panels == 1 && checks == 1 && strings == 6 && first_string == 13 && highlights == 1);
 ui_widget_row_draw_chevron(&widget, &definition, offset, &clip); assert(chevrons == 1);
 valid = 0; ui_dropdown_draw(); assert(panels == 1);
 assert(halo_menu_dropdown_click(&ui_dropdown, 0, 0) == -1);
 return 0;
}
'''
    probe, binary = tmp_path / 'drawing.c', tmp_path / 'drawing'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_slider_geometry(tmp_path):
    """Track endpoints, step snapping, thumb taps, and drag slop stay consistent."""
    source = tmp_path / 'slider.c'
    source.write_text(r'''
#include <assert.h>
#include <stddef.h>
#include "halo_menu_slider.h"
int main(void) {
 struct halo_menu_slider s;
 struct halo_slider_rect thumb;
 halo_menu_slider_geometry(&s, 100, 20, 265, 48);
 assert(s.left == 110 && s.right == 201 && s.y == 34);
 assert(halo_menu_slider_value(&s, -100, 11) == 0);
 assert(halo_menu_slider_value(&s, 900, 11) == 10);
 assert(halo_menu_slider_value(&s, 156, 11) == 5);
 assert(halo_menu_slider_value(&s, 200, 1) == 0);
 thumb = halo_menu_slider_thumb(&s, 5, 11);
 assert(thumb.x1 - thumb.x0 == 15 && thumb.y1 - thumb.y0 == 15);
 assert(halo_menu_slider_tap(&s, 5, 11, thumb.x0) == 0);
 assert(halo_menu_slider_tap(&s, 5, 11, thumb.x1 - 1) == 0);
 assert(halo_menu_slider_tap(&s, 5, 11, thumb.x0 - 1) == -1);
 assert(halo_menu_slider_tap(&s, 5, 11, thumb.x1) == 1);
 assert(!halo_menu_slider_dragged(100, 20, 102, 22));
 assert(halo_menu_slider_dragged(100, 20, 105, 20));
 assert(halo_menu_slider_hit(&s, 110, 34));
 assert(!halo_menu_slider_hit(&s, 250, 34));
 assert(265 - s.right >= 60);
 halo_menu_slider_geometry(&s, 185, 20, 265, 48);
 assert(s.left == 110 && s.right == 201);
 return 0;
}
''')
    binary = tmp_path / 'slider'
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-std=c89', '-pedantic',
                    '-iquote', str(ROOT / 'port/linux/include'), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_slider_held_pointer_and_layout(tmp_path):
    source = tmp_path / 'held.c'
    source.write_text(r'''
#include <assert.h>
#include <stddef.h>
#include "host_menu_pointer.h"
int main(void) {
 struct host_menu_pointer m = {0}; struct host_ui_pointer p;
 assert(sizeof(p) == 72 && offsetof(struct host_ui_pointer, held) == 60 && offsetof(struct host_ui_pointer, drawable_height) == 68);
 host_menu_pointer_set_menus(&m, 1);
 host_menu_pointer_touch_begin(&m, 1, 100, 100);
 host_menu_pointer_read(&m, &p); assert(p.held && p.moved);
 host_menu_pointer_touch_move(&m, 1, 102, 101);
 host_menu_pointer_read(&m, &p); assert(p.held && p.x == 102 && p.y == 101 && !p.moved);
 host_menu_pointer_read(&m, &p); assert(p.held && !p.moved);
 host_menu_pointer_touch_end(&m, 1);
 host_menu_pointer_read(&m, &p); assert(!p.held && p.left_clicks == 1);
 host_menu_pointer_gaze_begin(&m, 2, 20, 20);
 host_menu_pointer_touch_move(&m, 2, 40, 21);
 host_menu_pointer_read(&m, &p); assert(p.held && p.picture && p.x == 40);
 host_menu_pointer_touch_cancel(&m, 2);
 host_menu_pointer_read(&m, &p); assert(!p.held && !p.left_clicks);
 host_menu_pointer_device_button(&m, 1, 40, 50);
 host_menu_pointer_read(&m, &p); assert(p.held && !p.left_clicks);
 host_menu_pointer_device_move(&m, 80, 50);
 host_menu_pointer_read(&m, &p); assert(p.held && p.x == 80);
 host_menu_pointer_device_button(&m, 0, 80, 50);
 host_menu_pointer_read(&m, &p); assert(!p.held && p.left_clicks == 1);
 host_menu_pointer_touch_begin(&m, 3, 0, 0);
 host_menu_pointer_set_blocked(&m, 1);
 host_menu_pointer_read(&m, &p); assert(!p.held);
 return 0;
}
''')
    binary = tmp_path / 'held'
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-iquote', str(ROOT / 'port/ios/host'),
                    '-iquote', str(ROOT / 'port/runtime/include'), str(source), '-o', str(binary), '-lm'], check=True)
    subprocess.run([str(binary)], check=True)


def test_slider_art_registration_and_regeneration(tmp_path):
    pytest.importorskip('PIL')
    import struct
    from tools import xbox_menus, xbox_menu_art
    generated = xbox_menus.bitmap_document(ROOT / 'port/assets/menus', set())
    registered = ET.parse(ROOT / 'port/assets/menus/xbox/bitmaps.xml')
    manifest = json.loads((ROOT / 'port/assets/menus/menus.json').read_text())['files']
    for name in ('slider_track', 'slider_fill', 'slider_thumb'):
        a = generated.find(f"bitmap[@name='xbox/bitmaps/{name}']")
        b = registered.find(f"bitmap[@name='xbox/bitmaps/{name}']")
        assert a is not None and b is not None
        frame = b.find('frame')
        relative = frame.get('png')
        assert relative in manifest
        header = (ROOT / 'port/assets/menus' / relative).read_bytes()[:26]
        assert header[:8] == b'\x89PNG\r\n\x1a\n' and header[12:16] == b'IHDR'
        width, height, depth, color_type = struct.unpack('>IIBB', header[16:26])
        assert (depth, color_type) == (8, 6)  # 8-bit RGBA
        assert (width, height) == (int(frame.get('width')) * 4, int(frame.get('height')) * 4)
    if shutil.which('rsvg-convert'):
        xbox_menu_art.slider_art(tmp_path)
        for artifact in tmp_path.iterdir():
            assert artifact.read_bytes() == (ROOT / 'port/assets/menus/xbox/art' / artifact.name).read_bytes()


def test_slider_capture_and_normal_spinner_events(tmp_path):
    """A held slider consumes scroll, taps step once, and drags use normal events."""
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    program = r'''
#include <assert.h>
#include <stddef.h>
#include "halo_menu_slider.h"
#include "halo_ui_pointer.h"
#define TRUE 1
#define FALSE 0
#define HALO_ROW_RANGE 3
#define _ui_mouse_target_value 1
typedef int boolean;
typedef struct { short y0, x0, y1, x1; } rectangle2d;
struct widget_instance { long creation_time; struct { struct { int selected_index, number_of_items; } list; } parameters; };
struct ui_mouse_target { struct widget_instance *widget; rectangle2d value_bounds; int kind; };
struct row { int kind; };
static struct widget_instance widget;
static struct ui_mouse_target ui_mouse_targets[1];
static struct widget_instance *ui_slider_widget;
static long ui_slider_creation_time;
static struct halo_menu_slider ui_slider_geometry;
static int ui_slider_down_x, ui_slider_down_y, ui_slider_dragging, ui_slider_was_held;
static int ui_slider_selection = -1, ui_slider_last_selection = -1;
static int ui_mouse_target_count = 1, ui_mouse_press_count, controls = 2, presses, last_press;
int halo_menu_pointer_controls(void) { return controls; }
struct row ui_widget_row_control(struct widget_instance *w) { struct row r = {3}; assert(w == &widget); return r; }
struct ui_mouse_target *ui_mouse_target_at(short x, short y) {
 return x >= 100 && x <= 265 && y >= 20 && y <= 48 ? ui_mouse_targets : 0;
}
void ui_mouse_give_focus(struct widget_instance *w) { assert(w == &widget); }
void ui_mouse_list_directions(struct widget_instance *w, short *back, short *forward) {
 assert(w == &widget); *back = 10; *forward = 11;
}
void ui_mouse_press(short event) { presses++; last_press = event; }
void halo_ui_pointer_feedback(int kind) { assert(kind == 2); }
''' + block(source, 'static boolean ui_slider_process_pointer(') + r'''
int main(void) {
 struct halo_ui_pointer p = {0};
 struct halo_slider_rect thumb;
 int i;
 widget.creation_time = 20; widget.parameters.list.number_of_items = 11;
 widget.parameters.list.selected_index = 5;
 ui_mouse_targets[0].widget = &widget;
 ui_mouse_targets[0].kind = 1;
 ui_mouse_targets[0].value_bounds.x0 = 100; ui_mouse_targets[0].value_bounds.x1 = 265;
 ui_mouse_targets[0].value_bounds.y0 = 20; ui_mouse_targets[0].value_bounds.y1 = 48;
 /* Stationary thumb press and release does not step. */
 p.held = 1; p.x = 161; p.y = 34;
 assert(ui_slider_process_pointer(&p) && presses == 0);
 p.held = 0; p.left_clicks = 1; p.click_x = p.x; p.click_y = p.y;
 assert(ui_slider_process_pointer(&p) && presses == 0 && !ui_slider_widget);
 /* Track tap steps once, rather than jumping to the pointer's fraction. */
 p.x = 500; p.click_x = 200;
 assert(ui_slider_process_pointer(&p) && presses == 1 && last_press == 11);
 widget.parameters.list.selected_index++;
 p.left_clicks = 0;
 assert(ui_slider_process_pointer(&p) && !ui_slider_widget && presses == 1);
 /* Capture survives movement off the row and consumes wheel input. */
 p.held = 1; p.x = 170;
 assert(ui_slider_process_pointer(&p));
 p.x = 500; p.y = 300; p.wheel_steps = 8;
 for (i = 6; i < 10; i++) {
  widget.parameters.list.selected_index = i;
  assert(ui_slider_process_pointer(&p) && last_press == 11);
 }
 assert(presses == 5);
 widget.parameters.list.selected_index = 10;
 assert(ui_slider_process_pointer(&p) && presses == 5);
 p.held = 0; p.wheel_steps = 0;
 assert(ui_slider_process_pointer(&p) && !ui_slider_widget);
 /* Left track tap at the minimum clamps instead of wrapping. */
 widget.parameters.list.selected_index = 0;
 p.x = p.click_x = 103; p.y = p.click_y = 34; p.left_clicks = 1;
 assert(ui_slider_process_pointer(&p) && presses == 5 && !ui_slider_widget);
 /* Cancellation without a click does not step a stationary press. */
 p.left_clicks = 0; p.held = 1; p.x = 140;
 assert(ui_slider_process_pointer(&p));
 p.held = 0;
 assert(ui_slider_process_pointer(&p) && presses == 5 && !ui_slider_widget);
 /* Stale instances and switching to keyboard/controller drop capture. */
 p.held = 1; assert(ui_slider_process_pointer(&p));
 widget.creation_time++;
 assert(ui_slider_process_pointer(&p) && !ui_slider_widget);
 p.held = 0; assert(!ui_slider_process_pointer(&p));
 p.held = 1; assert(ui_slider_process_pointer(&p));
 controls = 0; assert(!ui_slider_process_pointer(&p) && !ui_slider_widget);
 /* Exercise the shared thumb math here as in the drawing path. */
 thumb = halo_menu_slider_thumb(&ui_slider_geometry, 0, 11);
 assert(thumb.x0 == 103);
 return 0;
}
'''
    probe, binary = tmp_path / 'capture.c', tmp_path / 'capture'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_slider_pointer_abi_for_guest_and_host(tmp_path):
    compiler = shutil.which('clang')
    if not compiler:
        pytest.skip('cross-target layout checks require clang')
    source = tmp_path / 'layout.c'
    source.write_text(r'''
#include <stddef.h>
#include "halo_host_pointer.h"
#include "halo_ui_pointer.h"
_Static_assert(sizeof(struct host_ui_pointer) == 72, "host size");
_Static_assert(offsetof(struct host_ui_pointer, x) == 20, "position offset");
_Static_assert(offsetof(struct host_ui_pointer, picture) == 48, "picture offset");
_Static_assert(offsetof(struct host_ui_pointer, held) == 60, "held offset");
_Static_assert(sizeof(struct halo_ui_pointer) == 18, "guest size");
_Static_assert(offsetof(struct halo_ui_pointer, held) == 16, "guest held offset");
''')
    for target in ('arm64-apple-macos', 'arm64_32-apple-watchos'):
        subprocess.run([compiler, '--target=' + target, '-isysroot', str(tmp_path), '-ffreestanding', '-fsyntax-only', '-Wall', '-Werror', '-Wno-multichar',
                        '-iquote', str(ROOT / 'port/runtime/include'),
                        '-iquote', str(ROOT / 'port/linux/include'), str(source)], check=True)


def test_slider_drawing_uses_menu_art_and_live_number(tmp_path):
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    program = r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include "halo_menu_slider.h"
#define NONE -1
typedef struct { short y0, x0, y1, x1; } rectangle2d;
typedef struct { short x, y; } point2d;
typedef struct { float alpha, red, green, blue; } real_argb_color;
struct widget_instance { struct { struct { int selected_index, number_of_items; } list; } parameters; };
struct ui_widget_definition { rectangle2d bounds; real_argb_color text_color; struct { int index; } text_font, text_label_string_list; };
#define SPINNER_EXTRA_WIDTH 20
static int draws, strings;
int spinner_string_list_extra_count(int index) { assert(index == 43); return 0; }
float widget_instance_get_cumulative_alpha_modifier(struct widget_instance *widget) { (void)widget; return .5f; }
void ui_dropdown_bitmap(struct widget_instance *widget, char const *name, short frame,
 rectangle2d *bounds, rectangle2d *clip) {
 (void)widget; assert(!frame && clip);
 if (!draws) {
  assert(!strcmp(name, "pc\\xbox\\bitmaps\\slider_track"));
  assert(bounds->x0 == 120 && bounds->x1 == 211 && bounds->y0 == 52 && bounds->y1 == 57);
 } else if (draws == 1) {
  assert(!strcmp(name, "pc\\xbox\\bitmaps\\slider_fill"));
  assert(bounds->x0 == 120 && bounds->x1 == 165 && bounds->y1 - bounds->y0 == 5);
 } else {
  assert(!strcmp(name, "pc\\xbox\\bitmaps\\slider_thumb"));
  assert(bounds->x0 == 153 && bounds->x1 == 178 && bounds->y0 == 42 && bounds->y1 == 67);
 }
 draws++;
}
void draw_string_set_draw_mode(int font, int style, int align, int flags, real_argb_color *color) {
 assert(font == 42 && style == NONE && align == 1 && !flags && color->alpha == .4f);
}
wchar_t *spinner_string_list_get_string(int tag, short index) {
 assert(tag == 43 && index == 5); return L"5";
}
void rasterizer_draw_unicode_string(rectangle2d *bounds, rectangle2d *clip, void *cursor,
 int flags, wchar_t const *text) {
 assert(clip && !cursor && !flags && !wcscmp(text, L"5"));
 assert(bounds->x0 == 217 && bounds->x1 == 275 && bounds->y0 == 40 && bounds->y1 == 68); strings++;
}
''' + block(source, 'static void ui_widget_row_draw_slider(') + r'''
int main(void) {
 struct widget_instance w = {0}; struct ui_widget_definition d = {0};
 rectangle2d clip = {0, 0, 480, 640}; point2d offset = {10, 20};
 w.parameters.list.selected_index = 5; w.parameters.list.number_of_items = 11;
 d.bounds.x0 = 100; d.bounds.x1 = 265; d.bounds.y0 = 20; d.bounds.y1 = 48;
 d.text_color.alpha = .8f; d.text_font.index = 42; d.text_label_string_list.index = 43;
 ui_widget_row_draw_slider(&w, &d, offset, &clip);
 assert(draws == 3 && strings == 1);
 /* Hit geometry is consumed by dispatch, not this isolated drawing probe. */
 assert(halo_menu_slider_dragged(0, 0, 5, 0));
 { struct halo_menu_slider s; halo_menu_slider_geometry(&s, 100, 20, 265, 48);
 assert(halo_menu_slider_value(&s, 110, 11) == 0);
 assert(halo_menu_slider_hit(&s, 110, 34)); assert(halo_menu_slider_tap(&s, 0, 11, 200) == 1); }
 return 0;
}
'''
    probe, binary = tmp_path / 'slider-draw.c', tmp_path / 'slider-draw'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_option_row_arrows_follow_the_input(tmp_path):
    """Bug class: arrows drawn for an input that does not use them. Runs the function the renderer draws every
    option row from (halo_menu_row_look) over each row kind (static, toggle, enum, range), with and without a
    dropdown, for each input: a controller and a keyboard (0) get the arrows and no direct control; a mouse (1)
    and a finger (2) get no arrows and exactly the control that acts on the row."""
    source = tmp_path / 'look.c'
    source.write_text('#include "halo_menu_row.h"\n'
                      'int look(int kind, int dropdown, int input, int *out) {\n'
                      ' struct halo_menu_row_look l = halo_menu_row_look((enum halo_menu_row_kind)kind, dropdown, input);\n'
                      ' out[0] = l.arrows; out[1] = l.toggle; out[2] = l.dropdown; out[3] = l.slider; return 0; }\n')
    library = tmp_path / 'look.dylib'
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-shared', '-fPIC',
                    '-iquote', str(ROOT / 'port/linux/include'), str(source), '-o', str(library)], check=True)
    look = ctypes.CDLL(str(library)).look
    look.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
    static, toggle, enum, range_ = 0, 1, 2, 3
    for kind in (static, toggle, enum, range_):
        for dropdown in (0, 1):
            for pointer in (0, 1, 2):
                out = (ctypes.c_int * 4)()
                look(kind, dropdown, pointer, out)
                arrows, has_toggle, has_dropdown, has_slider = (bool(value) for value in out)
                where = (kind, dropdown, pointer)
                assert arrows == (pointer == 0), where
                if pointer == 0:
                    assert not (has_toggle or has_dropdown or has_slider), where
                else:
                    assert has_toggle == (kind == toggle), where
                    assert has_slider == (kind == range_), where
                    assert has_dropdown == bool(dropdown), where
    # the renderer draws both arrow bitmaps and the controls from that one function
    draw = (ROOT / 'source/interface/ui_widget.c').read_text()
    assert draw.count('bitmap && look.arrows') == 2
    assert 'halo_menu_row_arrows_shown(' not in draw


def test_state_word_sits_in_the_value_column_for_every_input(tmp_path):
    """Bug class: a value drawn at a different place on one input (the MAPS state word drew right of the values on
    touch after it was fixed on a controller and keyboard). The slot a pointer input's control takes at a value's right
    end is one function (halo_menu_value_trailing_slot); a state word, which has no control, leaves the switch's slot
    on a mouse and a finger so it centers where the switch rows' values do, and nothing on a controller or keyboard,
    which draw every value centered in the same bounds."""
    source = tmp_path / 'slot.c'
    source.write_text('#include "halo_menu_row.h"\n'
                      'int value_slot(int kind, int dropdown, int input) {\n'
                      ' return halo_menu_value_trailing_slot(halo_menu_row_look((enum halo_menu_row_kind)kind, dropdown, input)); }\n'
                      'int word_slot(int input) { return halo_menu_state_word_trailing_slot(input); }\n')
    library = tmp_path / 'slot.dylib'
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-multichar', '-shared', '-fPIC',
                    '-iquote', str(ROOT / 'port/linux/include'), str(source), '-o', str(library)], check=True)
    lib = ctypes.CDLL(str(library))
    toggle, enum = 1, 2
    for pointer in (1, 2):
        assert lib.word_slot(pointer) == lib.value_slot(toggle, 0, pointer) == 38, pointer
        assert lib.value_slot(enum, 1, pointer) == 24, pointer
    assert lib.word_slot(0) == 0 and lib.value_slot(toggle, 0, 0) == 0 and lib.value_slot(enum, 1, 0) == 0
    # the renderer takes both from those functions, and nothing else trims a value's bounds
    draw = (ROOT / 'source/interface/ui_widget.c').read_text()
    assert 'halo_menu_value_trailing_slot(look)' in draw
    assert 'halo_menu_state_word_trailing_slot(halo_menu_pointer_controls())' in draw
    assert 'bounds.x1 -= 38' not in draw and 'bounds.x1 -= 24' not in draw


def test_every_option_row_steps_with_left_and_right():
    """Bug class: spinner rows not stepped by keyboard arrows. Every spinner of the menu set carries the flag that
    makes left and right (dpad, arrow keys, A and D) step its list, and sits in a row that passes events on."""
    for path in sorted((ROOT / 'port/assets/menus/xbox').glob('*.xml')):
        tree = ET.parse(path)
        parents = {child: parent for parent in tree.iter('widget') for child in parent.iter('widget')}
        for widget in tree.iter('widget'):
            # (a list of choices is spelled out in `strings` or read from a `string_list` tag)
            if widget.get('type') == 'spinner' and (widget.get('strings') is not None or widget.get('string_list') is not None):
                flags = (widget.get('flags') or '').split()
                assert 'left_right_tabs_items' in flags, (path.name, widget.get('name'))
                parent = parents.get(widget)
                if parent is not None and parent.get('type') in (None, 'column_list') and parent.get('flags') is not None:
                    assert 'pass_unhandled_to_focused_child' in parent.get('flags').split(), (path.name, widget.get('name'))
