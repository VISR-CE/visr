/* port: the campaign's saved game (a profile's savegame.bin) is written at every checkpoint, where the
Xbox game wrote it only at Save and Quit: a phone, a headset or an Apple TV ends an app without warning.
The host writes the file (port/runtime/save_writer.h): whole, on a thread of its own at a checkpoint,
from the copy of the game state it keeps as the session's checkpoint. */
#ifndef __SAVED_GAME_PERSIST_H
#define __SAVED_GAME_PERSIST_H

/* Whether this game has a saved game to write at all: the campaign, one player, with a profile, not in a
network game and not a replay (which plays under the device's own profile and must leave its saved game
alone). Save and Quit and the checkpoint ask the same question. */
boolean saved_game_persist_allowed(void);

/* The Apple builds' checkpoint (game_state_write_to_file and game_state_read_from_file call these where the
desktop builds use the file z:\savegame.bin). store: the host keeps a copy of the image as the session's
checkpoint and, when saved_game_persist_allowed and saves.checkpoint_persist say so, writes it to the profile's
saved game on its own thread; the game's thread touches no file. FALSE when the copy could not be kept. load: that
copy, into image, for a revert; FALSE when there is none. Both are FALSE on a build without a host. */
boolean saved_game_persist_checkpoint_store(const void *image, long size);
boolean saved_game_persist_checkpoint_load(void *image, long size);

/* Save and Quit: the image (the game state, as reverted to its checkpoint) becomes the saved game before
this returns TRUE; FALSE, and the saved game is the one it was. */
boolean saved_game_persist_write_now(const void *image, long size);

#endif
