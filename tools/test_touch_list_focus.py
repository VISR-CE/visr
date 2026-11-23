"""Executes the menu lists' focus drawing from source/interface/ui_widget.c for each input.

Bug class: a list row's focus drawn differently by input. A list's rows keep their focus frame on
touch, as on every other input, so a touch player sees which row the panel describes. Every row of a column or spinner
list draws its focus (selected frame, white text) on every input, at any depth (a row's text inside a row), and the
list keeps its focused child. A carousel card is the exception: no focus outline while the last input is touch, and
the list updaters ask the list for its kind (`ui_list_focus_item`), so they draw a carousel's cards as the carousel does.
"""

from pathlib import Path
import re
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


def test_list_rows_draw_focus_on_every_input():
    source = (ROOT / "source/interface/ui_widget.c").read_text()
    pieces = [function(source, "int ui_widget_port_draws_focus(\n"),
              function(source, "static enum halo_menu_focus_item ui_list_focus_item(\n"),
              function(source, "static boolean ui_list_child_draws_focus(\n"),
              function(source, "static boolean widget_instance_text_box_is_focused(\n"),
              function(source, "static __inline void spinner_list_update(\n"),
              function(source, "static void column_list_update(\n")]
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        raise SkipTest("A C compiler is required for the list focus probe")
    harness = r'''
#include <assert.h>
#include <stddef.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
enum { _ui_widget_type_container, _ui_widget_type_text_box, _ui_widget_type_spinner_list, _ui_widget_type_column_list };
struct widget_instance {
    struct widget_instance *parent, *child, *next, *focused_child;
    short type;
    struct { short current_frame_index, number_of_sprite_frames; } animation;
};
struct ui_widget_definition { int unused; };
static int pointer_controls;
enum halo_menu_focus_item { HALO_MENU_FOCUS_ROW, HALO_MENU_FOCUS_CARD };
static int halo_menu_pointer_controls(void) { return pointer_controls; }
static boolean carousel;
static boolean ui_widget_is_carousel(struct widget_instance *widget) { (void)widget; return carousel; }
__PIECES__
int main(void) {
    struct widget_instance list = {0}, row[2] = {{0}}, label = {0};
    struct ui_widget_definition definition = {0};
    list.type = _ui_widget_type_column_list;
    list.child = &row[0];
    row[0].next = &row[1];
    row[0].parent = row[1].parent = &list;
    row[0].type = row[1].type = _ui_widget_type_text_box;
    row[0].animation.number_of_sprite_frames = row[1].animation.number_of_sprite_frames = 2;
    row[1].child = row[1].focused_child = &label;
    label.parent = &row[1];
    label.type = _ui_widget_type_text_box;
    list.focused_child = &row[1];
    for (pointer_controls = 0; pointer_controls <= 2; pointer_controls++) {
        int drawn = 1;
        assert(ui_widget_port_draws_focus(HALO_MENU_FOCUS_CARD, 1) == (pointer_controls != 2));
        assert(!ui_widget_port_draws_focus(HALO_MENU_FOCUS_ROW, 0) && !ui_widget_port_draws_focus(HALO_MENU_FOCUS_CARD, 0));
        for (int kind = 0; kind < 2; kind++) {
            list.type = kind ? _ui_widget_type_spinner_list : _ui_widget_type_column_list;
            if (kind) spinner_list_update(&list); else column_list_update(&list, &definition);
            assert(row[1].animation.current_frame_index == drawn);
            assert(row[0].animation.current_frame_index == 0);
            assert(widget_instance_text_box_is_focused(&row[1]) == drawn);
            assert(!widget_instance_text_box_is_focused(&row[0]));
            assert(widget_instance_text_box_is_focused(&label) == drawn);
            assert(list.focused_child == &row[1]);
        }
        /* a carousel's cards: the list updater and the carousel ask the same kind, the card's */
        carousel = TRUE;
        list.type = _ui_widget_type_spinner_list;
        spinner_list_update(&list);
        assert(row[1].animation.current_frame_index == (pointer_controls != 2));
        assert(row[0].animation.current_frame_index == 0);
        carousel = FALSE;
    }
    return 0;
}
'''
    program = harness.replace("__PIECES__", "\n".join(re.sub(r"static __inline ", "static ", piece) for piece in pieces))
    with tempfile.TemporaryDirectory() as directory:
        probe = Path(directory) / "probe.c"
        binary = Path(directory) / "probe"
        probe.write_text(program)
        subprocess.run([compiler, "-O1", "-Wall", "-Werror", "-Wno-unused-function", str(probe), "-o", str(binary)],
                       check=True)
        subprocess.run([str(binary)], check=True)
