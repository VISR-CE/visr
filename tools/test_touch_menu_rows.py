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
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-shared', '-fPIC',
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
                              (['1', '2', '3', 'inf'], 2, -1), (['1', '2', '3', '4%'], 2, -1)]:
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


def test_pointer_control_activity(tmp_path):
    source = (ROOT / 'port/linux/src/xinput_sdl.c').read_text()
    functions = '\n'.join(block(source, marker) for marker in (
        'void halo_controller_glyph_pointer_activity(', 'static void glyph_keyboard_activity(',
        'int halo_menu_input_kind(', 'int halo_menu_pointer_controls(', 'void halo_menu_pointer_activity('))
    program = r'''
#include <assert.h>
#include <string.h>
#define HALO_IOS 1
#define PLATFORM_MOUSE_BUTTON_COUNT 3
#define SDL_SCANCODE_COUNT 4
#include "halo_controller_glyphs.h"
#include "halo_ui_pointer.h"
static int glyph_keyboard_active, glyph_mouse_active, glyph_menus_active, dev, keyboard = 1;
static char const *family = "";
struct host_ui_pointer { int active, moved, left_clicks, right_clicks, wheel_steps; float radius; };
struct platform_input_state { int focused, mouse_released, mouse_buttons[3], keys[4]; float mouse_dx, mouse_dy, mouse_wheel; };
int platform_menu_keyboard(void) { return keyboard; }
int host_development_hooks(void) { return dev; }
char const *config_string(char const *key) { (void)key; return family; }
''' + functions + r'''
int main(void) {
 struct host_ui_pointer pointer = {1, 1, 0, 0, 0, 0};
 struct halo_ui_pointer menu_pointer = {0};
 struct platform_input_state input = {0};
 assert(halo_menu_pointer_controls() == 0);
 halo_controller_glyph_pointer_activity(1, &pointer);
 assert(halo_menu_pointer_controls() == 1 && halo_menu_input_kind() == HALO_INPUT_KEYBOARD);
 input.focused = 1; input.keys[0] = 1;
 glyph_keyboard_activity(&input); assert(halo_menu_pointer_controls() == 0);
 halo_controller_glyph_pointer_activity(1, &pointer); assert(halo_menu_pointer_controls() == 1);
 pointer.radius = 22; halo_controller_glyph_pointer_activity(1, &pointer);
 assert(halo_menu_pointer_controls() == 2);
 keyboard = 0; glyph_keyboard_activity(&input); assert(halo_menu_pointer_controls() == 2);
 pointer.moved = 0; pointer.radius = 0;
 halo_controller_glyph_pointer_activity(1, &pointer); assert(halo_menu_pointer_controls() == 2);
 pointer.left_clicks = 1; halo_controller_glyph_pointer_activity(1, &pointer);
 assert(halo_menu_pointer_controls() == 1);
 glyph_keyboard_active = 0; assert(halo_menu_pointer_controls() == 0);
 input.keys[0] = 0; input.mouse_dx = 4;
 glyph_keyboard_activity(&input); assert(halo_menu_pointer_controls() == 1);
 input.mouse_dx = 0; input.mouse_buttons[0] = 1;
 glyph_keyboard_activity(&input); assert(halo_menu_pointer_controls() == 1);
 menu_pointer.moved = 1;
 glyph_keyboard_active = 0; halo_menu_pointer_activity(&menu_pointer);
 assert(halo_menu_pointer_controls() == 1);
 menu_pointer.radius = 22; halo_menu_pointer_activity(&menu_pointer);
 assert(halo_menu_pointer_controls() == 2);
 menu_pointer.radius = 0; halo_menu_pointer_activity(&menu_pointer);
 family = "touch"; assert(halo_menu_pointer_controls() == 1);
 dev = 1; assert(halo_menu_pointer_controls() == 2);
 family = "keyboard"; assert(halo_menu_pointer_controls() == 0);
 family = "xbox"; assert(halo_menu_pointer_controls() == 0);
 return 0;
}
'''
    probe, binary = tmp_path / 'input.c', tmp_path / 'input'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


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
struct ui_mouse_target { struct widget_instance *widget; short back_arrow_end, forward_arrow_start; };
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
 struct ui_mouse_target target = {&widget, 100, 200};
 struct ui_widget_definition definition = {{0, 100, 28, 180}};
 point2d offset = {10, 20};
 ui_widget_row_draw_toggle(&widget, &definition, offset, 0, 1);
 assert(draws == 1 && last_frame == 1);
 assert(drawn.x0 == 158 && drawn.x1 == 190 && drawn.y0 == 25 && drawn.y1 == 42);
 ui_widget_row_draw_toggle(&widget, &definition, offset, 0, 0); assert(last_frame == 0);
 row.kind = HALO_ROW_TOGGLE; controls = 2;
 ui_mouse_row_step(&target, 30, 10, 11, 1); assert(step == 11);
 widget.parameters.list.selected_index = 1;
 ui_mouse_row_step(&target, 210, 10, 11, 0); assert(step == 10);
 controls = 1;
 ui_mouse_row_step(&target, 201, 10, 11, 0); assert(step == 11);
 widget.parameters.list.selected_index = 0;
 ui_mouse_row_step(&target, 95, 10, 11, 0); assert(step == 10);
 ui_mouse_row_step(&target, 30, 10, 11, 1); assert(step == 11);
 controls = 0; ui_mouse_row_step(&target, 30, 10, 11, 1); assert(step == 10);
 row.kind = HALO_ROW_STATIC; controls = 2; step = -1;
 ui_mouse_row_step(&target, 30, 10, 11, 1); assert(step == -1);
 row.kind = HALO_ROW_ENUM; step = -1;
 ui_mouse_row_step(&target, 30, 10, 11, 0); assert(opened == 1 && step == -1);
 controls = 1; ui_mouse_row_step(&target, 95, 10, 11, 0); assert(step == 10 && opened == 1);
 ui_mouse_row_step(&target, 201, 10, 11, 1); assert(step == 11 && opened == 1);
 ui_mouse_row_step(&target, 160, 10, 11, 0); assert(opened == 2);
 controls = 0; ui_mouse_row_step(&target, 160, 10, 11, 0); assert(step == 11 && opened == 2);
 return 0;
}
'''
    probe, binary = tmp_path / 'toggle.c', tmp_path / 'toggle'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-iquote', str(ROOT / 'port/linux/include'),
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
 halo_menu_dropdown_open(&d, 500, 100, 128, 20, 18);
 assert(d.open && d.x == 475 && d.y == 128 && d.rows == 6);
 assert(d.first == 13 && d.highlight == 18);
 assert(halo_menu_dropdown_option(&d, 477, 130) == 13);
 assert(halo_menu_dropdown_option(&d, 477, 182) == 14);
 assert(halo_menu_dropdown_option(&d, 475, 129) == -1);
 assert(halo_menu_dropdown_option(&d, 640, 130) == -1);
 halo_menu_dropdown_scroll(&d, 50); assert(d.first == 14);
 halo_menu_dropdown_scroll(&d, -100); assert(d.first == 0);
 assert(halo_menu_dropdown_click(&d, 477, 182) == 1 && !d.open);
 /* No room under a bottom row: the panel opens above it. */
 halo_menu_dropdown_open(&d, -10, 450, 478, 3, 1);
 assert(d.x == 0 && d.y == 291 && d.rows == 3 && d.first == 0);
 assert(halo_menu_dropdown_click(&d, 200, 400) == -1 && !d.open);
 /* A long list keeps to the rows that fit under the value and scrolls. */
 halo_menu_dropdown_open(&d, 80, 200, 228, 100, 99);
 assert(d.rows == 4 && d.first == 96 && d.y == 228);
 assert(halo_menu_dropdown_option(&d, 90, 437) == 99);
 assert(halo_menu_dropdown_option(&d, 90, 438) == -1);
 halo_menu_dropdown_open(&d, 10, 300, 328, 8, 0);
 assert(d.y == 328 && d.rows == 2);
 halo_menu_dropdown_open(&d, 10, 400, 428, 8, 0);
 assert(d.y == 33 && d.rows == 7);
 halo_menu_dropdown_open(&d, 10, 0, 28, 0, 0); assert(!d.open);
 return 0;
}
''')
    binary = tmp_path / 'dropdown'
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-std=c89', '-pedantic',
                    '-iquote', str(ROOT / 'port/linux/include'), str(source), '-o', str(binary)], check=True)
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
 widget.parameters.list.number_of_items = 30;
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 25);
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
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21);
 pointer.click_x = 5;
 assert(ui_dropdown_process_pointer(&pointer) && !ui_dropdown.open && ui_dropdown_selection == -1);
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21);
 pointer.left_clicks = 0; pointer.right_clicks = 1;
 assert(ui_dropdown_process_pointer(&pointer) && !ui_dropdown.open && presses == 21);
 /* Keyboard ownership changes before Escape is delivered: it must remain modal. */
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21);
 controls = 0; pointer.right_clicks = 0;
 assert(ui_dropdown_process_pointer(&pointer) && ui_dropdown.open);
 assert(!ui_dropdown_filter_event(&event) && ui_dropdown.open);
 event.type = _event_type_button;
 assert(ui_dropdown_filter_event(&event) && !ui_dropdown.open);
 assert(!ui_dropdown_filter_event(&event));
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21);
 event.type = _event_type_left_stick;
 assert(ui_dropdown_filter_event(&event) && !ui_dropdown.open);
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 30, 21);
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
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-iquote', str(ROOT / 'port/linux/include'),
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
 assert(bounds->x0 == 108 && bounds->x1 == 241);
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
 halo_menu_dropdown_open(&ui_dropdown, 100, 100, 128, 20, 18);
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
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
