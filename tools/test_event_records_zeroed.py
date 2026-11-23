"""Every event the game builds starts from a zeroed record.

Bug class: an event field one kind of event leaves unset carries a value from an earlier event or from the stack.
`struct event_record` keeps its data in a union, so a button's `pointer` byte (set only for a tap or a click on the
button's picture) shares storage with a stick's y. A held button polled after a deflected stick then looked like a
tap, and on the PASSWORD screen X (a keyboard's E) flipped HIDE and SHOW while a letter was being typed.

`test_polled_button_events_are_keys` compiles `event_manager_update` and `ui_bar_key_is_letter` from the source and
runs them with a deflected stick and a held X: the X event must be a key (its `pointer` byte 0), so a letter being
typed swallows it. `test_every_event_record_on_the_stack_starts_zeroed` scans every function that fills an event
record it declares and fails on one that does not zero it first.
"""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import SkipTest

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [ROOT / "source", ROOT / "port"]


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


HARNESS = r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
typedef unsigned char byte;
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
enum { MAXIMUM_GAMEPADS = 4 };
enum { _gamepad_analog_button_a, _gamepad_analog_button_b, _gamepad_analog_button_x, NUMBER_OF_GAMEPAD_ANALOG_BUTTONS };
enum { NUMBER_OF_GAMEPAD_BUTTONS = 8 };
enum { _gamepad_stick_left, _gamepad_stick_right, NUMBER_OF_GAMEPAD_STICKS };
enum { _event_type_null, _event_type_button, _event_type_left_stick, _event_type_right_stick };
enum { HALO_MENU_TYPING_KEY, HALO_MENU_TYPING_POINTER };
typedef struct { short x, y; } point2d;
struct event_record {
    short type;
    short controller_index;
    union { point2d stick; struct { byte index; byte value; byte pointer; } button; long value; } data;
};
struct gamepad_state {
    byte analog_buttons[NUMBER_OF_GAMEPAD_ANALOG_BUTTONS];
    byte analog_button_thresholds[NUMBER_OF_GAMEPAD_ANALOG_BUTTONS];
    byte buttons[NUMBER_OF_GAMEPAD_BUTTONS];
    point2d sticks[NUMBER_OF_GAMEPAD_STICKS];
};
static struct { struct { boolean initialized; } state; } event_manager_globals = { { TRUE } };
static struct gamepad_state pad;
static boolean input_has_gamepad(short index) { return index == 0; }
static struct gamepad_state const *input_get_gamepad_state(short index) { return index == 0 ? &pad : NULL; }
static struct event_record queued[32];
static int queued_count;
static void queue_event(struct event_record *event, short controller_index) {
    if (controller_index == 0 && event->type != _event_type_null) queued[queued_count++] = *event;
}
/* the PASSWORD screen: X's action waits for a tap while a letter is being typed */
struct widget_instance { long definition_tag_index; };
struct halo_menu_meta { unsigned char typing; };
static struct widget_instance x_action = { 7 };
static struct halo_menu_meta const *pc_menu_meta(long tag) { static struct halo_menu_meta meta; meta.typing = tag == 7 ? HALO_MENU_TYPING_POINTER : HALO_MENU_TYPING_KEY; return &meta; }
static struct widget_instance *ui_bar_action_widget(struct widget_instance *screen, short button, boolean caption) {
    (void)screen; (void)caption; return button == _gamepad_analog_button_x ? &x_action : NULL;
}
static boolean ui_widget_port_text_editing(void) { return TRUE; }
__PIECES__
int main(void) {
    struct widget_instance screen = { 0 };
    int index, buttons = 0;
    /* a stick deflected with a y whose low byte is not 0 (the byte a button's pointer shares), and X held */
    pad.sticks[_gamepad_stick_left].x = 0x0101;
    pad.sticks[_gamepad_stick_left].y = 0x0101;
    pad.sticks[_gamepad_stick_right].y = 0x0101;
    pad.buttons[_gamepad_analog_button_x] = 1;
    event_manager_update();
    for (index = 0; index < queued_count; index++) {
        if (queued[index].type != _event_type_button) continue;
        buttons++;
        assert(queued[index].data.button.pointer == 0);
        assert(queued[index].data.button.index == _gamepad_analog_button_x);
        /* the held X is a key, so the letter being typed takes it and HIDE/SHOW does not flip */
        assert(ui_bar_key_is_letter(&screen, &queued[index]));
    }
    assert(buttons == 1);
    return 0;
}
'''


def test_polled_button_events_are_keys():
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        raise SkipTest("A C compiler is required for the event probe")
    manager = (ROOT / "source/interface/event_manager.c").read_text()
    widgets = (ROOT / "source/interface/ui_widget.c").read_text()
    pieces = [function(manager, "void event_manager_update(\n"),
              function(widgets, "static boolean ui_bar_key_is_letter(\n")]
    program = HARNESS.replace("__PIECES__", "\n".join(pieces))
    with tempfile.TemporaryDirectory() as directory:
        probe = Path(directory) / "probe.c"
        binary = Path(directory) / "probe"
        probe.write_text(program)
        subprocess.run([compiler, "-O1", "-Wall", "-Werror", "-Wno-unused-function", "-ftrivial-auto-var-init=pattern",
                        str(probe), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


def unzeroed_records(source):
    """(line, name) for each event record declared in a function without an initializer and filled field by field
    (an assignment to name.type or name.data) without a memset of it first"""
    found = []
    for match in re.finditer(r"\bstruct event_record (\w+);", source):
        name = match.group(1)
        rest = source[match.end():]
        fill = re.search(rf"\b{name}\.(?:type|data)\b[^=;]*=[^=]", rest)
        if not fill:
            continue
        before = rest[:fill.start()]
        if re.search(rf"memset\(&{name}\b", before):
            continue
        found.append((source.count("\n", 0, match.start()) + 1, name))
    return found


def test_every_event_record_on_the_stack_starts_zeroed():
    found = {}
    for folder in SOURCES:
        for path in folder.rglob("*.c"):
            if "third_party" in path.parts:
                continue
            sites = unzeroed_records(path.read_text(errors="replace"))
            if sites:
                found[str(path.relative_to(ROOT))] = sites
    assert not found, f"event records filled without being zeroed first: {found}"


def test_the_scan_finds_an_unzeroed_record():
    source = (ROOT / "source/interface/event_manager.c").read_text()
    broken = source.replace("struct event_record event = {0};\n\n\t\t\t\t\t\tevent.type = _event_type_button;",
                            "struct event_record event;\n\n\t\t\t\t\t\tevent.type = _event_type_button;")
    assert broken != source
    assert unzeroed_records(broken)
