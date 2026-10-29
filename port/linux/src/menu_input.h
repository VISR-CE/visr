/* Input activity changes the desired look; only a screen open commits it. */
#ifndef MENU_INPUT_H
#define MENU_INPUT_H
#include <stdlib.h>
#include <string.h>
struct menu_input_tracker { int initialized, pad, locked; };
static int menu_input_env(char const *name)
{
	char const *value = getenv(name);
	return value && !strcmp(value, "1");
}
static void menu_input_start(struct menu_input_tracker *state, int mobile, int locked, int controller)
{
	state->initialized = 1;
	state->locked = locked;
	state->pad = mobile || locked || controller;
}
static void menu_input_activity(struct menu_input_tracker *state, int pad, float dx, float dy, int motion)
{
	if (state->locked || (motion && dx * dx + dy * dy < 16.0f)) return;
	state->pad = pad;
}
#endif
