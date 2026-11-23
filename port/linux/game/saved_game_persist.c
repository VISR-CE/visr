/* port: see saved_game_persist.h */
#include "cseries.h"
#include "saved_game_persist.h"
#include "math/real_math.h"
#include "main/main.h"
#include "interface/player_ui.h"
#include "network_coop.h"
#include "../src/input_replay.h"
#include "../src/port_config.h"

/* The Apple builds have the host's writer; the desktop builds have no host, so there a checkpoint is not
kept and Save and Quit writes the file as the game always did (game_state.c). */
#ifdef HALO_IOS
/* platform.h's, and the host's (port/runtime/guest/runtime/guest_host.h) */
void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);
int host_save_checkpoint_store(const void *image, unsigned int size, const char *session_path, const char *target_path);
int host_save_checkpoint_load(void *image, unsigned int size);
int host_save_write_now(const char *session_path, const char *target_path, const void *image, unsigned int size);

enum { PERSIST_PATH_SIZE = 1024 };
#endif

boolean saved_game_persist_allowed(
	void)
{
	return main_get_current_solo_level() != NONE &&
		player_spawn_count == 1 &&
		player_ui_get_active_player_profile_index(0) != NONE &&
		!network_coop_active() &&
		/* a replay, a benchmark, a test or a runner passes checkpoints too, and is not the player's game */
		(config_boolean("debug.scripted_run_saves") || (!input_replay_playing() && !config_scripted_run()));
}

#ifdef HALO_IOS
/* A path in the session folder (where the writer's temporary file goes; no file has this name on these builds)
   and the active profile's saved game's host path. A translation walks folders on the disk, and this runs on
   the game's thread at every checkpoint, so both are kept: the session's for the run, the saved game's until
   another profile's folder is asked for. */
static boolean persist_paths(
	char *session_path,
	char *target_path)
{
	static char kept_directory[256], kept_target[PERSIST_PATH_SIZE], kept_session[PERSIST_PATH_SIZE];
	char profile_directory[256];
	char xbox_path[256 + 16];

	if (!player_ui_get_path_to_local_player_profile_directory(0, profile_directory))
		return FALSE;
	if (strlen(profile_directory) + sizeof("savegame.bin") > sizeof(xbox_path))
		return FALSE;
	if (!kept_target[0] || strcmp(kept_directory, profile_directory))
	{
		strcpy(xbox_path, profile_directory);
		strcat(xbox_path, "savegame.bin");
		platform_translate_path(xbox_path, kept_target, PERSIST_PATH_SIZE);
		strcpy(kept_directory, profile_directory);
	}
	if (!kept_session[0])
		platform_translate_path("z:\\savegame.bin", kept_session, PERSIST_PATH_SIZE);
	strcpy(target_path, kept_target);
	strcpy(session_path, kept_session);
	return target_path[0] && session_path[0];
}

#endif

boolean saved_game_persist_checkpoint_store(
	const void *image,
	long size)
{
#ifdef HALO_IOS
	char session_path[PERSIST_PATH_SIZE] = "";
	char target_path[PERSIST_PATH_SIZE];
	boolean persist = config_boolean("saves.checkpoint_persist") &&
		saved_game_persist_allowed() &&
		persist_paths(session_path, target_path);

	return host_save_checkpoint_store(image, (unsigned int)size, session_path, persist ? target_path : NULL) != 0;
#else
	(void)image;
	(void)size;
	return FALSE;
#endif
}

boolean saved_game_persist_checkpoint_load(
	void *image,
	long size)
{
#ifdef HALO_IOS
	return host_save_checkpoint_load(image, (unsigned int)size) != 0;
#else
	(void)image;
	(void)size;
	return FALSE;
#endif
}

boolean saved_game_persist_write_now(
	const void *image,
	long size)
{
#ifdef HALO_IOS
	char session_path[PERSIST_PATH_SIZE];
	char target_path[PERSIST_PATH_SIZE];

	return persist_paths(session_path, target_path) &&
		host_save_write_now(session_path, target_path, image, (unsigned int)size) != 0;
#else
	(void)image;
	(void)size;
	return FALSE;
#endif
}
