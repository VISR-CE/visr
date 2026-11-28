"""menu_screen_up_log (port/linux/game/menu_functions.c) writes "menus: screen up <name> at frame N" once per new active screen.

The real function is cut out of the source and compiled against stubs for the widget, the tag names and the log; scripted polls
must log exactly one line per new instance. An instance is its address, definition tag and creation time, so the in-place test
changes one object between polls (a reused widget address): it fails if the compare is cut back to the pointer alone.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from probekit import source as probe_source  # noqa: E402

SOURCE = "port/linux/game/menu_functions.c"

HARNESS = r'''
#include <stdio.h>
#include <stdarg.h>
#define NONE (-1)
struct widget_instance { long definition_tag_index; long creation_time; };
static struct widget_instance a = {1, 100}, b = {2, 200}, a2 = {1, 300}, pause = {3, 400}, item = {4, 500}, *active;
static unsigned long frames;
struct widget_instance *ui_widget_port_active_screen(short p) { (void)p; return active; }
char *tag_get_name(long t) {
	return t == 1 ? "ui\\shell\\main_menu" : t == 2 ? "ui\\shell\\settings" : t == 3 ? "ui\\shell\\pause_game\\pause_game" :
		t == 4 ? "ui\\shell\\pause_game\\item" : 0; }
unsigned long platform_clock_frames(void) { return frames; }
void platform_log(char const *f, ...) { va_list v; va_start(v, f); vprintf(f, v); putchar('\n'); va_end(v); }
FUNCTION
int main(void)
{
BODY
	return 0;
}
'''


def run(body):
    """compile the real function with `body` as main's statements; return the log lines. Poll steps are written
    `STEP(frame, expression)`: the active screen becomes `expression`, the frame counter `frame`, and the function runs."""
    program = HARNESS.replace("FUNCTION", probe_source.function(SOURCE, "menu_screen_up_log")).replace("BODY", body)
    program = program.replace("int main(void)", "#define STEP(f, e) do { frames = (f); active = (e); menu_screen_up_log(); } while (0)\nint main(void)")
    with tempfile.TemporaryDirectory() as tmp:
        src, exe = Path(tmp) / "t.c", Path(tmp) / "t"
        src.write_text(program)
        subprocess.run(["cc", "-o", str(exe), str(src)], check=True)
        return subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout.splitlines()


MAIN = "ui\\shell\\main_menu"
SETTINGS = "ui\\shell\\settings"
PAUSE = "ui\\shell\\pause_game\\pause_game"


def test_logs_once_per_new_screen():
    lines = run("""
	STEP(0, 0); STEP(10, &a); STEP(20, &a); STEP(30, &b); STEP(40, &b); STEP(50, 0); STEP(60, &a); STEP(70, &a2);""")
    assert lines == [f"menus: screen up {MAIN} at frame 10", f"menus: screen up {SETTINGS} at frame 30", "menus: screen down at frame 50",
                     f"menus: screen up {MAIN} at frame 60", f"menus: screen up {MAIN} at frame 70"]


def test_same_address_new_instance_is_logged():
    """one object changed in place between polls, as a widget reallocated at a freed one's address: a new creation time is a new
    screen, then a new tag is another; the same object unchanged logs nothing. A pointer-only compare logs only the first line."""
    lines = run("""
	STEP(10, &a); STEP(20, &a); a.creation_time = 101; STEP(30, &a); a.definition_tag_index = 2; STEP(40, &a); STEP(50, &a);""")
    assert lines == [f"menus: screen up {MAIN} at frame 10", f"menus: screen up {MAIN} at frame 30",
                     f"menus: screen up {SETTINGS} at frame 40"]


def test_screens_opened_over_a_running_game_are_logged():
    """a game's pause menu and its items (the pause rows of screens.tsv), then the main menu again in the same run (pause_sp_i3 quits
    to it): the log is not gated on the main menu being up"""
    lines = run("""
	STEP(0, &a); STEP(10, 0); STEP(20, 0); STEP(30, &pause); STEP(40, &item); STEP(50, &pause); STEP(60, 0); STEP(70, &a2);""")
    assert lines == [f"menus: screen up {MAIN} at frame 0", "menus: screen down at frame 10", f"menus: screen up {PAUSE} at frame 30",
                     "menus: screen up ui\\shell\\pause_game\\item at frame 40", f"menus: screen up {PAUSE} at frame 50",
                     "menus: screen down at frame 60", f"menus: screen up {MAIN} at frame 70"]


def test_polled_by_the_menu_pump():
    poll = probe_source.function(SOURCE, "menu_invite_poll")
    assert poll.index("menu_screen_up_log();") < poll.index("main_port_main_menu_loaded()")   # before the gate
