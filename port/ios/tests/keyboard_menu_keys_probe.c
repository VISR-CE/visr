/* The keyboard's menu keys, from xinput_sdl.c's own keyboard_gamepad: Backspace (and forward delete) is the
secondary action X, nothing else is, and a text-entry screen's Backspace never reaches the menu. */
#include <assert.h>
#include <string.h>
#include <SDL3/SDL.h>
typedef int BOOL;
typedef unsigned char BYTE;
typedef char CHAR;
typedef short SHORT;
#define TRUE 1
#define FALSE 0
#define XINPUT_GAMEPAD_A 0
#define XINPUT_GAMEPAD_B 1
#define XINPUT_GAMEPAD_X 2
#define XINPUT_GAMEPAD_DPAD_UP 1
#define XINPUT_GAMEPAD_DPAD_DOWN 2
#define XINPUT_GAMEPAD_DPAD_LEFT 4
#define XINPUT_GAMEPAD_DPAD_RIGHT 8
#define XINPUT_GAMEPAD_START 16
#define XINPUT_GAMEPAD_BACK 32
#define XINPUT_GAMEPAD_LEFT_THUMB 64
#define PLATFORM_MOUSE_BUTTON_COUNT 8
typedef struct {unsigned short wButtons;unsigned char bAnalogButtons[8];short sThumbLX,sThumbLY,sThumbRX,sThumbRY;} XINPUT_GAMEPAD;
#include "sdl_platform.h"
#include "halo_controller_glyphs.h"
static int glyph_x_destructive;
static BOOL text_typing, text_typing_enter_armed, text_typing_submit;
/* ANALOG */
/* TYPING */
/* KEYBOARD */
static void press(struct platform_input_state *input, int scancode) { memset(input, 0, sizeof(*input)); input->keys[scancode] = 1; input->mouse_released = TRUE; }
int main(void)
{
	struct platform_input_state input;
	XINPUT_GAMEPAD pad;
	int scancode;

	/* a list whose X deletes (profiles, gametypes): Backspace and forward delete are X and only X */
	text_typing = FALSE;
	glyph_x_destructive = TRUE;
	int const secondary[] = {SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_DELETE};
	for (scancode = 0; scancode < 2; scancode++)
	{
		press(&input, secondary[scancode]); memset(&pad, 0, sizeof(pad));
		keyboard_gamepad(&input, &pad);
		assert(pad.bAnalogButtons[XINPUT_GAMEPAD_X] == 0xff);
		assert(pad.bAnalogButtons[XINPUT_GAMEPAD_B] == 0 && pad.bAnalogButtons[XINPUT_GAMEPAD_A] == 0 && !pad.wButtons);
	}
	press(&input, SDL_SCANCODE_ESCAPE); memset(&pad, 0, sizeof(pad));
	keyboard_gamepad(&input, &pad);
	assert(pad.bAnalogButtons[XINPUT_GAMEPAD_B] == 0xff && pad.bAnalogButtons[XINPUT_GAMEPAD_X] == 0);
	/* a screen whose X is a plain action (DEFAULTS applies and saves): Backspace and forward delete do nothing, E
	still presses X */
	glyph_x_destructive = FALSE;
	for (scancode = 0; scancode < 2; scancode++)
	{
		press(&input, secondary[scancode]); memset(&pad, 0, sizeof(pad));
		keyboard_gamepad(&input, &pad);
		assert(pad.bAnalogButtons[XINPUT_GAMEPAD_X] == 0 && pad.bAnalogButtons[XINPUT_GAMEPAD_B] == 0 && !pad.wButtons);
	}
	press(&input, SDL_SCANCODE_E); memset(&pad, 0, sizeof(pad));
	keyboard_gamepad(&input, &pad);
	assert(pad.bAnalogButtons[XINPUT_GAMEPAD_X] == 0xff);
	/* a text-entry screen: Backspace edits the text, the menu sees no action */
	text_typing = TRUE;
	glyph_x_destructive = TRUE;
	for (scancode = 0; scancode < 2; scancode++)
	{
		press(&input, secondary[scancode]); memset(&pad, 0, sizeof(pad));
		keyboard_gamepad(&input, &pad);
		assert(pad.bAnalogButtons[XINPUT_GAMEPAD_X] == 0 && pad.bAnalogButtons[XINPUT_GAMEPAD_B] == 0 && !pad.wButtons);
	}
	/* the legend's label names that key */
	assert(!strcmp(HALO_GLYPH_MENU_SECONDARY_LABEL, "Backspace"));
	return 0;
}
