/*
MENU_BACKDROP_FREEZE.C (test)

debug.menu_backdrop_freeze (sdl_platform.c halo_menu_backdrop_seconds): the main
menu's backdrop is the menu scenario's world, which advances with the game's
time. With the setting on, that world is held (it advances no seconds) so two
captures of one screen differ only where the screen does; a level, which is
never held, and the setting off, which plays the backdrop, are untouched.
test_menu_backdrop_freeze.py takes the code (under_test.inc).
*/

#include "harness.h"

static int frozen;
static int config_boolean(char const *name)
{
	CHECK(!strcmp(name, "debug.menu_backdrop_freeze"), "read another setting: %s", name);
	return frozen;
}

#include "under_test.inc"

int main(int argc, char **argv)
{
	char const *case_name = argv[1];

	/* the setting on, the menu's world: held */
	CASE("held-in-menu")
	{
		frozen = 1;
		CHECK(halo_menu_backdrop_seconds(1, 0.0167f) == 0.0f, "the menu backdrop advanced");
		CHECK(halo_menu_backdrop_seconds(1, 0.0f) == 0.0f, "a zero step changed");
	}
	/* the setting on, a level: never held */
	CASE("level-plays")
	{
		frozen = 1;
		CHECK(halo_menu_backdrop_seconds(0, 0.0167f) == 0.0167f, "a level's time was held");
	}
	/* the setting off, the menu's world: plays */
	CASE("plays-by-default")
	{
		frozen = 0;
		CHECK(halo_menu_backdrop_seconds(1, 0.0167f) == 0.0167f, "the backdrop was held with the setting off");
	}
	return 0;
}
