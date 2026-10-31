"""Execute the production menu pointer's target search with a finger's reach."""

from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import SkipTest

ROOT = Path(__file__).resolve().parents[1]


def test_target_search_reach():
    source = (ROOT / "source/interface/ui_widget.c").read_text()
    start = source.index("/* the squared distance from a point to a rectangle")
    end = source.index("static boolean ui_mouse_widget_has_focus(", start)
    search = source[start:end]
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        raise SkipTest("A C compiler is required for the menu target probe")
    harness = r'''
#include <assert.h>
#include <stddef.h>
typedef short boolean;
typedef struct { short x0, y0, x1, y1; } rectangle2d;
struct widget_instance;
struct ui_mouse_target { struct widget_instance *widget; rectangle2d bounds; short kind; short button_index; };
#define UI_MOUSE_MAXIMUM_TARGETS 96
static struct ui_mouse_target ui_mouse_targets[UI_MOUSE_MAXIMUM_TARGETS];
static long ui_mouse_target_count = 0;
static short ui_mouse_radius = 0;
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(a) ((a) < 0 ? -(a) : (a))
enum { _ui_mouse_target_item, _ui_mouse_target_value, _ui_mouse_target_list_slot, _ui_mouse_target_button };
static long ui_redraw_touch_target_units(void) { return 0; }
__SEARCH__
static void add(short x0, short y0, short x1, short y1) {
    ui_mouse_targets[ui_mouse_target_count].bounds = (rectangle2d){ x0, y0, x1, y1 };
    ui_mouse_target_count++;
}
int main(void) {
    /* two rows 20 units tall with a 10 unit gap, and a button drawn over the first row's right end */
    add(100, 100, 300, 120);
    add(100, 130, 300, 150);
    add(250, 100, 300, 120);
    assert(ui_mouse_target_at(150, 110) == &ui_mouse_targets[0]);
    /* containment: the target drawn last wins */
    assert(ui_mouse_target_at(260, 110) == &ui_mouse_targets[2]);
    /* a mouse (no reach) finds nothing in the gap or outside */
    assert(!ui_mouse_target_at(150, 125));
    assert(!ui_mouse_target_at(90, 110));
    /* a finger's reach: the nearest target within it, none beyond */
    ui_mouse_radius = 12;
    assert(ui_mouse_target_at(150, 123) == &ui_mouse_targets[0]);   /* 4 below the first row, 7 above the second */
    assert(ui_mouse_target_at(150, 127) == &ui_mouse_targets[1]);
    assert(ui_mouse_target_at(95, 110) == &ui_mouse_targets[0]);
    assert(!ui_mouse_target_at(60, 110));
    assert(!ui_mouse_target_at(150, 80));
    /* containment still beats a nearer edge of another target */
    assert(ui_mouse_target_at(150, 119) == &ui_mouse_targets[0]);
    /* the reach does not change what contains the point */
    assert(ui_mouse_target_at(260, 110) == &ui_mouse_targets[2]);
    ui_mouse_radius = 0;
    assert(!ui_mouse_target_at(150, 125));
    return 0;
}
'''.replace("__SEARCH__", search)
    with tempfile.TemporaryDirectory() as directory:
        probe = Path(directory) / "probe.c"
        binary = Path(directory) / "probe"
        probe.write_text(harness)
        subprocess.run([compiler, "-O1", "-Wall", "-Werror", str(probe), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


def test_click_waits_for_the_first_draw():
    source = (ROOT / "source/interface/ui_widget.c").read_text()
    process = source[source.index("static void ui_widgets_process_mouse("):]
    process = process[:process.index("\n}\n")]
    # a click before the menu has drawn has no targets and would be the "nothing to pick" A
    assert "!ui_mouse_menu_drawn" in process
    # forgotten whenever the pointer is gone, so the next menu draws first
    assert process.index("ui_mouse_menu_drawn = FALSE") < process.index("ui_mouse_radius = pointer.radius")
    # a screen with no menu widget (the scores after a game) has nothing to draw first
    assert "if (!ui_mouse_menu())\n\t\t\tui_mouse_menu_drawn = TRUE;" in process
    render = source[source.index("ui_mouse_noting_targets = widget->local_player_index == NONE"):]
    assert "ui_mouse_menu_drawn = TRUE" in render[:render.index("debug_show_path")]


def test_scroll_arrows_are_targets():
    source = (ROOT / "source/interface/ui_widget.c").read_text()
    note = source[source.index("static void ui_mouse_note_target("):]
    note = note[:note.index("\n}\n")]
    # a list's scroll arrows (Xbox look: server browser, map lists) step the list when tapped
    assert 'strcmp(widget->name, "scroll_up_arrow")' in note and "_widget_event_dpad_up" in note
    assert 'strcmp(widget->name, "scroll_down_arrow")' in note and "_widget_event_dpad_down" in note


def test_pointer_synthesizes_no_input_activity():
    # touch-made mouse events must not count as mouse activity, and the menu's own presses (posted to the
    # event manager) must not reach the input class
    platform = (ROOT / "port/linux/src/sdl_platform.c").read_text()
    motion = platform[platform.index("case SDL_EVENT_MOUSE_MOTION:"):]
    motion = motion[:motion.index("#ifndef HALO_ILP32")]
    assert "SDL_TOUCH_MOUSEID" in motion and "SDL_PEN_MOUSEID" in motion
    assert "SDL_TOUCH_MOUSEID || event.button.which == SDL_PEN_MOUSEID" in platform
    host = (ROOT / "port/ios/host/host_sdl.c").read_text()
    pointer = host[host.index("static void menu_pointer_event("):host.index("HALO_MENU_POINTER_SCRIPT")]
    assert "platform_menu_activity" not in pointer
    widget = (ROOT / "source/interface/ui_widget.c").read_text()
    assert "platform_menu_activity" not in widget


def test_carousel_arrows_are_targets():
    source = (ROOT / "source/interface/ui_widget.c").read_text()
    helper = source[source.index("static void ui_mouse_note_carousel_arrows("):]
    helper = helper[:helper.index("\n}\n")]
    # the profile carousel's arrows have no widget: strips outside the first and last items press the d-pad
    assert "_widget_event_dpad_left" in helper and "ui_mouse_list_directions" in helper
    assert "target->button_index = side ? forward : back;" in helper
    note = source[source.index("static void ui_mouse_note_target("):]
    note = note[:note.index("\n}\n")]
    assert "ui_mouse_note_carousel_arrows(widget, offset)" in note
