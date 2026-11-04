#pragma once
#include <SDL3/SDL.h>
/* The native Mac app's keyboard shortcuts. The menu bar items own the actions
   (host_macos_menu.m); this decides which keystrokes the game must not also
   receive. The function keys that were the Mac app's shortcuts (F1 Back, F11 full
   screen, F12 mouse release) are removed: a Mac keyboard's top row is brightness
   and volume, and the chords below replace them. F8 and F9 are development
   toggles that exist only in development builds. */
enum host_macos_shortcut
{
	HOST_MACOS_SHORTCUT_NONE,
	HOST_MACOS_SHORTCUT_FULL_SCREEN,	/* Control-Command-F */
	HOST_MACOS_SHORTCUT_MOUSE,			/* Control-Command-M */
	HOST_MACOS_SHORTCUT_BACK,			/* Command-[ */
};

/* the shortcut a key press makes, with exactly its modifiers held (Shift, and
Option with Command, belong to other commands) */
static inline enum host_macos_shortcut host_macos_shortcut_for(SDL_Scancode scancode, SDL_Keymod mod)
{
	int command = (mod & SDL_KMOD_GUI) != 0, control = (mod & SDL_KMOD_CTRL) != 0;

	if (!command || (mod & (SDL_KMOD_ALT | SDL_KMOD_SHIFT)))
		return HOST_MACOS_SHORTCUT_NONE;
	if (control)
		return scancode == SDL_SCANCODE_F ? HOST_MACOS_SHORTCUT_FULL_SCREEN :
			scancode == SDL_SCANCODE_M ? HOST_MACOS_SHORTCUT_MOUSE : HOST_MACOS_SHORTCUT_NONE;
	return scancode == SDL_SCANCODE_LEFTBRACKET ? HOST_MACOS_SHORTCUT_BACK : HOST_MACOS_SHORTCUT_NONE;
}

/* function keys the Mac app no longer reads at all */
static inline int host_macos_key_removed(SDL_Scancode scancode)
{
	return scancode == SDL_SCANCODE_F1 || scancode == SDL_SCANCODE_F11 || scancode == SDL_SCANCODE_F12;
}

/* the keys of shortcuts in progress, so their repeats and releases are held
back as well */
struct host_macos_key_state
{
	unsigned char held[SDL_SCANCODE_COUNT];
};

/* nonzero if the game must not see this key event */
static inline int host_macos_key_swallow(struct host_macos_key_state *state, const SDL_KeyboardEvent *key)
{
	if (key->scancode <= SDL_SCANCODE_UNKNOWN || key->scancode >= SDL_SCANCODE_COUNT)
		return 0;
	if (host_macos_key_removed(key->scancode))
		return 1;
	if (!key->down || key->repeat)
	{
		int held = state->held[key->scancode];

		if (!key->down)
			state->held[key->scancode] = 0;
		return held;
	}
	if (host_macos_shortcut_for(key->scancode, key->mod) != HOST_MACOS_SHORTCUT_NONE)
		state->held[key->scancode] = 1;
	return state->held[key->scancode];
}
