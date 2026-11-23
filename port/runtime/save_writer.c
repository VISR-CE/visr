/* The one writer of a profile's saved game: see save_writer.h. */
#include "save_writer.h"
#include "saved_game_image.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <pthread/qos.h>
#endif

enum { PATH_SIZE = 1024, COPY_PIECE = 1 << 20 };

/* ---- the I/O of a real file system */

static int posix_open(void *context, const char *path, int create)
{
	(void)context;
	return create ? open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644) : open(path, O_RDONLY | O_CLOEXEC);
}

static int posix_open_folder(void *context, const char *path)
{
	(void)context;
	return open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
}

static long long posix_read_at(void *context, int file, void *data, size_t size, long long offset)
{
	size_t done = 0;

	(void)context;
	while (done < size)
	{
		ssize_t count = pread(file, (char *)data + done, size - done, (off_t)(offset + (long long)done));

		if (count < 0 && errno == EINTR)
			continue;
		if (count < 0)
			return -1;
		if (!count)
			break;
		done += (size_t)count;
	}
	return (long long)done;
}

static int posix_write_at(void *context, int file, const void *data, size_t size, long long offset)
{
	size_t done = 0;

	(void)context;
	while (done < size)
	{
		ssize_t count = pwrite(file, (const char *)data + done, size - done, (off_t)(offset + (long long)done));

		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0)
			return -1;
		done += (size_t)count;
	}
	return 0;
}

static int posix_full_sync(void *context, int file)
{
	(void)context;
#ifdef F_FULLFSYNC
	/* fsync on Apple's systems leaves the bytes in the drive's own cache; a file system that has no
	   full sync (a network volume) refuses, and gets the ordinary one */
	if (fcntl(file, F_FULLFSYNC) == 0)
		return 0;
#endif
	return fsync(file);
}

static int posix_sync_folder(void *context, int folder)
{
	(void)context;
	return fsync(folder);
}

static int posix_close(void *context, int file)
{
	(void)context;
	return close(file);
}

static int posix_rename(void *context, const char *from, const char *to)
{
	(void)context;
	return rename(from, to);
}

static int posix_remove(void *context, const char *path)
{
	(void)context;
	return unlink(path);
}

static long long posix_size(void *context, const char *path)
{
	struct stat st;

	(void)context;
	if (stat(path, &st) != 0)
		return errno == ENOENT ? -1 : -2;
	return (long long)st.st_size;
}

const struct save_writer_io save_writer_posix_io = {
	NULL, posix_open, posix_open_folder, posix_read_at, posix_write_at, posix_full_sync, posix_sync_folder,
	posix_close, posix_rename, posix_remove, posix_size,
};

/* ---- paths */

/* where the last component of path begins, and the length of what is before its separator (0: no folder) */
static const char *last_component(const char *path, size_t *folder_length)
{
	const char *slash = strrchr(path, '/');

	*folder_length = slash ? (size_t)(slash - path) : 0;
	return slash ? slash + 1 : path;
}

/* the slot's folder name and the file's name in it: "<slot>/savegame.bin" */
static int slot_and_file(const char *target_path, const char **slot, size_t *slot_length)
{
	size_t folder_length;
	const char *file = last_component(target_path, &folder_length);
	const char *start;

	if (!*file || !folder_length)
		return 0;
	start = target_path + folder_length;
	while (start > target_path && start[-1] != '/')
		start--;
	*slot = start;
	*slot_length = (size_t)(target_path + folder_length - start);
	return *slot_length != 0;
}

int save_writer_temporary_path(const char *session_path, const char *target_path, char *path, size_t path_size)
{
	size_t folder_length, slot_length;
	const char *slot;
	int length;

	if (!session_path || !target_path || !slot_and_file(target_path, &slot, &slot_length))
		return 0;
	(void)last_component(session_path, &folder_length);
	if (!folder_length)
		return 0;
	length = snprintf(path, path_size, "%.*s/" SAVE_WRITER_TEMPORARY_PREFIX "%.*s" SAVE_WRITER_TEMPORARY_SUFFIX,
		(int)folder_length, session_path, (int)slot_length, slot);
	return length > 0 && (size_t)length < path_size;
}

