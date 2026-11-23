/* The saved game's writer in the host: see host_save_writer.h and port/runtime/save_writer.h. */
#include "host_save_writer.h"
#include "host.h"
#include "save_writer.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

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

	if (name && !strcmp(name + 1, SAVED_GAME_FILE))
		save_writer_withdraw(path);
}

void host_save_withdraw_folder(const char *folder_path)
{
	if (folder_path)
		save_writer_withdraw_folder(folder_path);
}

void host_save_writer_wait(double seconds)
{
	if (!save_writer_wait(seconds))
		host_logf(HOST_LOG_INFO, "saved game: a checkpoint was still being written after %.0f s; the saved game stays the one before it", seconds);
}

int host_save_checkpoint_store(const void *image, unsigned int size, const char *session_path, const char *target_path)
{
	static int told;
	struct timespec began, ended;
	int stored;

	if (!image)
		return 0;
	clock_gettime(CLOCK_MONOTONIC, &began);
	stored = save_writer_checkpoint_store(image, size, session_path, target_path);
	clock_gettime(CLOCK_MONOTONIC, &ended);
	/* (the game's thread only: once a run, so a log says which kind of game it was) */
	if (!target_path && !told)
	{
		told = 1;
		host_logf(HOST_LOG_INFO, "saved game: this game's checkpoints are not written to a saved game (no profile, co-op, "
			"a network game, a replay or scripted run, or saves.checkpoint_persist off)");
	}
	/* (a line a checkpoint: a log then shows where each fell among the frames, kept or not, and what it cost
	   the game's thread) */
	if (stored)
		host_logf(HOST_LOG_INFO, "saved game: checkpoint, %s; the game's thread kept it in %.1f ms",
			target_path ? "to be the saved game" : "not kept",
			(double)(ended.tv_sec - began.tv_sec) * 1e3 + (double)(ended.tv_nsec - began.tv_nsec) / 1e6);
	else
		host_logf(HOST_LOG_WARN, "saved game: no memory to keep a checkpoint of %u bytes; there is none to revert to", size);
	return stored;
}

int host_save_checkpoint_load(void *image, unsigned int size)
{
	return image ? save_writer_checkpoint_load(image, size) : 0;
}

int host_save_write_now(const char *session_path, const char *target_path, const void *image, unsigned int size)
{
	if (!session_path || !target_path || !image)
		return 0;
	return save_writer_write_now(session_path, target_path, image, size);
}
