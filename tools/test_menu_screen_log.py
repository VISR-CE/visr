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
struct widget_instance { long definition_tag_index; };
static struct widget_instance a = {1}, b = {2}, *active;
static unsigned long frames;
struct widget_instance *ui_widget_port_active_screen(short p) { (void)p; return active; }
char *tag_get_name(long t) { return t == 1 ? "ui\\shell\\main_menu" : t == 2 ? "ui\\shell\\settings" : 0; }
unsigned long platform_clock_frames(void) { return frames; }
void platform_log(char const *f, ...) { va_list v; va_start(v, f); vprintf(f, v); putchar('\n'); va_end(v); }
%s
int main(void)
{
	struct widget_instance *script[] = { 0, &a, &a, &b, &b, 0, &a };
	for (int i = 0; i < 7; i++) { frames = 10 * i; active = script[i]; menu_screen_up_log(); }
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
        "menus: screen up ui\\shell\\main_menu at frame 60",
    ]


def test_polled_by_the_menu_pump():
    assert "menu_screen_up_log();" in SOURCE[SOURCE.index("void menu_invite_poll"):]
