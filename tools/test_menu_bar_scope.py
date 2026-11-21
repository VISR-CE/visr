"""Executes the bar's touch visibility from source/interface/ui_widget.c for each scope and input.

Bug class: what a bar action shows on touch decided by its widget's name or caption. A declared bar action shows by
the scope the menus' data gives it (halo_menu_meta.h): on touch a select or back action is not drawn (a tap on the item
selects, the title's chevron goes back) and a screen-wide action is; with a controller every action is drawn. The
probe's widget names say nothing (reading one aborts), so a name-based answer fails.
"""

from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import SkipTest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    """the whole definition that starts at the last occurrence of signature (the first may be a declaration)"""
    start = source.rindex(signature)
    start = source.rindex("\n", 0, start) + 1
    depth, index = 0, source.index("{", start)
    while True:
        depth += {"{": 1, "}": -1}.get(source[index], 0)
        index += 1
        if depth == 0:
            return source[start:index]


def test_bar_actions_show_by_scope():
    source = (ROOT / "source/interface/ui_widget.c").read_text()
    meta = (ROOT / "port/linux/include/halo_menu_meta.h").read_text()
    pieces = [function(source, "static short ui_bar_slot_button(unsigned char slot)"),
              function(source, "static boolean ui_touch_legend_suppressed(struct widget_instance *widget)")]
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        raise SkipTest("A C compiler is required for the bar scope probe")
    harness = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
enum { _gamepad_analog_button_a, _gamepad_analog_button_b, _gamepad_analog_button_x, _gamepad_analog_button_y };
__META__
struct widget_instance { struct widget_instance *parent; long definition_tag_index; short local_player_index; };
static short local_player_index_for_draw_string_and_hack_in_icons = 0;
static int touch;
static int controller_glyph_hidden(short owner, short button) { (void)owner; (void)button; return touch; }
/* a bar action's name says nothing: reading it is the bug */
static char const *tag_get_name(long tag_index) { (void)tag_index; abort(); }
static short ui_mouse_key_button(struct widget_instance *widget) { (void)widget; return NONE; }
static struct widget_instance *ui_legend_save_caption(struct widget_instance *widget) { (void)widget; abort(); }
static struct halo_menu_meta record;
struct halo_menu_meta const *pc_menu_meta(long tag_index) { (void)tag_index; return &record; }
__PIECES__
int main(void) {
    struct widget_instance action = { NULL, 7, NONE };
    static unsigned char const slots[] = { HALO_MENU_SLOT_X, HALO_MENU_SLOT_Y, HALO_MENU_SLOT_B, HALO_MENU_SLOT_A };
    static unsigned char const scopes[] = { HALO_MENU_SCOPE_GLOBAL, HALO_MENU_SCOPE_SELECT, HALO_MENU_SCOPE_BACK };
    record.role = HALO_MENU_ROLE_BAR_ACTION;
    for (touch = 0; touch < 2; touch++)
        for (int slot = 0; slot < 4; slot++)
            for (int scope = 0; scope < 3; scope++) {
                record.slot = slots[slot];
                record.scope = scopes[scope];
                assert(ui_bar_slot_button(record.slot) != NONE);
                assert(ui_touch_legend_suppressed(&action) ==
                       (touch && scopes[scope] != HALO_MENU_SCOPE_GLOBAL));
            }
    return 0;
}
'''
    program = harness.replace("__META__", meta).replace("__PIECES__", "\n".join(pieces))
    with tempfile.TemporaryDirectory() as directory:
        probe = Path(directory) / "probe.c"
        binary = Path(directory) / "probe"
        probe.write_text(program)
        subprocess.run([compiler, "-O1", "-Wall", "-Werror", "-Wno-unused-function", str(probe), "-o", str(binary)],
                       check=True)
        subprocess.run([str(binary)], check=True)