static int same_saved_game(const char *a, const char *b)
{
	const char *slot_a, *slot_b;
	size_t length_a, length_b;

	if (!slot_and_file(a, &slot_a, &length_a) || !slot_and_file(b, &slot_b, &length_b))
		return 0;
	/* the slot's name and what follows it */
	return length_a == length_b && !strcmp(slot_a, slot_b);
}

/* ---- one replacement */

static void put_u32(uint8_t *bytes, uint32_t value)
{
	bytes[0] = (uint8_t)value; bytes[1] = (uint8_t)(value >> 8); bytes[2] = (uint8_t)(value >> 16); bytes[3] = (uint8_t)(value >> 24);
}

/* whether a saved game of this size may take target_path's place: SAVE_WRITER_REPLACED when it may */
static int target_takes(const struct save_writer_io *io, const char *target_path, size_t size)
{
	long long existing = io->size(io->context, target_path);

	if (existing == -1 || existing == (long long)size)
		return SAVE_WRITER_REPLACED;
	return existing < 0 ? SAVE_WRITER_FAILED : SAVE_WRITER_REFUSED;
}

/* the temporary file is written: to the disk, into place, and the slot's folder after it */
static int finish(const struct save_writer_io *io, int file, const char *temporary_path, const char *target_path,
	int (*current)(void *context), void *context)
{
	size_t folder_length;
	char folder_path[PATH_SIZE];
	int synced = io->full_sync(io->context, file) == 0;
	int closed = io->close(io->context, file) == 0;
	int folder;

	if (!synced || !closed)
	{
		io->remove(io->context, temporary_path);
		return SAVE_WRITER_FAILED;
	}
	if (current && !current(context))
	{
		io->remove(io->context, temporary_path);
		return SAVE_WRITER_ABANDONED;
	}
	if (io->rename(io->context, temporary_path, target_path) != 0)
	{
		io->remove(io->context, temporary_path);
		return SAVE_WRITER_FAILED;
	}
	/* the saved game is the new one from here on; the folder's sync only makes that outlast a power cut */
	(void)last_component(target_path, &folder_length);
	if (folder_length && folder_length < sizeof(folder_path))
	{
		memcpy(folder_path, target_path, folder_length);
		folder_path[folder_length] = 0;
		folder = io->open_folder(io->context, folder_path);
		if (folder >= 0)
		{
			io->sync_folder(io->context, folder);
			io->close(io->context, folder);
		}
	}
	return SAVE_WRITER_REPLACED;
}

int save_writer_replace_with_data(const struct save_writer_io *io, const char *temporary_path, const char *target_path,
	const uint8_t *data, size_t size)
{
	int file = io->open(io->context, temporary_path, 1);

	if (file < 0)
		return SAVE_WRITER_FAILED;
	if (size && io->write_at(io->context, file, data, size, 0) != 0)
	{
		io->close(io->context, file);
		io->remove(io->context, temporary_path);
		return SAVE_WRITER_FAILED;
	}
	return finish(io, file, temporary_path, target_path, NULL, NULL);
}

int save_writer_replace_with_image(const struct save_writer_io *io, const char *temporary_path, const char *target_path,
	const uint8_t *image, size_t size, int (*current)(void *context), void *context)
{
	uint8_t checksum[4];
	int file, takes;

	if (size < SAVED_GAME_HEADER_SIZE)
		return SAVE_WRITER_FAILED;
	takes = target_takes(io, target_path, size);
	if (takes != SAVE_WRITER_REPLACED)
		return takes;
	file = io->open(io->context, temporary_path, 1);
	if (file < 0)
		return SAVE_WRITER_FAILED;
	put_u32(checksum, saved_game_image_checksum(image, size));
	if (io->write_at(io->context, file, image, SAVED_GAME_HEADER_CHECKSUM, 0) != 0 ||
		io->write_at(io->context, file, checksum, 4, SAVED_GAME_HEADER_CHECKSUM) != 0 ||
		io->write_at(io->context, file, image + SAVED_GAME_HEADER_CHECKSUM + 4, size - SAVED_GAME_HEADER_CHECKSUM - 4,
			SAVED_GAME_HEADER_CHECKSUM + 4) != 0)
	{
		io->close(io->context, file);
		io->remove(io->context, temporary_path);
		return SAVE_WRITER_FAILED;
	}
	return finish(io, file, temporary_path, target_path, current, context);
}

