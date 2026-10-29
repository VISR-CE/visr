"""Exercise scripted browser shortcuts and the shipped action widget's name routing."""

from pathlib import Path
import json
import re
import shutil
import subprocess
import tempfile
from unittest import SkipTest
from xml.etree import ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]


def function(source: str, name: str) -> str:
    """Copy the production function, including its own branches, into the probe."""
    start = re.search(re.escape(name) + r"[^;]*?\n\{", source).start()
    end = source.index("\n}\n", start) + 3
    return source[start:end]


def compile_and_run(source: str) -> None:
    compiler = shutil.which("clang") or shutil.which("cc")
    if compiler is None:
        raise SkipTest("A C compiler is required for the browser shortcut probe")
    with tempfile.TemporaryDirectory(prefix="browser-shortcuts-") as temporary:
        path = Path(temporary) / "shortcuts.c"
        executable = Path(temporary) / "shortcuts"
        path.write_text(source)
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", str(path), "--output", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


def test_scripted_x_and_y_emit_analog_buttons_for_exact_press_window() -> None:
    """A missing or swapped X/Y branch must fail, including the release poll."""
    source = (ROOT / "port/linux/src/xinput_sdl.c").read_text()
    start = source.index("#define TEST_PRESS_POLLS")
    end = source.index("static void test_input_gamepad", start)
    production = source[start:end]
    compile_and_run(r'''
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
enum { XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y };
enum { XINPUT_GAMEPAD_START = 1, XINPUT_GAMEPAD_DPAD_UP = 2, XINPUT_GAMEPAD_DPAD_DOWN = 4,
       XINPUT_GAMEPAD_DPAD_LEFT = 8, XINPUT_GAMEPAD_DPAD_RIGHT = 16 };
static int menu_pad;
void platform_menu_activity(int pad, float dx, float dy, int motion) {
    (void)dx; (void)dy; (void)motion; menu_pad = pad;
}
typedef struct { unsigned short wButtons; unsigned char bAnalogButtons[8]; } XINPUT_GAMEPAD;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "shortcut line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)
''' + production + r'''
int main(void) {
    test_input_presses_parse("x@8 y@16 a@24 b@32 start@40 up@48 down@56 left@64 right@72");
    CHECK(test_press_count == 9);
    for (unsigned long poll = 0; poll < 80; poll++) {
        XINPUT_GAMEPAD pad = {0};
        test_input_presses(&pad, poll);
        CHECK(pad.bAnalogButtons[XINPUT_GAMEPAD_X] == (poll >= 8 && poll < 12 ? 255 : 0));
        CHECK(pad.bAnalogButtons[XINPUT_GAMEPAD_Y] == (poll >= 16 && poll < 20 ? 255 : 0));
        CHECK(pad.bAnalogButtons[XINPUT_GAMEPAD_A] == (poll >= 24 && poll < 28 ? 255 : 0));
        CHECK(pad.bAnalogButtons[XINPUT_GAMEPAD_B] == (poll >= 32 && poll < 36 ? 255 : 0));
        unsigned short expected = poll >= 40 && poll < 44 ? XINPUT_GAMEPAD_START :
            poll >= 48 && poll < 52 ? XINPUT_GAMEPAD_DPAD_UP :
            poll >= 56 && poll < 60 ? XINPUT_GAMEPAD_DPAD_DOWN :
            poll >= 64 && poll < 68 ? XINPUT_GAMEPAD_DPAD_LEFT :
            poll >= 72 && poll < 76 ? XINPUT_GAMEPAD_DPAD_RIGHT : 0;
        CHECK(pad.wButtons == expected);
    }
    test_press_count = 0;
    test_input_presses_parse("x@0 y@0");
    XINPUT_GAMEPAD pad = {0};
    test_input_presses(&pad, 0);
    CHECK(pad.bAnalogButtons[XINPUT_GAMEPAD_X] == 255 && pad.bAnalogButtons[XINPUT_GAMEPAD_Y] == 255);
    test_press_count = 0;
    test_input_presses_parse("ka@0 none@8 knone@16");
    pad = (XINPUT_GAMEPAD){0};
    test_input_presses(&pad, 0);
    CHECK(!menu_pad && pad.bAnalogButtons[XINPUT_GAMEPAD_A] == 255);
    pad = (XINPUT_GAMEPAD){0};
    test_input_presses(&pad, 8);
    CHECK(menu_pad && !pad.wButtons && !pad.bAnalogButtons[XINPUT_GAMEPAD_A]);
    test_input_presses(&pad, 16);
    CHECK(!menu_pad);
    return 0;
}
''')


def test_browser_action_widget_routes_refresh_and_internet_create_after_name_truncation() -> None:
    """The action table must keep both name-based callbacks valid in 31 characters."""
    root = ET.parse(ROOT / "port/assets/menus/xbox/main_menu.multiplayer_type_select.join_game.xml")
    widgets = {widget.get("name"): widget for widget in root.iter("widget")}
    screen = next(widget for name, widget in widgets.items() if name.endswith("/join_game_screen"))
    actions = [widgets[child.get("widget")] for child in screen.findall("child")
               if child.get("widget") in widgets and widgets[child.get("widget")].find('on[@event="x"]') is not None]
    assert len(actions) == 1
    action = actions[0]
    assert [(handler.get("event"), handler.get("run")) for handler in action.findall("on")] == [
        ("x", "gamespy select button"), ("y", "join controller to mp game"), ("y", "mp type set mode")]
    target = action.find('on[@event="y"][@open]').get("open")
    assert target == r"ui\shell\main_menu\multiplayer_type_select\connected\connected_map_select_wrapper"
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    mode = function(source, "static void multiplayer_mode_set(")
    select = function(source, "static boolean lobby_browser_select(")
    start = select.index('\tif (strstr(widget->name, "button_refresh"))')
    end = select.index('\n\tif (row == NONE', start)
    refresh = select[start:end]
    leaf = action.get("name").rsplit("/", 1)[-1]
    compile_and_run(r'''
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define TRUE 1
#define FALSE 0
#define SOUND_FORWARD 7
struct widget_instance { char name[32]; };
enum { _multiplayer_mode_host_internet = 1, _multiplayer_mode_host_lan,
       _multiplayer_mode_lan, _multiplayer_mode_direct_link, _multiplayer_mode_server_browser };
static struct { int mode, invite_pending; } multiplayer;
static struct { int begin_time; } lobby_browser;
static int refreshes, sound;
static void p2p_lobby_refresh(void) { refreshes++; }
static void platform_log(const char *format, ...) { (void)format; }
static int system_milliseconds(void) { return 1234; }
static void ui_play_audio_feedback_sound(int value) { sound = value; }
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "browser action line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)
''' + mode + '\nstatic int refresh_action(struct widget_instance *widget) {\n' + refresh + '\nreturn FALSE;\n}\n' + r'''
int main(void) {
    struct widget_instance widget;
    snprintf(widget.name, sizeof(widget.name), "%.31s", __NAME__);
    CHECK(refresh_action(&widget));
    CHECK(refreshes == 1 && lobby_browser.begin_time == 1234 && sound == SOUND_FORWARD);
    multiplayer.invite_pending = TRUE;
    multiplayer_mode_set(&widget);
    CHECK(multiplayer.mode == _multiplayer_mode_host_internet && !multiplayer.invite_pending);
    return 0;
}
'''.replace("__NAME__", json.dumps(leaf)))
