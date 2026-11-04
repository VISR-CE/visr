/* Each game engine's picture in the gametype menus is its own frame of the
game_type_grafix bitmap (menu_gametype.h). */
#include "../../linux/include/menu_gametype.h"
#include <assert.h>

int main(void)
{
	/* engines: none 0, CTF 1, slayer 2, oddball 3, king 4, race 5;
	frames: CTF 0, king 1, slayer 2, oddball 3, race 4, question mark 5 */
	assert(menu_gametype_picture_frame(1) == 0);
	assert(menu_gametype_picture_frame(2) == 2);
	assert(menu_gametype_picture_frame(3) == 3);
	assert(menu_gametype_picture_frame(4) == 1);
	assert(menu_gametype_picture_frame(5) == 4);
	/* no engine, and numbers out of range, show the question mark */
	assert(menu_gametype_picture_frame(0) == 5);
	assert(menu_gametype_picture_frame(-1) == 5);
	assert(menu_gametype_picture_frame(6) == 5);
	return 0;
}