int save_writer_replace_with_pieces(const struct save_writer_io *io, save_writer_piece_reader *read_piece, void *read_context,
	const char *temporary_path, const char *target_path, size_t size, int (*current)(void *context), void *context)
{
	uint8_t checksum[4];
	uint8_t *piece;
	uint32_t crc = SAVED_GAME_CRC_START;
	size_t offset = 0;
	int file, result = SAVE_WRITER_FAILED;

	if (size < SAVED_GAME_HEADER_SIZE)
		return SAVE_WRITER_FAILED;
	result = target_takes(io, target_path, size);
	if (result != SAVE_WRITER_REPLACED)
		return result;
	result = SAVE_WRITER_FAILED;
	piece = malloc(COPY_PIECE);
	if (!piece)
		return SAVE_WRITER_FAILED;
	file = io->open(io->context, temporary_path, 1);
	if (file < 0)
	{
		free(piece);
		return SAVE_WRITER_FAILED;
	}
	while (offset < size)
	{
		size_t count = size - offset < COPY_PIECE ? size - offset : COPY_PIECE;
		int read = read_piece(read_context, piece, count, offset);

		if (read != SAVE_WRITER_PIECE_READ)
		{
			/* (a source that is another checkpoint's by now is no failure of the disk) */
			if (read == SAVE_WRITER_PIECE_GONE)
				result = SAVE_WRITER_ABANDONED;
			break;
		}
		if (io->write_at(io->context, file, piece, count, (long long)offset) != 0)
			break;
		crc = saved_game_crc_run(crc, piece, offset, count);
		offset += count;
		/* overtaken or withdrawn: whoever waits for this write (Save and Quit, a profile being deleted)
		   waits for one piece, not for the image */
		if (offset < size && current && !current(context))
		{
			result = SAVE_WRITER_ABANDONED;
			break;
		}
	}
	free(piece);
	if (offset == size)
	{
		if (current && !current(context))
			result = SAVE_WRITER_ABANDONED;
		else
		{
			put_u32(checksum, crc);
			if (io->write_at(io->context, file, checksum, 4, SAVED_GAME_HEADER_CHECKSUM) == 0)
				return finish(io, file, temporary_path, target_path, current, context);
		}
	}
	io->close(io->context, file);
	io->remove(io->context, temporary_path);
	return result;
}

/* ---- the worker */

struct request
{
	int valid;
	unsigned generation;
	size_t size;
	char target[PATH_SIZE], temporary[PATH_SIZE];
};

static struct
{
	pthread_mutex_t lock;
	pthread_cond_t changed;
	const struct save_writer_io *io;
	void (*log)(int warning, const char *line);
	int thread_started, busy;
	struct request queued, active;
	unsigned long replaced;
} writer = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, &save_writer_posix_io, NULL, 0, 0, { 0 }, { 0 }, 0 };

/* (the log is never called with the lock held: the game's thread takes the lock at every checkpoint) */
static atomic_int writer_refusal_logged;

/* counts the checkpoints kept: a write is of the checkpoint its request names, or of none */
static atomic_uint writer_generation;

/* The session's checkpoint: the game state as it was at the last one. The game's thread copies into it at a
   checkpoint and out of it at a revert; the worker copies out of it a piece at a time. The lock is held around
   those copies and nothing else (never around a call to the disk), so the game's thread waits for one piece's
   copy at most. The buffer is made at the first checkpoint and kept for the run (the worker is detached); it is
   made again only for a checkpoint of another size, under the lock. It holds no path: where a checkpoint is to be
   saved is in its request alone, so a checkpoint whose write was withdrawn (its profile deleted) is never
   written later. */
static struct
{
	pthread_mutex_t lock;
	uint8_t *image;
	size_t size;
	int valid;
} kept = { PTHREAD_MUTEX_INITIALIZER, NULL, 0, 0 };
/* the write in progress is not to reach the saved game */
static atomic_int writer_withdrawn;

static void writer_log(int warning, const char *format, const char *path)
{
	char line[PATH_SIZE + 160];

	if (!writer.log)
		return;
	snprintf(line, sizeof(line), format, path);
	writer.log(warning, line);
}

