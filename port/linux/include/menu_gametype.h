/* A gametype's picture in the menus. */
#ifndef MENU_GAMETYPE_H
#define MENU_GAMETYPE_H

/* The frame of the game_type_grafix bitmap that a game engine's picture is.
The bitmap's frames are the Xbox's multiplayer_game_bitmap_frame: CTF, king,
slayer, oddball, race, then the question mark. The engines (game_engine_type)
number none, CTF, slayer, oddball, king and race, so the engine's number is
not the frame. */
static inline short menu_gametype_picture_frame(long engine)
{
	static short const frames[] = { 5, 0, 2, 3, 1, 4 };

	return frames[engine < 0 || engine > 5 ? 0 : engine];
}

#endif
