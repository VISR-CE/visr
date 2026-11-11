"""Exercise menu input classification from the SDL event pump without a window."""

from pathlib import Path
import shutil
import subprocess

import pytest


ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("ios", [False, True])
def test_keyboard_activity_requires_hardware_on_ios(tmp_path, ios):
    source = (ROOT / "port/linux/src/sdl_platform.c").read_text()
    activity = source.split("case SDL_EVENT_KEY_UP:", 1)[1].split(
        "if (event.key.scancode < SDL_SCANCODE_COUNT)", 1)[0]
    program = tmp_path / "keyboard.c"
    program.write_text(r'''
#include <assert.h>
#include "menu_input.h"
static struct menu_input_tracker state;
static int hardware_keyboard;
static unsigned int input_press_count;
static int key_counts_as_press(int scancode, unsigned int mod) { return 1; }
int host_menu_keyboard(void) { return hardware_keyboard; }
static void platform_menu_activity(int pad, float dx, float dy, int motion) {
    menu_input_activity(&state, pad, dx, dy, motion);
}
static void key_event(int down, int repeat) {
    struct { struct { int down, repeat, scancode; unsigned int mod; } key; } event = {{down, repeat, 0, 0}};
''' + activity + r'''
}
int main(void) {
    for (hardware_keyboard = 0; hardware_keyboard <= 1; hardware_keyboard++) {
        menu_input_start(&state, 1, 0, 0);
        key_event(0, 0);
        assert(state.pad);
        key_event(1, 1);
        assert(state.pad);
        key_event(1, 0);
#ifdef HALO_IOS
        assert(state.pad == !hardware_keyboard);
#else
        assert(!state.pad);
#endif
    }
    return 0;
}
''')
    binary = tmp_path / "keyboard"
    subprocess.run([shutil.which("cc"), "-std=c11", "-Wall", "-Werror",
                    "-Wno-unused-function", *(["-DHALO_IOS=1"] if ios else []),
                    "--include-directory=" + str(ROOT / "port/linux/src"),
                    str(program), "--output=" + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


@pytest.mark.parametrize("event_kind", ["motion", "button"])
def test_pen_mouse_events_follow_touch_classification(tmp_path, event_kind):
    source = (ROOT / "port/linux/src/sdl_platform.c").read_text()
    motion = source.split("case SDL_EVENT_MOUSE_MOTION:", 1)[1].split(
        "#ifndef HALO_ILP32", 1)[0]
    button = source.split("case SDL_EVENT_MOUSE_BUTTON_UP:", 1)[1].split(
        "if (event.button.button < 32)", 1)[0]
    program = tmp_path / "mouse.c"
    program.write_text(r'''
#include <assert.h>
#include "menu_input.h"
#define SDL_TOUCH_MOUSEID ((unsigned int)-1)
#define SDL_PEN_MOUSEID ((unsigned int)-2)
static struct menu_input_tracker state;
static int menu_ignore_warp;
static unsigned int input_press_count;
static float menu_warp_x, menu_warp_y;
static void platform_menu_activity(int pad, float dx, float dy, int motion) {
    menu_input_activity(&state, pad, dx, dy, motion);
}
static void mouse_motion(unsigned int which, float dx) {
    struct { struct { unsigned int which; float x, y, xrel, yrel; } motion; } event = {
        {which, 50, 50, dx, 0}
    };
''' + motion + r'''
}
static void mouse_button(unsigned int which, int down) {
    struct { struct { unsigned int which; int down; } button; } event = {{which, down}};
''' + button + r'''
}
int main(int argc, char **argv) {
    assert(argc == 2);
    unsigned int ids[] = {SDL_TOUCH_MOUSEID, SDL_PEN_MOUSEID, 1};
    for (unsigned int i = 0; i < 3; i++) {
        if (!strcmp(argv[1], "motion")) {
            menu_input_start(&state, 1, 0, 0);
            mouse_motion(ids[i], 3);
            assert(state.pad);
            mouse_motion(ids[i], 4);
            assert(state.pad == (i < 2));
        } else {
            menu_input_start(&state, 0, 0, 0);
            mouse_button(ids[i], 0);
            assert(!state.pad);
            mouse_button(ids[i], 1);
            assert(state.pad == (i < 2));
        }
    }
    /* A synthetic warp must still be ignored for a real mouse. */
    menu_input_start(&state, 1, 0, 0);
    menu_ignore_warp = 1;
    menu_warp_x = menu_warp_y = 50;
    mouse_motion(1, 40);
    assert(state.pad && !menu_ignore_warp);
    mouse_motion(1, 4);
    assert(!state.pad);
    return 0;
}
''')
    binary = tmp_path / "mouse"
    subprocess.run([shutil.which("cc"), "-std=c11", "-Wall", "-Werror",
                    "-Wno-unused-function", "--include-directory=" + str(ROOT / "port/linux/src"),
                    str(program), "--output=" + str(binary)], check=True)
    subprocess.run([str(binary), event_kind], check=True)


@pytest.mark.parametrize("scancode,mod,counts", [
    ("SDL_SCANCODE_LGUI", "0", 0),
    ("SDL_SCANCODE_LSHIFT", "0", 0),
    ("SDL_SCANCODE_RCTRL", "0", 0),
    ("SDL_SCANCODE_CAPSLOCK", "0", 0),
    ("SDL_SCANCODE_M", "SDL_KMOD_GUI", 0),
    ("SDL_SCANCODE_M", "SDL_KMOD_CTRL | SDL_KMOD_GUI", 0),
    ("SDL_SCANCODE_3", "SDL_KMOD_SHIFT | SDL_KMOD_GUI", 0),
    ("SDL_SCANCODE_A", "SDL_KMOD_ALT", 0),
    ("SDL_SCANCODE_A", "0", 1),
    ("SDL_SCANCODE_A", "SDL_KMOD_SHIFT", 1),
])
def test_intro_skip_ignores_modifiers_and_chords(tmp_path, scancode, mod, counts):
    source = (ROOT / "port/linux/src/sdl_platform.c").read_text()
    helper = "static int key_counts_as_press" + source.split(
        "static int key_counts_as_press", 1)[1].split("\nunsigned int platform_input_press_count", 1)[0]
    program = tmp_path / "chord.c"
    program.write_text(r'''
#include <assert.h>
enum { SDL_SCANCODE_A = 4, SDL_SCANCODE_M = 16, SDL_SCANCODE_3 = 32,
    SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_LALT, SDL_SCANCODE_LGUI,
    SDL_SCANCODE_RCTRL, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_RALT, SDL_SCANCODE_RGUI,
    SDL_SCANCODE_MODE, SDL_SCANCODE_CAPSLOCK = 57 };
#define SDL_KMOD_SHIFT 0x3u
#define SDL_KMOD_CTRL 0xc0u
#define SDL_KMOD_ALT 0x300u
#define SDL_KMOD_GUI 0xc00u
''' + helper + r'''
int main(void) {
    assert(key_counts_as_press(''' + scancode + ", " + mod + r''') == ''' + str(counts) + r''');
    return 0;
}
''')
    binary = tmp_path / "chord"
    subprocess.run([shutil.which("cc"), "-std=c11", "-Wall", "-Werror", str(program),
                    "--output=" + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