static void writer_log_result(int result, const char *target)
{
	if (result == SAVE_WRITER_REPLACED)
		writer_log(0, "saved game: %s replaced", target);
	else if (result == SAVE_WRITER_FAILED)
		writer_log(1, "saved game: %s could not be replaced; the one before it stays", target);
	else if (result == SAVE_WRITER_REFUSED && !atomic_exchange(&writer_refusal_logged, 1))
	{
		writer_log(1, "saved game: %s is of another size (another build's) and is left alone", target);
	}
}

static int worker_current(void *context)
{
	const struct request *request = context;

	return atomic_load(&writer_generation) == request->generation && !atomic_load(&writer_withdrawn);
}

/* a piece of the checkpoint the request is of; none once the game has kept another */
static int worker_read_piece(void *context, uint8_t *piece, size_t count, size_t offset)
{
	const struct request *request = context;
	int read = SAVE_WRITER_PIECE_GONE;

	pthread_mutex_lock(&kept.lock);
	if (kept.valid && atomic_load(&writer_generation) == request->generation && offset + count <= kept.size)
	{
		memcpy(piece, kept.image + offset, count);
		read = SAVE_WRITER_PIECE_READ;
	}
	pthread_mutex_unlock(&kept.lock);
	return read;
}

static void *worker(void *unused)
{
	(void)unused;
#ifdef __APPLE__
	/* never ahead of the game's own threads */
	pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif
	pthread_mutex_lock(&writer.lock);
	for (;;)
	{
		int result;

		while (!writer.queued.valid)
			pthread_cond_wait(&writer.changed, &writer.lock);
		writer.active = writer.queued;
		writer.queued.valid = 0;
		writer.busy = 1;
		atomic_store(&writer_withdrawn, 0);
		pthread_mutex_unlock(&writer.lock);
		result = save_writer_replace_with_pieces(writer.io, worker_read_piece, &writer.active, writer.active.temporary,
			writer.active.target, writer.active.size, worker_current, &writer.active);
		writer_log_result(result, writer.active.target);
		pthread_mutex_lock(&writer.lock);
		writer.busy = 0;
		writer.replaced += result == SAVE_WRITER_REPLACED;
		pthread_cond_broadcast(&writer.changed);
	}
	return NULL;
}

void save_writer_start(const struct save_writer_io *io, void (*log)(int warning, const char *line))
{
	pthread_mutex_lock(&writer.lock);
	writer.io = io ? io : &save_writer_posix_io;
	writer.log = log;
	pthread_mutex_unlock(&writer.lock);
}

void save_writer_clean(const struct save_writer_io *io, const char *session_folder)
{
	DIR *folder = opendir(session_folder);
	struct dirent *entry;
	size_t prefix = strlen(SAVE_WRITER_TEMPORARY_PREFIX), suffix = strlen(SAVE_WRITER_TEMPORARY_SUFFIX);

	if (!io)
		io = &save_writer_posix_io;
	if (!folder)
		return;
	while ((entry = readdir(folder)) != NULL)
	{
		size_t length = strlen(entry->d_name);
		char path[PATH_SIZE];

		/* (and the checkpoint file of builds before the checkpoint was kept in memory: nothing reads it now) */
		if (strcmp(entry->d_name, SAVED_GAME_FILE) &&
			(length <= prefix + suffix || strncmp(entry->d_name, SAVE_WRITER_TEMPORARY_PREFIX, prefix) ||
			strcmp(entry->d_name + length - suffix, SAVE_WRITER_TEMPORARY_SUFFIX)))
			continue;
		if ((size_t)snprintf(path, sizeof(path), "%s/%s", session_folder, entry->d_name) < sizeof(path))
			io->remove(io->context, path);
	}
	closedir(folder);
}

