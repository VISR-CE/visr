/* The native Mac app's shortcut mapping: which chords the menu bar owns, which
   function keys are gone, and that the game never sees either. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../port/macos/host_macos_shortcuts.h"

static SDL_KeyboardEvent key(SDL_Scancode scancode, SDL_Keymod mod, int down, int repeat)
{
	SDL_KeyboardEvent event;

	memset(&event, 0, sizeof(event));
	event.scancode = scancode;
	event.mod = mod;
	event.down = down != 0;
	event.repeat = repeat != 0;
	return event;
}

int main(void)
{
	struct host_macos_key_state state;
	SDL_KeyboardEvent event;
	const SDL_Keymod ctrl_cmd = SDL_KMOD_LCTRL | SDL_KMOD_LGUI;

	memset(&state, 0, sizeof(state));
	assert(host_macos_shortcut_for(SDL_SCANCODE_F, ctrl_cmd) == HOST_MACOS_SHORTCUT_FULL_SCREEN);
	assert(host_macos_shortcut_for(SDL_SCANCODE_M, SDL_KMOD_RCTRL | SDL_KMOD_RGUI) == HOST_MACOS_SHORTCUT_MOUSE);
	assert(host_macos_shortcut_for(SDL_SCANCODE_LEFTBRACKET, SDL_KMOD_LGUI) == HOST_MACOS_SHORTCUT_BACK);
	/* no other combination of those keys is ours */
	assert(host_macos_shortcut_for(SDL_SCANCODE_F, SDL_KMOD_LGUI) == HOST_MACOS_SHORTCUT_NONE);
	assert(host_macos_shortcut_for(SDL_SCANCODE_F, SDL_KMOD_LCTRL) == HOST_MACOS_SHORTCUT_NONE);
	assert(host_macos_shortcut_for(SDL_SCANCODE_M, SDL_KMOD_LGUI) == HOST_MACOS_SHORTCUT_NONE);	/* Minimize */
	assert(host_macos_shortcut_for(SDL_SCANCODE_M, ctrl_cmd | SDL_KMOD_LALT) == HOST_MACOS_SHORTCUT_NONE);
	assert(host_macos_shortcut_for(SDL_SCANCODE_LEFTBRACKET, SDL_KMOD_LGUI | SDL_KMOD_LSHIFT) == HOST_MACOS_SHORTCUT_NONE);
	assert(host_macos_shortcut_for(SDL_SCANCODE_LEFTBRACKET, SDL_KMOD_NONE) == HOST_MACOS_SHORTCUT_NONE);
	/* the game's own keys: none of its movement, action or menu keys are shortcuts */
	for (int scancode = SDL_SCANCODE_A; scancode < SDL_SCANCODE_COUNT; scancode++)
	{
		enum host_macos_shortcut shortcut = host_macos_shortcut_for((SDL_Scancode)scancode, SDL_KMOD_NONE);

		assert(shortcut == HOST_MACOS_SHORTCUT_NONE);
	}

	/* F1, F11 and F12 are gone, with any modifier, on press, repeat and release */
	SDL_Scancode removed[] = { SDL_SCANCODE_F1, SDL_SCANCODE_F11, SDL_SCANCODE_F12 };
	for (int index = 0; index < 3; index++)
	{
		event = key(removed[index], SDL_KMOD_NONE, 1, 0);
		assert(host_macos_key_swallow(&state, &event));
		event = key(removed[index], SDL_KMOD_LGUI, 1, 1);
		assert(host_macos_key_swallow(&state, &event));
		event = key(removed[index], SDL_KMOD_NONE, 0, 0);
		assert(host_macos_key_swallow(&state, &event));
	}
	/* F8 and F9 are not the host's to take: the game reads them as ordinary keys
	(F9's late look runs only in development builds) */
	event = key(SDL_SCANCODE_F8, SDL_KMOD_NONE, 1, 0);
	assert(!host_macos_key_swallow(&state, &event));
	event = key(SDL_SCANCODE_F9, SDL_KMOD_NONE, 1, 0);
	assert(!host_macos_key_swallow(&state, &event));

	/* a chord's press, its repeats and its release (after Command is let go) never reach the game */
	event = key(SDL_SCANCODE_F, ctrl_cmd, 1, 0);
	assert(host_macos_key_swallow(&state, &event));
	event = key(SDL_SCANCODE_F, ctrl_cmd, 1, 1);
	assert(host_macos_key_swallow(&state, &event));
	event = key(SDL_SCANCODE_F, SDL_KMOD_NONE, 0, 0);
	assert(host_macos_key_swallow(&state, &event));
	/* and afterwards F is an ordinary key again */
	event = key(SDL_SCANCODE_F, SDL_KMOD_NONE, 1, 0);
	assert(!host_macos_key_swallow(&state, &event));
	event = key(SDL_SCANCODE_F, SDL_KMOD_NONE, 0, 0);
	assert(!host_macos_key_swallow(&state, &event));
	/* the modifier keys themselves pass */
	event = key(SDL_SCANCODE_LGUI, SDL_KMOD_LGUI, 1, 0);
	assert(!host_macos_key_swallow(&state, &event));
	puts("Mac shortcut mapping and key removal passed");
	return 0;
}
