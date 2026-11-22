/* port: the campaign's saved game (a profile's savegame.bin) is written at every checkpoint, where the
Xbox game wrote it only at Save and Quit: a phone, a headset or an Apple TV ends an app without warning.
The host writes the file (port/runtime/save_writer.h): whole, on a thread of its own at a checkpoint. */
#ifndef __SAVED_GAME_PERSIST_H
#define __SAVED_GAME_PERSIST_H

/* Whether this game has a saved game to write at all: the campaign, one player, with a profile, not in a
network game and not a replay (which plays under the device's own profile and must leave its saved game
alone). Save and Quit and the checkpoint ask the same question. */
boolean saved_game_persist_allowed(void);

/* around game_state_save's write of the checkpoint file; written is whether that write succeeded */
void saved_game_persist_checkpoint_begin(void);
void saved_game_persist_checkpoint_end(boolean written, long size);

/* Save and Quit: the image (the game state, as reverted to its checkpoint) becomes the saved game before
this returns TRUE; FALSE, and the saved game is the one it was. */
boolean saved_game_persist_write_now(const void *image, long size);

#endif
