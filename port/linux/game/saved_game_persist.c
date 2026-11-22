/* port: see saved_game_persist.h */
#include "cseries.h"
#include "saved_game_persist.h"
#include "main/main.h"
#include "interface/player_ui.h"
#include "network_coop.h"
#include "../src/input_replay.h"
#include "../src/port_config.h"

/* platform.h's, and the host's (port/runtime/guest/runtime/guest_host.h) */
void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);
void host_save_checkpoint_begin(void);
void host_save_checkpoint_end(const char *checkpoint_path, const char *target_path, unsigned int size);
int host_save_write_now(const char *checkpoint_path, const char *target_path, const void *image, unsigned int size);

enum { PERSIST_PATH_SIZE = 1024 };

boolean saved_game_persist_allowed(
	void)
{
	return main_get_current_solo_level() != NONE &&
		player_spawn_count == 1 &&
		player_ui_get_active_player_profile_index(0) != NONE &&
		!network_coop_active() &&
		!input_replay_playing();
}

/* the checkpoint file's and the active profile's saved game's host paths */
static boolean persist_paths(
	char *checkpoint_path,
	char *target_path)
{
	char profile_directory[256];
	char xbox_path[256 + 16];

	if (!player_ui_get_path_to_local_player_profile_directory(0, profile_directory))
		return FALSE;
	if (strlen(profile_directory) + sizeof("savegame.bin") > sizeof(xbox_path))
		return FALSE;
	strcpy(xbox_path, profile_directory);
	strcat(xbox_path, "savegame.bin");
	platform_translate_path(xbox_path, target_path, PERSIST_PATH_SIZE);
	platform_translate_path("z:\\savegame.bin", checkpoint_path, PERSIST_PATH_SIZE);
	return target_path[0] && checkpoint_path[0];
}

void saved_game_persist_checkpoint_begin(
	void)
{
	host_save_checkpoint_begin();
}

void saved_game_persist_checkpoint_end(
	boolean written,
	long size)
{
	char checkpoint_path[PERSIST_PATH_SIZE] = "";
	char target_path[PERSIST_PATH_SIZE];
	boolean persist = written &&
		config_boolean("saves.checkpoint_persist") &&
		saved_game_persist_allowed() &&
		persist_paths(checkpoint_path, target_path);

	/* (a checkpoint that is not to be saved still ends the host's count of the write) */
	host_save_checkpoint_end(checkpoint_path, persist ? target_path : NULL, (unsigned int)size);
}

boolean saved_game_persist_write_now(
	const void *image,
	long size)
{
	char checkpoint_path[PERSIST_PATH_SIZE];
	char target_path[PERSIST_PATH_SIZE];

	return persist_paths(checkpoint_path, target_path) &&
		host_save_write_now(checkpoint_path, target_path, image, (unsigned int)size) != 0;
}