int save_writer_checkpoint_store(const uint8_t *image, size_t size, const char *session_path, const char *target_path)
{
	struct request request = { 0 };
	unsigned generation;
	int stored;

	pthread_mutex_lock(&kept.lock);
	/* (whatever the worker is writing is of the checkpoint before: it reads no more of it) */
	generation = atomic_fetch_add(&writer_generation, 1) + 1;
	if (kept.image && kept.size != size)
	{
		free(kept.image);
		kept.image = NULL;
	}
	if (!kept.image)
		kept.image = malloc(size);
	kept.size = size;
	kept.valid = kept.image != NULL;
	if (kept.valid)
		memcpy(kept.image, image, size);
	stored = kept.valid;
	pthread_mutex_unlock(&kept.lock);

	if (stored && target_path && session_path && strlen(target_path) < PATH_SIZE &&
		save_writer_temporary_path(session_path, target_path, request.temporary, sizeof(request.temporary)))
	{
		request.valid = 1;
		request.generation = generation;
		request.size = size;
		strcpy(request.target, target_path);
	}
	pthread_mutex_lock(&writer.lock);
	/* a checkpoint that is not to be saved still takes the place of an older one that waits: that one is gone */
	writer.queued = request;
	if (request.valid)
	{
		if (!writer.thread_started)
		{
			pthread_t thread;

			if (pthread_create(&thread, NULL, worker, NULL) == 0)
			{
				pthread_detach(thread);
				writer.thread_started = 1;
			}
			else
				writer.queued.valid = 0;
		}
		pthread_cond_broadcast(&writer.changed);
	}
	pthread_mutex_unlock(&writer.lock);
	return stored;
}

int save_writer_checkpoint_load(uint8_t *image, size_t size)
{
	int loaded;

	pthread_mutex_lock(&kept.lock);
	loaded = kept.valid && kept.size == size;
	if (loaded)
		memcpy(image, kept.image, size);
	pthread_mutex_unlock(&kept.lock);
	return loaded;
}

/* with the lock held: nothing queued, and nothing being written when it returns */
static void quiesce_locked(void)
{
	writer.queued.valid = 0;
	if (writer.busy)
		atomic_store(&writer_withdrawn, 1);
	while (writer.busy)
		pthread_cond_wait(&writer.changed, &writer.lock);
}

int save_writer_write_now(const char *session_path, const char *target_path, const uint8_t *image, size_t size)
{
	char temporary[PATH_SIZE];
	const struct save_writer_io *io;
	int result;

	if (!save_writer_temporary_path(session_path, target_path, temporary, sizeof(temporary)))
		return 0;
	pthread_mutex_lock(&writer.lock);
	quiesce_locked();
	io = writer.io;
	pthread_mutex_unlock(&writer.lock);
	result = save_writer_replace_with_image(io, temporary, target_path, image, size, NULL, NULL);
	writer_log_result(result, target_path);
	return result == SAVE_WRITER_REPLACED;
}

/* under the lock */
static void withdraw_locked(const char *target_path)
{
	if (writer.queued.valid && same_saved_game(writer.queued.target, target_path))
		writer.queued.valid = 0;
	if (writer.busy && same_saved_game(writer.active.target, target_path))
	{
		atomic_store(&writer_withdrawn, 1);
		while (writer.busy)
			pthread_cond_wait(&writer.changed, &writer.lock);
	}
}

void save_writer_withdraw(const char *target_path)
{
	pthread_mutex_lock(&writer.lock);
	withdraw_locked(target_path);
	pthread_mutex_unlock(&writer.lock);
}

void save_writer_withdraw_folder(const char *folder_path)
{
	char target[PATH_SIZE];
	size_t length = folder_path ? strlen(folder_path) : 0;

	while (length && folder_path[length - 1] == '/')
		length--;
	if (!length || (size_t)snprintf(target, sizeof(target), "%.*s/" SAVED_GAME_FILE, (int)length, folder_path) >= sizeof(target))
		return;
	save_writer_withdraw(target);
}

int save_writer_wait(double seconds)
{
	struct timeval now;
	struct timespec until;
	int idle;

	gettimeofday(&now, NULL);
	until.tv_sec = now.tv_sec + (time_t)seconds;
	until.tv_nsec = now.tv_usec * 1000 + (long)((seconds - (double)(time_t)seconds) * 1e9);
	if (until.tv_nsec >= 1000000000L)
	{
		until.tv_sec++;
		until.tv_nsec -= 1000000000L;
	}
	pthread_mutex_lock(&writer.lock);
	while ((writer.queued.valid || writer.busy) &&
		pthread_cond_timedwait(&writer.changed, &writer.lock, &until) == 0)
		;
	idle = !writer.queued.valid && !writer.busy;
	pthread_mutex_unlock(&writer.lock);
	return idle;
}

unsigned long save_writer_replaced_count(void)
{
	unsigned long count;

	pthread_mutex_lock(&writer.lock);
	count = writer.replaced;
	pthread_mutex_unlock(&writer.lock);
	return count;
}
