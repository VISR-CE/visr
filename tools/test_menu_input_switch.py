"""Which input the menus follow: one record, changed by one rule, read by everything drawn for an input.

Bug classes held here:
- two records of the input (the legend's and the option rows') that different writers changed, so a fix to one
  writer left the other record behind;
- events that are not the player choosing an input counting as one: a window focus change and the system's cursor
  report after it (Command-Tab away and back), a pointer entering or leaving the window, a warp, a report of an
  unchanged position, a modifier key alone, a key repeat, a software keyboard's keys, touch-synthesized mouse motion;
- held state counting every frame: a key, a mouse button or a stick held through a mouse movement taking the input
  back on the next frame.

Each probe compiles the game's own functions out of xinput_sdl.c (the Apple builds' event writers behind the host's
pointer gate, and the desktop builds' polled writers) and plays sequences of events through them, checking after
every step which input is in use and that the help wording and the rows' controls are the ones for it.
"""
from pathlib import Path
import re
import shutil
import subprocess

import pytest

from tools.test_controller_glyphs import block

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'port/linux/src/xinput_sdl.c'

COMMON = r'''
static struct halo_glyph_input glyph_menu_input;
static int glyph_menus_active;
static struct halo_glyph_motion glyph_mouse_motion;
static int dev, keyboard = 1;
static char const *family = "";
static int host_development_hooks(void) { return dev; }
static char const *config_string(char const *key) { (void)key; return family; }
static int platform_menu_keyboard(void) { return keyboard; }

/* after every step: the input in use is the expected one, and the wording and the controls are that input's */
static int step;
static void expect(int source)
{
	static const int kinds[] = { HALO_INPUT_TOUCH, 0, HALO_INPUT_CONTROLLER, HALO_INPUT_KEYBOARD, HALO_INPUT_KEYBOARD };
	static const int controls[] = { 2, 0, 0, 0, 1 };
	step++;
	/* (the desktop builds draw the game's own controller legend, so their wording is always the controller's) */
	if (glyph_menu_input.source != source || halo_menu_input_kind() != (DESKTOP ? HALO_INPUT_CONTROLLER : kinds[source + 2]) ||
		halo_menu_pointer_controls() != controls[source + 2])
	{
		printf("step %d: input %d (expected %d), wording %d, controls %d\n", step, glyph_menu_input.source, source,
			halo_menu_input_kind(), halo_menu_pointer_controls());
		exit(1);
	}
}
#define TOUCH HALO_GLYPH_INPUT_TOUCH
#define PAD HALO_GLYPH_INPUT_CONTROLLER
#define KEYS HALO_GLYPH_INPUT_KEYBOARD
#define MOUSE HALO_GLYPH_INPUT_MOUSE
'''

