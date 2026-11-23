"""menu_screen_up_log (port/linux/game/menu_functions.c) writes "menus: screen up <name> at frame N" once per new active screen.

The real function is cut out of the source and compiled against stubs for the widget, the tag names and the log; a scripted
sequence of active screens must log exactly one line per change (none while it stays, one on go-back, none in play).
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "port/linux/game/menu_functions.c").read_text()

HARNESS = r'''
#include <stdio.h>
#include <stdarg.h>
#define NONE (-1)
struct widget_instance { long definition_tag_index; long creation_time; };
static struct widget_instance a = {1, 100}, b = {2, 200}, a2 = {1, 300}, pause = {3, 400}, item = {4, 500}, *active;
static unsigned long frames;
struct widget_instance *ui_widget_port_active_screen(short p) { (void)p; return active; }
char *tag_get_name(long t) { return t == 1 ? "ui\\shell\\main_menu" : t == 2 ? "ui\\shell\\settings" : t == 3 ? "ui\\shell\\pause_game\\pause_game" : t == 4 ? "ui\\shell\\pause_game\\item" : 0; }
unsigned long platform_clock_frames(void) { return frames; }
void platform_log(char const *f, ...) { va_list v; va_start(v, f); vprintf(f, v); putchar('\n'); va_end(v); }
%s
int main(void)
{
	struct widget_instance *script[] = { 0, &a, &a, &b, &b, 0, &a, &a2 };
	for (int i = 0; i < 8; i++) { frames = 10 * i; active = script[i]; menu_screen_up_log(); }
	return 0;
}
'''


def function():
    start = SOURCE.index("static void menu_screen_up_log(void)")
    end = SOURCE.index("\n}\n", start) + 3
    return SOURCE[start:end]


def test_logs_once_per_new_screen():
    with tempfile.TemporaryDirectory() as tmp:
        src, exe = Path(tmp) / "t.c", Path(tmp) / "t"
        src.write_text(HARNESS % function())
        subprocess.run(["cc", "-o", str(exe), str(src)], check=True)
        lines = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout.splitlines()
    assert lines == [
        "menus: screen up ui\\shell\\main_menu at frame 10",
        "menus: screen up ui\\shell\\settings at frame 30",
        "menus: screen down at frame 50",
        "menus: screen up ui\\shell\\main_menu at frame 60",
        "menus: screen up ui\\shell\\main_menu at frame 70",
    ]


def test_polled_by_the_menu_pump():
    poll = SOURCE[SOURCE.index("void menu_invite_poll"):]
    assert poll.index("menu_screen_up_log();") < poll.index("main_port_main_menu_loaded()")   # before the gate


GAME_HARNESS = HARNESS.replace(
    "struct widget_instance *script[] = { 0, &a, &a, &b, &b, 0, &a, &a2 };",
    "struct widget_instance *script[] = { &a, 0, 0, &pause, &item, &pause, 0, &a2 };")


def test_screens_opened_over_a_running_game_are_logged():
    """a game's pause menu and its items (campaign and multiplayer pause rows of screens.tsv), then the main menu again in the
    same run (pause_sp_i3 quits to it): the log is not gated on the main menu being up"""
    with tempfile.TemporaryDirectory() as tmp:
        src, exe = Path(tmp) / "t.c", Path(tmp) / "t"
        src.write_text(GAME_HARNESS % function())
        subprocess.run(["cc", "-o", str(exe), str(src)], check=True)
        lines = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout.splitlines()
    assert lines == [
        "menus: screen up ui\\shell\\main_menu at frame 0",
        "menus: screen down at frame 10",
        "menus: screen up ui\\shell\\pause_game\\pause_game at frame 30",
        "menus: screen up ui\\shell\\pause_game\\item at frame 40",
        "menus: screen up ui\\shell\\pause_game\\pause_game at frame 50",
        "menus: screen down at frame 60",
        "menus: screen up ui\\shell\\main_menu at frame 70",
    ]
