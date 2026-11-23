"""The scores after a game can be left by touch.

The postgame report is not a widget screen: `game_engine_post_rasterize_post_game` draws "quit" and "pick game" as
a string with button glyphs, and the game read only the gamepads, so on touch the captions drew without glyphs
and a tap did nothing. `ui_widget_port_postgame_legend` draws them as the standard legend buttons and notes them
as pointer targets, and the game takes a tap through `ui_widget_port_postgame_pressed`. This compiles the two
functions with the legend's own slot code and checks the buttons exist, sit in A's and B's slots without
overlapping, and deliver exactly one press each.
"""
from pathlib import Path
import shutil
import subprocess

from test_controller_glyphs import block

ROOT = Path(__file__).resolve().parents[1]


def test_postgame_buttons_are_targets_in_the_standard_slots(tmp_path):
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    slots = source[source.index("/* port: the touch legend's four fixed slots."):
                   source.index('void halo_screen_menu_visible_bottom(')]
    slots += block(source, 'static void ui_touch_legend_slot_bounds(')
    legend = block(source, 'boolean ui_widget_port_postgame_legend(')
    pressed = block(source, 'boolean ui_widget_port_postgame_pressed(')
    program = r'''
#include <assert.h>
#include <string.h>
#include <wchar.h>
typedef unsigned char boolean; typedef float real;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define csmemset memset
#include "menu_geometry.h"
typedef struct { short y0, x0, y1, x1; } rectangle2d;
typedef struct { real alpha, red, green, blue; } real_argb_color;
enum { _gamepad_analog_button_a, _gamepad_analog_button_b, _gamepad_analog_button_x, _gamepad_analog_button_y };
enum { _ui_mouse_target_item, _ui_mouse_target_button, _ui_mouse_target_action };
struct widget_instance;
struct ui_mouse_target { struct widget_instance *widget; rectangle2d bounds; short kind, button_index; };
#define UI_MOUSE_MAXIMUM_TARGETS 96
static struct ui_mouse_target ui_mouse_targets[UI_MOUSE_MAXIMUM_TARGETS];
static long ui_mouse_target_count;
static int touch = 2, pills, texts;
static struct widget_instance *menu_up;
static struct widget_instance *ui_mouse_menu(void) { return menu_up; }
static long font;
int halo_menu_pointer_controls(void) { return touch; }
static long draw_string_get_font(void) { return font; }
static void draw_string_set_font(long index) { font = index; }
static void draw_string_set_format(long a, long b, long c) { (void)a; (void)b; (void)c; }
static void draw_string_set_indents(long a, long b) { (void)a; (void)b; }
static void draw_string_set_draw_mode(long a, long b, long c, long d, real_argb_color *e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
/* 12 units a letter */
static short ui_widget_port_text_width(wchar_t const *text) { return (short)(12 * wcslen(text)); }
static void ui_widget_draw_touch_legend(rectangle2d const *bounds, rectangle2d const *clip, real alpha, boolean pressed)
{ (void)clip; (void)alpha; (void)pressed; assert(bounds->x1 > bounds->x0); pills++; }
static void rasterizer_draw_unicode_string(rectangle2d const *bounds, void *a, void *b, long c, wchar_t const *text)
{ (void)a; (void)b; (void)c; assert(bounds && text[0]); texts++; }
''' + slots + r'''
static short ui_postgame_pending = NONE, ui_postgame_pressed = NONE, ui_postgame_pressed_frames;
''' + legend + pressed + r'''
int main(void)
{
    /* a controller, or keyboard and mouse: the game draws its own prompt */
    touch = 0;
    assert(!ui_widget_port_postgame_legend(1, 1.0f, TRUE) && !ui_mouse_target_count && !pills);
    touch = 2;
    /* a hosted game: PICK GAME and QUIT */
    assert(ui_widget_port_postgame_legend(1, 1.0f, TRUE));
    assert(ui_mouse_target_count == 2 && pills == 2 && texts == 2);
    assert(ui_mouse_targets[0].button_index == _gamepad_analog_button_a);
    assert(ui_mouse_targets[1].button_index == _gamepad_analog_button_b);
    for (int i = 0; i < 2; i++) {
        rectangle2d b = ui_mouse_targets[i].bounds;
        /* an action of its own kind, never a widget's button: nothing that walks the buttons reads its widget */
        assert(ui_mouse_targets[i].kind == _ui_mouse_target_action && !ui_mouse_targets[i].widget);
        assert(b.x0 >= 0 && b.x1 <= 640 && b.y0 >= 0 && b.y1 <= 480 && b.y1 - b.y0 >= 60 && b.x1 - b.x0 >= 60);
    }
    /* B's slot is left of A's, and they do not overlap */
    assert(ui_mouse_targets[1].bounds.x1 <= ui_mouse_targets[0].bounds.x0);
    /* a joined game: QUIT alone, still in B's slot */
    rectangle2d quit = ui_mouse_targets[1].bounds;
    ui_mouse_target_count = 0;
    assert(ui_widget_port_postgame_legend(1, 1.0f, FALSE) && ui_mouse_target_count == 1);
    assert(ui_mouse_targets[0].button_index == _gamepad_analog_button_b && ui_mouse_targets[0].bounds.x0 == quit.x0);
    /* a tap is taken once, by the button it was for */
    assert(!ui_widget_port_postgame_pressed(_gamepad_analog_button_a));
    ui_postgame_pending = _gamepad_analog_button_b;
    assert(!ui_widget_port_postgame_pressed(_gamepad_analog_button_a));
    assert(ui_widget_port_postgame_pressed(_gamepad_analog_button_b));
    assert(!ui_widget_port_postgame_pressed(_gamepad_analog_button_b));
    /* a menu over the scores (a dialog, the pause menu): no buttons, no targets, and a waiting press is dropped */
    ui_mouse_target_count = pills = 0;
    ui_postgame_pending = _gamepad_analog_button_b;
    menu_up = (struct widget_instance *)&touch;
    assert(!ui_widget_port_postgame_legend(1, 1.0f, TRUE) && !ui_mouse_target_count && !pills);
    assert(!ui_widget_port_postgame_pressed(_gamepad_analog_button_b));
    menu_up = 0;
    /* a press the game never takes is dropped when its button stops showing pressed, not kept for the next scores */
    ui_postgame_pending = ui_postgame_pressed = _gamepad_analog_button_a;
    ui_postgame_pressed_frames = 3;
    for (int frame = 0; frame < 2; frame++) {
        ui_mouse_target_count = 0;
        assert(ui_widget_port_postgame_legend(1, 1.0f, TRUE) && ui_postgame_pending == _gamepad_analog_button_a);
    }
    ui_mouse_target_count = 0;
    assert(ui_widget_port_postgame_legend(1, 1.0f, TRUE) && ui_postgame_pending == NONE);
    assert(!ui_widget_port_postgame_pressed(_gamepad_analog_button_a));
    return 0;
}
'''
    probe, binary = tmp_path / 'postgame.c', tmp_path / 'postgame'
    probe.write_text(program)
    subprocess.run([shutil.which('clang') or shutil.which('cc'), '-std=gnu11', '-Wall', '-Werror',
                    '-Wno-unused-function', '-Wno-unused-variable', '-fsanitize=address,undefined',
                    '-iquote', str(ROOT / 'port/linux/include'), str(probe), '--output=' + str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)


def test_the_game_takes_the_taps_and_skips_its_own_prompt_on_touch():
    """(wiring that only a build and a device can show working: the call sites exist and are ordered)"""
    engine = (ROOT / 'source/game/game_engine.c').read_text()
    raster = block(engine, 'void game_engine_post_rasterize_post_game(\n\tvoid)\n{')
    assert raster.index('ui_widget_port_postgame_legend(') < raster.index('draw_string_and_hack_in_icons(')
    assert engine.count('ui_widget_port_postgame_pressed(0)') == 1
    assert engine.count('ui_widget_port_postgame_pressed(1)') == 1
