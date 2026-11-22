/* The saved game's writer in the host: see host_save_writer.h and port/runtime/save_writer.h. */
#include "host_save_writer.h"
#include "host.h"
#include "save_writer.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void writer_log(int warning, const char *line)
{
	host_logf(warning ? HOST_LOG_WARN : HOST_LOG_INFO, "%s", line);
}

void host_save_writer_start(const char *save_root)
{
	char session[1100];

	save_writer_start(NULL, writer_log);
	if ((size_t)snprintf(session, sizeof(session), "%s/z", save_root) < sizeof(session))
		save_writer_clean(NULL, session);
}

void host_save_writer_unlinking(const char *path)
{
	const char *name = path ? strrchr(path, '/') : NULL;

	if (name && !strcmp(name + 1, "savegame.bin"))
		save_writer_withdraw(path);
}

/* (the game's thread only) the saved game a checkpoint put in the folder now being removed, if one did */
static char late_saved_game[1100];

void host_save_writer_removing_folder(const char *path)
{
	late_saved_game[0] = 0;
	if (path)
		save_writer_withdraw_folder(path, late_saved_game, sizeof(late_saved_game));
}

void host_save_writer_folder_left(int directory, const char *path, int *removed)
{
	int error = errno;

	/* a checkpoint's saved game that arrived after the game had emptied the folder goes with it */
	if (late_saved_game[0] && (error == ENOTEMPTY || error == EEXIST) && unlinkat(directory, late_saved_game, 0) == 0)
	{
		host_logf(HOST_LOG_INFO, "saved game: removed a checkpoint's saved game from a profile that was being deleted");
		*removed = unlinkat(directory, path, AT_REMOVEDIR);
		error = errno;
	}
	late_saved_game[0] = 0;
	errno = error;
}

void host_save_writer_wait(double seconds)
{
	if (!save_writer_wait(seconds))
		host_logf(HOST_LOG_INFO, "saved game: a checkpoint was still being written after %.0f s; the saved game stays the one before it", seconds);
}

void host_save_checkpoint_begin(void)
{
	save_writer_checkpoint_begin();
}

void host_save_checkpoint_end(const char *checkpoint_path, const char *target_path, unsigned int size)
{
	static int told;

	/* (the game's thread only: once a run, so a log says which kind of game it was) */
	if (!target_path && !told)
	{
		told = 1;
		host_logf(HOST_LOG_INFO, "saved game: this game's checkpoints are not written to a saved game (no profile, co-op, "
			"a network game, a replay or scripted run, or saves.checkpoint_persist off)");
	}
	save_writer_checkpoint_end(checkpoint_path ? checkpoint_path : "", target_path, size);
}

int host_save_write_now(const char *checkpoint_path, const char *target_path, const void *image, unsigned int size)
{
	if (!checkpoint_path || !target_path || !image)
		return 0;
	return save_writer_write_now(checkpoint_path, target_path, image, size);
}
