/* Controller prompt identities shared by the guest and native raster service. */
#ifndef HALO_CONTROLLER_GLYPHS_H
#define HALO_CONTROLLER_GLYPHS_H

enum { HALO_GLYPH_ORIGINAL, HALO_GLYPH_HIDDEN, HALO_GLYPH_SYMBOL, HALO_GLYPH_KEYCAP };

/* One table drives the menu key presses and the labels on their keycaps. */
#define HALO_GLYPH_MENU_BINDINGS(X) \
	X(0, SDL_SCANCODE_RETURN, "Enter") \
	X(1, SDL_SCANCODE_ESCAPE, "Escape") \
	X(2, SDL_SCANCODE_E, "E") \
	X(3, SDL_SCANCODE_TAB, "Tab")

/* The left stick click is the wildcard fifth action; its menu key is R. */
#define HALO_GLYPH_MENU_WILDCARD_SCANCODE SDL_SCANCODE_R

static inline const char *halo_glyph_menu_key(int physical)
{
#define MENU_LABEL(button, scancode, label) case button: return label;
	switch (physical)
	{
		HALO_GLYPH_MENU_BINDINGS(MENU_LABEL)
		case 8: return "Up";
		case 9: return "Down";
		case 10: return "Left";
		case 11: return "Right";
		case 12: return "Escape";
		case 13: return "F1";
		case 14: return "R";
		case 16: return "WASD";
		case 17: return "Mouse";
		default: return "";
	}
#undef MENU_LABEL
}

enum { HALO_GLYPH_INPUT_TOUCH = -2, HALO_GLYPH_INPUT_CONTROLLER = 0, HALO_GLYPH_INPUT_KEYBOARD = 1,
	HALO_GLYPH_INPUT_MOUSE = 2 };
struct halo_glyph_input { int source; unsigned int gamepad; };

/* Events, rather than held state, select the device. Idle and key-repeat
samples cannot steal the prompts from a more recently used device. */
static inline struct halo_glyph_input halo_glyph_input_note(
	struct halo_glyph_input previous, int source, unsigned int gamepad, int active)
{
	if (active)
	{
		previous.source = source;
		previous.gamepad = source == HALO_GLYPH_INPUT_CONTROLLER ? gamepad : 0;
	}
	return previous;
}

/* The mouse is its own source, separate from the keyboard: a click always claims
the prompts, but motion must travel this far (menu units) so a bumped desk does not. */
#define HALO_GLYPH_MOUSE_MOTION_THRESHOLD 2.0f

static inline int halo_glyph_mouse_motion_active(float dx, float dy)
{
	return dx * dx + dy * dy >= HALO_GLYPH_MOUSE_MOTION_THRESHOLD * HALO_GLYPH_MOUSE_MOTION_THRESHOLD;
}

union SDL_Event;
void halo_controller_glyph_input_event(union SDL_Event const *event);

/* Icon indices are HUD identities, not frames in the maps' sprite atlases. */
static inline int halo_glyph_keyboard_action(int semantic)
{
	static const signed char actions[] = {13, 7, 6, 14, 4, 8, 11, 12, 5, 10};
	return semantic >= 18 && semantic < 28 ? actions[semantic - 18] : -1;
}

static inline int halo_glyph_is_axis(int physical)
{
	return physical == 6 || physical == 7 || physical == 16 || physical == 17;
}

/* SDL3 button and axis enum values: White/Black are modern shoulders. */
static inline int halo_glyph_element(int physical)
{
	static const signed char elements[] = {0, 1, 2, 3, 10, 9, 4, 5, 11, 12, 13, 14, 6, 4, 7, 8, 0, 2};
	return physical >= 0 && physical < 18 ? elements[physical] : -1;
}

/* The input the player 0 menus' prompts follow: 0 keyboard and mouse, 1 controller, 2 touch. The order is that of the
{keyboard|controller|touch} wordings in help text, which ui_widget.c indexes by it. */
enum { HALO_INPUT_KEYBOARD, HALO_INPUT_CONTROLLER, HALO_INPUT_TOUCH };
int halo_menu_input_kind(void);
/* 0 keyboard/controller, 1 mouse, 2 finger or pinch. */
int halo_menu_pointer_controls(void);
struct halo_ui_pointer;
void halo_menu_pointer_activity(struct halo_ui_pointer const *pointer);
int halo_controller_glyph_request(short player, short semantic, short physical, char *label, unsigned int size);
struct host_ui_pointer;
/* the screen's secondary action (X) is destructive: the keyboard legend reads Delete */
void halo_controller_glyph_x_destructive(int destructive);
/* the X legend being drawn is a screen-wide action, not the focused item's secondary: the mouse legend reads
the keyboard key, never the right button (right-click is the secondary action only) */
void halo_controller_glyph_x_global(int global);
void halo_controller_glyph_pointer_activity(int menus, struct host_ui_pointer const *pointer);
#endif
