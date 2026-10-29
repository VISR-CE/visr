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
int host_menu_keyboard(void) { return hardware_keyboard; }
static void platform_menu_activity(int pad, float dx, float dy, int motion) {
    menu_input_activity(&state, pad, dx, dy, motion);
}
static void key_event(int down, int repeat) {
    struct { struct { int down, repeat; } key; } event = {{down, repeat}};
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
