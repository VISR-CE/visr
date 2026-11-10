"""Runtime row classification and pointer control ownership."""
import ctypes
import json
from pathlib import Path
import shutil
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
static int controls, step, draws, last_frame;
static struct halo_menu_row row;
static rectangle2d drawn;
int halo_menu_pointer_controls(void) { return controls; }
struct halo_menu_row ui_widget_row_control(struct widget_instance *widget) { (void)widget; return row; }
void ui_mouse_press(int event) { step = event; }
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
 row.kind = HALO_ROW_ENUM; ui_mouse_row_step(&target, 30, 10, 11, 0); assert(step == 11);
 return 0;
}
'''
    probe, binary = tmp_path / 'toggle.c', tmp_path / 'toggle'
    probe.write_text(program)
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
