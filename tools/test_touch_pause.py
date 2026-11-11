"""Execute the production touch reader with short taps, held axes and menu changes."""

from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import SkipTest

ROOT = Path(__file__).resolve().parents[1]


def test_paused_touch_input_and_shared_hardware():
    source = (ROOT / "port/ios/host/host_touch.m").read_text()
    start = source.find("static int touch_menu_up(")
    if start < 0:
        start = source.index("int host_ios_gamepad_axis(")
    end = source.index("\nvoid host_ios_touch_frame(", start)
    reader = source[start:end]
    setters = source[source.index("static void button_state("):source.index("void host_ios_touch_initialize(")]
    latches = "\n".join(line for line in source.splitlines()
                        if line.startswith("static struct host_touch_latch "))
    frame_start = source.index("void host_ios_touch_frame(")
    reset_end = source.index("\nvoid host_touch_replay_active(", frame_start)
    frame_reset = source[frame_start:reset_end]
    glyph_state = next(line for line in source.splitlines()
                       if line.startswith("static atomic_int glyph_touch_active"))
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        raise SkipTest("A C compiler is required for the touch pause probe")
    harness = r'''
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "host_touch_replay.h"
#include "host_touch_latch.h"
#include <math.h>
typedef int BOOL;
typedef short Sint16;
enum { NO, YES };
typedef struct { int id; int axes[6]; unsigned char buttons[32]; } SDL_Gamepad;
enum { SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
       SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
       SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
       SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
       SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_MISC1,
       SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
       SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, SDL_GAMEPAD_BUTTON_TOUCHPAD, SDL_GAMEPAD_BUTTON_MISC2,
       SDL_GAMEPAD_BUTTON_MISC3, SDL_GAMEPAD_BUTTON_MISC4, SDL_GAMEPAD_BUTTON_MISC5,
       SDL_GAMEPAD_BUTTON_MISC6, SDL_GAMEPAD_BUTTON_COUNT };
static int pause_pending;
__GLYPH_STATE__
enum { SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_RIGHTX,
       SDL_GAMEPAD_AXIS_RIGHTY, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_GAMEPAD_AXIS_COUNT };
__LATCHES__
static struct host_touch_replay_gate replay_gate;
static int touch_id = 1, primary_hardware, paused, sheet_up, audio_panel_up;
static int host_window_up(void) { return sheet_up; }
static int host_audio_output_up(void) { return audio_panel_up; }
static SDL_Gamepad touch = {1, {12000, -18000, 14000, -20000, 9000, 15000}, {0}};
static SDL_Gamepad hardware = {2, {-24000, 26000, -28000, 30000, 18000, 21000}, {0}};
static int SDL_GetGamepadButton(SDL_Gamepad *pad, int button) { return pad->buttons[button]; }
static int SDL_GetGamepadID(SDL_Gamepad *pad) { return pad->id; }
static int SDL_GetGamepadAxis(SDL_Gamepad *pad, int axis) { return pad->axes[axis]; }
static SDL_Gamepad *SDL_GetGamepadFromID(int id) { assert(id == 2); return &hardware; }
static int __attribute__((unused)) SDL_GetHintBoolean(const char *name, int fallback) {
    assert(!strcmp(name, "VISR_PAUSE_MENU")); (void)fallback; return paused;
}
static SDL_Gamepad *touch_joystick = &touch;
static void SDL_SetJoystickVirtualButton(SDL_Gamepad *pad, int button, int down) { pad->buttons[button] = down; }
static void SDL_SetJoystickVirtualAxis(SDL_Gamepad *pad, int axis, Sint16 value) {
    /* SDL maps the virtual joystick's signed trigger range to 0..32767. */
    pad->axes[axis] = axis >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? (value + 32767) / 2 : value;
}
__SETTERS__
__READER__
__FRAME_RESET__
int main(void) {
    for (int axis = 0; axis < 6; axis++)
        assert(host_ios_gamepad_axis(&touch, axis) == touch.axes[axis]);
    paused = 1;
    /* The sticks reach the menu (the guest drops the player's movement and
       look behind an open menu); fire and grenade, the triggers, do not. */
    for (int axis = 0; axis < 4; axis++)
        assert(host_ios_gamepad_axis(&touch, axis) == touch.axes[axis]);
    assert(host_ios_gamepad_axis(&touch, 4) == 0);
    assert(host_ios_gamepad_axis(&touch, 5) == 0);
    primary_hardware = 2;
    for (int axis = 0; axis < 6; axis++) {
        assert(host_ios_gamepad_axis(&touch, axis) == hardware.axes[axis]);
        assert(host_ios_gamepad_axis(&hardware, axis) == hardware.axes[axis]);
    }
    assert(atomic_load(&glyph_touch_active) == 0);
    paused = 0; primary_hardware = 0;
    for (int axis = 0; axis < 6; axis++)
        assert(host_ios_gamepad_axis(&touch, axis) == touch.axes[axis]);
    atomic_store(&replay_gate.active, 1);
    for (int axis = 0; axis < 6; axis++)
        assert(host_ios_gamepad_axis(&touch, axis) == 0);
    primary_hardware = 2; paused = 1;
    for (int axis = 0; axis < 6; axis++)
        assert(host_ios_gamepad_axis(&touch, axis) == hardware.axes[axis]);

    /* Buttons: every touch button down; hardware idle. */
    primary_hardware = 0; atomic_store(&replay_gate.active, 0);
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) touch.buttons[b] = 1;
    paused = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
        assert(host_ios_gamepad_button(&touch, b) == 1);
    paused = 1;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        int menu = b == SDL_GAMEPAD_BUTTON_SOUTH || b == SDL_GAMEPAD_BUTTON_EAST || b == SDL_GAMEPAD_BUTTON_START ||
            b == SDL_GAMEPAD_BUTTON_DPAD_UP || b == SDL_GAMEPAD_BUTTON_DPAD_DOWN ||
            b == SDL_GAMEPAD_BUTTON_DPAD_LEFT || b == SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
        /* Reload/use, weapon switch, crouch, zoom, flashlight and grenade switch must not reach the game. */
        assert(host_ios_gamepad_button(&touch, b) == menu);
    }
    /* Hardware buttons still merge into the shared pad while paused. */
    primary_hardware = 2; touch.buttons[SDL_GAMEPAD_BUTTON_WEST] = 0;
    hardware.buttons[SDL_GAMEPAD_BUTTON_WEST] = 1;
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_WEST) == 1);
    primary_hardware = 0;
    /* The pause button still works from touch while a menu is up. */
    touch.buttons[SDL_GAMEPAD_BUTTON_START] = 0; pause_pending = 1;
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_START) == 1);
    /* A released tap survives event polls until a read, then all reads in
       that input frame agree. Removing the latch must fail these checks. */
    host_ios_touch_reset(); paused = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        button_state(b, YES); button_state(b, NO);
        host_ios_touch_frame(); host_ios_touch_frame();
        assert(host_ios_gamepad_button(&touch, b) == 1);
        assert(host_ios_gamepad_button(&touch, b) == 1);
        host_ios_touch_frame();
        assert(host_ios_gamepad_button(&touch, b) == 0);
    }
    for (int axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER; axis <= SDL_GAMEPAD_AXIS_RIGHT_TRIGGER; axis++) {
        axis_state(axis, 1); axis_state(axis, -1);
        host_ios_touch_frame();
        assert(host_ios_gamepad_axis(&touch, axis) == 32767);
        assert(host_ios_gamepad_axis(&touch, axis) == 32767);
        host_ios_touch_frame();
        assert(host_ios_gamepad_axis(&touch, axis) == 0);
    }
    /* A new press after a read must survive the next poll even if it was
       released before that poll. */
    button_state(SDL_GAMEPAD_BUTTON_SOUTH, YES); button_state(SDL_GAMEPAD_BUTTON_SOUTH, NO);
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_SOUTH) == 1);
    button_state(SDL_GAMEPAD_BUTTON_SOUTH, YES); button_state(SDL_GAMEPAD_BUTTON_SOUTH, NO);
    host_ios_touch_frame();
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_SOUTH) == 1);
    host_ios_touch_frame();
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_SOUTH) == 0);
    /* Held input continues through later frames using SDL's real state. */
    button_state(SDL_GAMEPAD_BUTTON_SOUTH, YES);
    axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1);
    for (int frame = 0; frame < 3; frame++) {
        assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_SOUTH) == 1);
        assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == 32767);
        host_ios_touch_frame();
    }
    button_state(SDL_GAMEPAD_BUTTON_SOUTH, NO);
    axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -1);
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_SOUTH) == 0);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == 0);

    /* A pause menu drops pending presses it does not read, so reload or fire
       tapped over it does not act when it closes; menu buttons stay latched.
       Replay blocking does not consume a pending press. Reset clears both
       pending and seen presses, and replay-blocked delivery must never
       create one. */
    button_state(SDL_GAMEPAD_BUTTON_WEST, YES); button_state(SDL_GAMEPAD_BUTTON_WEST, NO);
    axis_state(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 1); axis_state(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -1);
    button_state(SDL_GAMEPAD_BUTTON_SOUTH, YES); button_state(SDL_GAMEPAD_BUTTON_SOUTH, NO);
    paused = 1;
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_WEST) == 0);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) == 0);
    host_ios_touch_frame(); host_ios_touch_frame();
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_SOUTH) == 1);
    host_ios_touch_frame(); paused = 0;
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_WEST) == 0);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) == 0);
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_SOUTH) == 0);
    button_state(SDL_GAMEPAD_BUTTON_WEST, YES); button_state(SDL_GAMEPAD_BUTTON_WEST, NO);
    axis_state(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 1); axis_state(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -1);
    atomic_store(&replay_gate.active, 1);
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_WEST) == 0);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) == 0);
    host_ios_touch_frame(); atomic_store(&replay_gate.active, 0);
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_WEST) == 1);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) == 32767);
    button_state(SDL_GAMEPAD_BUTTON_NORTH, YES); button_state(SDL_GAMEPAD_BUTTON_NORTH, NO);
    axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1); axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -1);
    host_ios_touch_reset();
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_WEST) == 0);
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_NORTH) == 0);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) == 0);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == 0);
    atomic_store(&replay_gate.active, 1);
    button_state(SDL_GAMEPAD_BUTTON_SOUTH, YES);
    axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1);
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_SOUTH) == 0);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == 0);
    atomic_store(&replay_gate.active, 0);
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_SOUTH) == 0);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == 0);

    /* A share sheet or the audio output panel drops every latch, pending
       ones too, so a tap made over it never reaches the game afterward. */
    for (int gate = 0; gate < 2; gate++) {
        button_state(SDL_GAMEPAD_BUTTON_WEST, YES); button_state(SDL_GAMEPAD_BUTTON_WEST, NO);
        axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1); axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -1);
        if (gate) audio_panel_up = 1; else sheet_up = 1;
        host_ios_touch_frame();
        sheet_up = 0; audio_panel_up = 0;
        host_ios_touch_frame();
        assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_WEST) == 0);
        assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == 0);
    }
    /* Without a gate the same tap survives the frame. */
    button_state(SDL_GAMEPAD_BUTTON_WEST, YES); button_state(SDL_GAMEPAD_BUTTON_WEST, NO);
    host_ios_touch_frame();
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_WEST) == 1);
    host_ios_touch_reset();

    /* Touch latches cannot press a separate hardware pad. */
    button_state(SDL_GAMEPAD_BUTTON_SOUTH, YES); button_state(SDL_GAMEPAD_BUTTON_SOUTH, NO);
    axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1); axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -1);
    hardware.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 0;
    assert(host_ios_gamepad_button(&hardware, SDL_GAMEPAD_BUTTON_SOUTH) == 0);
    assert(host_ios_gamepad_axis(&hardware, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == 0);
    primary_hardware = 2;
    assert(host_ios_gamepad_button(&touch, SDL_GAMEPAD_BUTTON_WEST) == 1);
    assert(host_ios_gamepad_axis(&touch, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) == 18000);
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="touch-pause-") as temporary:
        probe = Path(temporary) / "touch_pause.c"
        executable = Path(temporary) / "touch_pause"
        probe.write_text(harness.replace("__READER__", reader).replace("__GLYPH_STATE__", glyph_state)
                         .replace("__LATCHES__", latches).replace("__SETTERS__", setters)
                         .replace("__FRAME_RESET__", frame_reset))
        subprocess.run([compiler, "-O2", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "port/ios/host"),
                        str(probe), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    test_paused_touch_input_and_shared_hardware()