APPLE = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h> /* apple-only */
#define HALO_IOS 1
#define DESKTOP 0
#define PORT_COUNT 4
typedef struct { unsigned short wButtons; unsigned char bAnalogButtons[8]; short sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XINPUT_GAMEPAD;
#include "halo_controller_glyphs.h"
#include "halo_ui_pointer.h"
#include "halo_host_pointer.h"
#include "halo_pointer_gate.h"
''' + COMMON + r'''
/* FUNCTIONS */

/* events as the host hands them on: each classified once by the pointer gate, then read by the game */
static struct halo_pointer_gate gate;
static void feed(SDL_Event event) { halo_pointer_gate_classify(&gate, &event); halo_controller_glyph_input_event(&event); }
static void window(unsigned type) { SDL_Event event; memset(&event, 0, sizeof(event)); event.type = type; feed(event); }
static void motion(float x, float y, float dx, float dy, unsigned which)
{
	SDL_Event event; memset(&event, 0, sizeof(event)); event.type = SDL_EVENT_MOUSE_MOTION;
	event.motion.x = x; event.motion.y = y; event.motion.xrel = dx; event.motion.yrel = dy; event.motion.which = which;
	feed(event);
}
static void key(int scancode, unsigned modifiers, int repeat)
{
	SDL_Event event; memset(&event, 0, sizeof(event)); event.type = SDL_EVENT_KEY_DOWN;
	event.key.scancode = scancode; event.key.mod = modifiers; event.key.repeat = repeat;
	feed(event);
}
static void button(void)
{
	SDL_Event event; memset(&event, 0, sizeof(event)); event.type = SDL_EVENT_MOUSE_BUTTON_DOWN; feed(event);
}
static void axis(unsigned which, int axis, int value)
{
	SDL_Event event; memset(&event, 0, sizeof(event)); event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
	event.gaxis.which = which; event.gaxis.axis = (Uint8)axis; event.gaxis.value = (Sint16)value;
	feed(event);
}
static void command_tab(float x, float y)
{
	/* away: Command down, focus lost; back: focus gained, the pointer enters, the system reports the cursor */
	key(SDL_SCANCODE_LGUI, SDL_KMOD_LGUI, 0);
	window(SDL_EVENT_WINDOW_FOCUS_LOST); window(SDL_EVENT_WINDOW_MOUSE_LEAVE);
	window(SDL_EVENT_WINDOW_FOCUS_GAINED); window(SDL_EVENT_WINDOW_MOUSE_ENTER);
	motion(x, y, 0, 0, 0);
}

int main(void)
{
	SDL_Event event;
	XINPUT_GAMEPAD pad = {0};
	struct halo_glyph_held pad_use = {0};
	struct host_ui_pointer pointer = {0};
	struct halo_ui_pointer menu_pointer = {0};
	int from;

	expect(PAD);                                                /* a fresh start */
	/* every input, then every event that is not the player choosing one: the input stays */
	for (from = 0; from < 4; from++)
	{
		switch (from)
		{
		case 0: key(SDL_SCANCODE_DOWN, 0, 0); expect(KEYS); break;
		case 1: motion(100, 100, 5, 0, 0); motion(110, 100, 10, 0, 0); expect(MOUSE); break;
		case 2:
			memset(&event, 0, sizeof(event)); event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.which = 22;
			feed(event); expect(PAD); assert(glyph_menu_input.gamepad == 22); break;
		default: window(SDL_EVENT_FINGER_DOWN); expect(TOUCH); break;
		}
		command_tab(110, 100); expect(glyph_menu_input.source);
		{
			int source = glyph_menu_input.source;

			window(SDL_EVENT_WINDOW_FOCUS_GAINED); motion(500, 400, 390, 300, 0); expect(source);   /* a warp */
			motion(500, 400, 0, 0, 0); expect(source);                    /* a report of the same place */
			window(SDL_EVENT_WINDOW_MOUSE_LEAVE); window(SDL_EVENT_WINDOW_MOUSE_ENTER);
			motion(505, 400, 5, 0, 0); expect(source);                    /* the report after an enter */
			motion(505, 400, 0, 0, 0); expect(source);
			key(SDL_SCANCODE_RGUI, 0, 0); key(SDL_SCANCODE_LSHIFT, 0, 0); expect(source);   /* modifiers alone */
			key(SDL_SCANCODE_TAB, SDL_KMOD_LGUI, 0); expect(source);      /* a Command chord */
			key(SDL_SCANCODE_DOWN, 0, 1); expect(source);                 /* a repeat */
			keyboard = 0; key(SDL_SCANCODE_A, 0, 0); expect(source); keyboard = 1;   /* a software keyboard */
			motion(600, 400, 95, 0, SDL_TOUCH_MOUSEID); expect(source);   /* touch-synthesized motion */
			motion(506, 400, 1, 0, 0); expect(source);                    /* a point of wobble */
			motion(505, 400, -1, 0, 0); motion(506, 400, 1, 0, 0); motion(505, 400, -1, 0, 0); expect(source);
			pointer.active = 1; pointer.moved = 0; pointer.radius = 0;
			halo_controller_glyph_pointer_activity(1, &pointer); expect(source);   /* an idle pointer */
			menu_pointer.moved = 1; halo_menu_pointer_activity(&menu_pointer); expect(source);
			glyph_pad_activity(&pad_use, &pad); expect(source);          /* an idle pad */
		}
	}
	/* every pair of inputs, both ways */
	key(SDL_SCANCODE_UP, 0, 0); expect(KEYS);
	motion(506, 400, 1, 0, 0); expect(KEYS);                         /* a slow mouse: a point at a time */
	motion(507, 400, 1, 0, 0); expect(MOUSE);
	key(SDL_SCANCODE_UP, 0, 0); expect(KEYS);
	button(); expect(MOUSE);
	memset(&event, 0, sizeof(event)); event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.which = 21;
	feed(event); expect(PAD);
	motion(520, 400, 13, 0, 0); expect(MOUSE);
	window(SDL_EVENT_FINGER_DOWN); expect(TOUCH);
	motion(540, 400, 20, 0, 0); expect(MOUSE);
	window(SDL_EVENT_FINGER_MOTION); expect(TOUCH);
	key(SDL_SCANCODE_DOWN, 0, 0); expect(KEYS);
	window(SDL_EVENT_FINGER_DOWN); expect(TOUCH);
	feed(event); expect(PAD);
	window(SDL_EVENT_FINGER_DOWN); expect(TOUCH);
	feed(event); expect(PAD);
	key(SDL_SCANCODE_DOWN, 0, 0); expect(KEYS);
	feed(event); expect(PAD);
	memset(&event, 0, sizeof(event)); event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.y = 1; feed(event); expect(MOUSE);
	/* the host's pointer notes what makes no SDL event: a finger or a pinch, a scripted click */
	key(SDL_SCANCODE_DOWN, 0, 0);
	pointer.moved = 1; pointer.radius = 22; halo_controller_glyph_pointer_activity(1, &pointer); expect(TOUCH);
	pointer.radius = 0; halo_controller_glyph_pointer_activity(1, &pointer); expect(TOUCH);   /* movement is SDL's to note */
	pointer.moved = 0; pointer.left_clicks = 1; halo_controller_glyph_pointer_activity(1, &pointer); expect(MOUSE);
	pointer.left_clicks = 0;
	halo_controller_glyph_pointer_activity(0, &pointer); expect(MOUSE);   /* (no menu up) */
	/* held state counts when it starts: a stick held through a mouse movement does not take the input back */
	key(SDL_SCANCODE_DOWN, 0, 0);
	pad.sThumbLX = 20000; glyph_pad_activity(&pad_use, &pad); expect(PAD);
	motion(560, 400, 20, 0, 0); expect(MOUSE);
	glyph_pad_activity(&pad_use, &pad); glyph_pad_activity(&pad_use, &pad); expect(MOUSE);
	/* a control counts when it starts though another is held: a button pressed with the stick still over */
	pad.wButtons = 1; glyph_pad_activity(&pad_use, &pad); expect(PAD);
	motion(580, 400, 20, 0, 0); expect(MOUSE);
	glyph_pad_activity(&pad_use, &pad); expect(MOUSE);
	pad.bAnalogButtons[2] = 255; glyph_pad_activity(&pad_use, &pad); expect(PAD);   /* a trigger, both still held */
	motion(600, 400, 20, 0, 0); expect(MOUSE);
	pad.sThumbRY = -20000; glyph_pad_activity(&pad_use, &pad); expect(PAD);         /* the other stick */
	motion(620, 400, 20, 0, 0); expect(MOUSE);
	pad.wButtons = 0; pad.bAnalogButtons[2] = 0; pad.sThumbLX = pad.sThumbRY = 0;
	glyph_pad_activity(&pad_use, &pad); expect(MOUSE);            /* letting go is not using the pad */
	pad.wButtons = 1; glyph_pad_activity(&pad_use, &pad); expect(PAD);
	/* a stick's events count when the stick crosses the dead zone, not while it is held over and its value wavers */
	axis(21, SDL_GAMEPAD_AXIS_LEFTX, 3000); expect(PAD);
	motion(640, 400, 20, 0, 0); expect(MOUSE);
	axis(21, SDL_GAMEPAD_AXIS_LEFTX, 8000); expect(MOUSE);            /* inside the dead zone */
	axis(21, SDL_GAMEPAD_AXIS_LEFTX, -20000); expect(PAD); assert(glyph_menu_input.gamepad == 21);
	motion(660, 400, 20, 0, 0); expect(MOUSE);
	for (from = 0; from < 8; from++) { axis(21, SDL_GAMEPAD_AXIS_LEFTX, -20000 - 100 * from); expect(MOUSE); }
	key(SDL_SCANCODE_DOWN, 0, 0); axis(21, SDL_GAMEPAD_AXIS_LEFTX, -19000); expect(KEYS);
	axis(21, SDL_GAMEPAD_AXIS_LEFTY, 15000); expect(PAD);             /* the stick's other axis starts */
	motion(680, 400, 20, 0, 0); expect(MOUSE);
	axis(22, SDL_GAMEPAD_AXIS_RIGHTX, 15000); expect(PAD); assert(glyph_menu_input.gamepad == 22);   /* another pad */
	motion(700, 400, 20, 0, 0); axis(22, SDL_GAMEPAD_AXIS_RIGHTX, 16000); axis(21, SDL_GAMEPAD_AXIS_LEFTY, 16000);
	expect(MOUSE);
	axis(21, SDL_GAMEPAD_AXIS_LEFTX, 0); axis(21, SDL_GAMEPAD_AXIS_LEFTY, 0); expect(MOUSE);   /* letting go */
	axis(21, SDL_GAMEPAD_AXIS_LEFTX, -20000); expect(PAD);            /* and pushing it over again */
	motion(720, 400, 20, 0, 0); expect(MOUSE);
	axis(21, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -20000); expect(MOUSE);  /* a trigger reads one way */
	axis(21, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 20000); expect(PAD);
	motion(740, 400, 20, 0, 0); axis(21, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 25000); expect(MOUSE);
	axis(21, SDL_GAMEPAD_AXIS_LEFTX, 0); axis(21, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0);
	axis(22, SDL_GAMEPAD_AXIS_RIGHTX, 0); expect(MOUSE);
	memset(&event, 0, sizeof(event)); event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.which = 21;
	feed(event); expect(PAD);
	/* a capture's override stands in for the record for every reader at once, in development builds only */
	family = "keyboard"; expect(PAD);
	dev = 1; assert(halo_menu_input_kind() == HALO_INPUT_KEYBOARD && halo_menu_pointer_controls() == 0);
	family = "touch"; assert(halo_menu_input_kind() == HALO_INPUT_TOUCH && halo_menu_pointer_controls() == 2);
	family = "xbox"; assert(halo_menu_input_kind() == HALO_INPUT_CONTROLLER && halo_menu_pointer_controls() == 0);
	family = ",keyboard"; assert(halo_menu_input_kind() == HALO_INPUT_CONTROLLER);   /* (controller 2's part) */
	printf("%d steps\n", step);
	return 0;
}
'''

DESKTOP = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PLATFORM_MOUSE_BUTTON_COUNT 3
#define SDL_SCANCODE_COUNT 512
#define DESKTOP 1
typedef struct { unsigned short wButtons; unsigned char bAnalogButtons[8]; short sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XINPUT_GAMEPAD;
#include "halo_controller_glyphs.h"
#include "halo_ui_pointer.h"
struct host_ui_pointer { int active, moved, left_clicks, right_clicks, wheel_steps; float radius; };
struct platform_input_state { int focused, mouse_released, mouse_buttons[3]; unsigned char keys[512]; float mouse_dx, mouse_dy, mouse_wheel; };
''' + COMMON + r'''
/* FUNCTIONS */

int main(void)
{
	struct platform_input_state input = {0};
	struct halo_ui_pointer menu_pointer = {0};
	XINPUT_GAMEPAD pad = {0};
	struct halo_glyph_held pad_use = {0};
	int frame;

	input.focused = 1;
	glyph_polled_activity(&input); expect(PAD);
	input.keys[81] = 1; glyph_polled_activity(&input); expect(KEYS);          /* Down */
	/* the key still held, the mouse moves: the mouse takes the input and keeps it on the frames after */
	input.mouse_dx = 6; glyph_polled_activity(&input); expect(MOUSE);
	input.mouse_dx = 0;
	for (frame = 0; frame < 3; frame++) { glyph_polled_activity(&input); expect(MOUSE); }
	input.keys[81] = 0; glyph_polled_activity(&input); expect(MOUSE);
	input.keys[82] = 1; glyph_polled_activity(&input); expect(KEYS);          /* Up */
	input.keys[82] = 0;
	/* with the mouse in use: modifiers alone and a chord with Command or the Windows key are not the keyboard */
	input.mouse_dx = 6; glyph_polled_activity(&input); expect(MOUSE);
	input.mouse_dx = 0;
	input.keys[227] = 1; input.keys[226] = 1; glyph_polled_activity(&input); expect(MOUSE);
	input.keys[43] = 1; glyph_polled_activity(&input); expect(MOUSE);         /* Command-Tab */
	input.keys[43] = 0; input.keys[227] = input.keys[226] = 0; glyph_polled_activity(&input); expect(MOUSE);
	input.keys[231] = 1; input.keys[44] = 1; glyph_polled_activity(&input); expect(MOUSE);   /* both on one frame */
	input.keys[231] = input.keys[44] = 0; glyph_polled_activity(&input); expect(MOUSE);
	input.keys[225] = 1; glyph_polled_activity(&input); expect(MOUSE);
	input.keys[43] = 1; glyph_polled_activity(&input); expect(KEYS);          /* Shift-Tab is the keyboard */
	input.keys[225] = input.keys[43] = 0;
	input.mouse_dx = 6; glyph_polled_activity(&input); expect(MOUSE);
	input.mouse_dx = 0; input.keys[226] = 1; glyph_polled_activity(&input); expect(MOUSE);
	input.keys[226] = 0; input.keys[82] = 1; glyph_polled_activity(&input); expect(KEYS);
	input.keys[82] = 0;
	/* a slow mouse sums its movement; a wobble sums to nothing */
	input.mouse_dx = 1; glyph_polled_activity(&input); expect(KEYS);
	input.mouse_dx = -1; glyph_polled_activity(&input); expect(KEYS);
	input.mouse_dx = 1; glyph_polled_activity(&input); expect(KEYS);
	glyph_polled_activity(&input); expect(MOUSE);
	input.mouse_dx = 0;
	/* a mouse button held does not take the input back from a key pressed after it */
	input.keys[81] = 1; glyph_polled_activity(&input); expect(KEYS);
	input.mouse_buttons[0] = 1; glyph_polled_activity(&input); expect(MOUSE);
	input.keys[81] = 0; glyph_polled_activity(&input);
	input.keys[81] = 1; glyph_polled_activity(&input); expect(KEYS);
	glyph_polled_activity(&input); expect(KEYS);
	input.mouse_buttons[0] = 0; input.keys[81] = 0;
	/* nothing counts while the window is not in front, or the mouse is released to the system */
	input.focused = 0; input.mouse_dx = 50; input.mouse_buttons[1] = 1; glyph_polled_activity(&input); expect(KEYS);
	input.focused = 1; input.mouse_released = 1; glyph_polled_activity(&input); expect(KEYS);
	input.mouse_released = 0; input.mouse_dx = 0; input.mouse_buttons[1] = 0; glyph_polled_activity(&input);
	input.mouse_wheel = 1; glyph_polled_activity(&input); expect(MOUSE);
	input.mouse_wheel = 0;
	/* the menus' own pointer: the desktop builds' only report of the menus' mouse */
	input.keys[81] = 1; glyph_polled_activity(&input); expect(KEYS);
	halo_menu_pointer_activity(&menu_pointer); expect(KEYS);                  /* idle */
	menu_pointer.moved = 1; halo_menu_pointer_activity(&menu_pointer); expect(MOUSE);
	menu_pointer.radius = 22; halo_menu_pointer_activity(&menu_pointer); expect(TOUCH);
	/* a pad: when its use starts */
	pad.sThumbLX = 20000; glyph_pad_activity(&pad_use, &pad); expect(PAD);
	menu_pointer.radius = 0; halo_menu_pointer_activity(&menu_pointer); expect(MOUSE);
	glyph_pad_activity(&pad_use, &pad); expect(MOUSE);
	/* a control counts when it starts though another is held: a button pressed with the stick still over */
	pad.wButtons = 0x1000; glyph_pad_activity(&pad_use, &pad); expect(PAD);
	halo_menu_pointer_activity(&menu_pointer); expect(MOUSE);
	glyph_pad_activity(&pad_use, &pad); expect(MOUSE);
	pad.wButtons = 0; pad.sThumbLX = 0; glyph_pad_activity(&pad_use, &pad); expect(MOUSE);
	pad.bAnalogButtons[0] = 255; glyph_pad_activity(&pad_use, &pad); expect(PAD);
	printf("%d steps\n", step);
	return 0;
}
'''

SHARED = ('static void glyph_input_note(', 'static unsigned int glyph_gamepad_controls(', 'static void glyph_pad_activity(',
          'static struct halo_glyph_input glyph_input_shown(', 'int halo_menu_input_kind(',
          'void halo_menu_pointer_activity(', 'int halo_menu_pointer_controls(',
          'void halo_controller_glyph_pointer_activity(')


def run(tmp_path, name, program, markers, flags):
    source = SOURCE.read_text()
    functions = '\n'.join(block(source, marker) for marker in markers)
    probe, binary = tmp_path / f'{name}.c', tmp_path / name
    probe.write_text(program.replace('/* FUNCTIONS */', functions))
    subprocess.run([shutil.which('cc'), '-std=gnu11', '-Wall', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
                    '-iquote', str(ROOT / 'port/linux/include'), '-iquote', str(ROOT / 'port/runtime/include'),
                    *flags, str(probe), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


def test_apple_builds_follow_one_input_through_every_event(tmp_path):
    if not shutil.which('pkg-config') or subprocess.run(['pkg-config', '--exists', 'sdl3']).returncode:
        pytest.skip('SDL3 headers are required')
    flags = subprocess.check_output(['pkg-config', '--cflags', 'sdl3'], text=True).split()
    output = run(tmp_path, 'apple', APPLE, SHARED + ('static int glyph_axis_event_started(',
                                                   'void halo_controller_glyph_input_event('), flags)
    assert 'steps' in output


def test_desktop_builds_follow_one_input_through_polled_state(tmp_path):
    output = run(tmp_path, 'desktop', DESKTOP, SHARED + ('static void glyph_polled_activity(',), [])
    assert 'steps' in output


def test_there_is_one_record_of_the_input():
    """The second record is gone, and nothing changes the one that is left but its one function."""
    source = SOURCE.read_text()
    assert 'glyph_keyboard_active' not in source and 'glyph_mouse_active' not in source
    writes = [line for line in source.splitlines() if re.search(r'glyph_menu_input(\.\w+)?\s*=[^=]', line)]
    assert writes == ['\tglyph_menu_input = halo_glyph_input_note(glyph_menu_input, source, gamepad, active);'], writes
