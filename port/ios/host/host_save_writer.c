/* The saved game's writer in the host: see host_save_writer.h and port/runtime/save_writer.h. */
#include "host_save_writer.h"
#include "host.h"
#include "save_writer.h"

#include <stdio.h>
#include <string.h>

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
			"a network game, a replay, or saves.checkpoint_persist off)");
	}
	save_writer_checkpoint_end(checkpoint_path ? checkpoint_path : "", target_path, size);
}

int host_save_write_now(const char *checkpoint_path, const char *target_path, const void *image, unsigned int size)
{
	if (!checkpoint_path || !target_path || !image)
		return 0;
	return save_writer_write_now(checkpoint_path, target_path, image, size);
}
