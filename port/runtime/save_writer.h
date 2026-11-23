/* The one writer of a profile's saved game (<save root>/u/UDATA/<slot>/savegame.bin).

   A saved game is replaced whole: the new image goes to a temporary file in the session folder (the folder
   <save root>/z: the same volume, and outside every slot, so nothing that reads a
   slot ever sees it), is synced to the disk, renamed over the saved game, and the slot's folder is synced.
   Whenever the process or the power ends, the saved game is the old one or the new one.

   Two ways in. At a checkpoint the game's thread copies the game state into a buffer kept here (the session's
   checkpoint, which a revert copies back), and a worker thread writes that buffer out, with the checksum a
   saved game carries: the game's thread touches no file. Save and Quit writes the game's own buffer before it
   returns.

   All I/O goes through a table of functions, so a test can record the order of the calls and fail any. */
#ifndef SAVE_WRITER_H
#define SAVE_WRITER_H

#include <stddef.h>
#include <stdint.h>

/* a profile's saved game, in its slot's folder */
#define SAVED_GAME_FILE "savegame.bin"

struct save_writer_io
{
	void *context;
	/* a file to read (create 0) or a new, empty file to write (create 1); a descriptor, or -1 */
	int (*open)(void *context, const char *path, int create);
	/* a folder, to sync; a descriptor, or -1 */
	int (*open_folder)(void *context, const char *path);
	/* all size bytes or fewer at the end of the file; -1 on error */
	long long (*read_at)(void *context, int file, void *data, size_t size, long long offset);
	/* 0 when all size bytes were written */
	int (*write_at)(void *context, int file, const void *data, size_t size, long long offset);
	/* the file's bytes are on the disk itself (F_FULLFSYNC where there is one); 0 on success */
	int (*full_sync)(void *context, int file);
	/* a folder's entry is on its way to the disk (fsync); 0 on success */
	int (*sync_folder)(void *context, int folder);
	int (*close)(void *context, int file);
	int (*rename)(void *context, const char *from, const char *to);
	int (*remove)(void *context, const char *path);
	/* the size of the file at path, -1 when there is none, -2 when it cannot be told */
	long long (*size)(void *context, const char *path);
};

extern const struct save_writer_io save_writer_posix_io;

/* The temporary file's name for a saved game at target_path, in the folder of session_path (any path in the session folder). 0 when the
   paths cannot be used. */
int save_writer_temporary_path(const char *session_path, const char *target_path, char *path, size_t path_size);
/* the prefix and suffix of every temporary file, for the cleanup at launch */
#define SAVE_WRITER_TEMPORARY_PREFIX "persist-"
#define SAVE_WRITER_TEMPORARY_SUFFIX ".new"

/* Any file under the save root (a profile, a game type, a saved game another device made): the same
   sequence, with no rule about what the file holds. The temporary file is in the caller's care: it must be
   a name on the target's volume that nothing reads. */
int save_writer_replace_with_data(const struct save_writer_io *io, const char *temporary_path, const char *target_path,
	const uint8_t *data, size_t size);

/* Replaces target_path with the image, whose checksum field is computed here (the image is not changed).
   current, when given, is asked before the rename: 0 abandons the write. Returns SAVE_WRITER_REPLACED,
   SAVE_WRITER_ABANDONED, SAVE_WRITER_FAILED (an I/O error) or SAVE_WRITER_REFUSED (a saved game of another
   size is another build's and is left alone, as the game's own check of a profile leaves it). Nothing but the
   temporary file is touched unless the saved game was replaced. */
enum { SAVE_WRITER_REFUSED = -2, SAVE_WRITER_FAILED = -1, SAVE_WRITER_ABANDONED = 0, SAVE_WRITER_REPLACED = 1 };
int save_writer_replace_with_image(const struct save_writer_io *io, const char *temporary_path, const char *target_path,
	const uint8_t *image, size_t size, int (*current)(void *context), void *context);
/* The same from a source read a piece at a time, in order, each piece once (the worker's: the kept checkpoint).
   A reader returns SAVE_WRITER_PIECE_READ, SAVE_WRITER_PIECE_FAILED, or SAVE_WRITER_PIECE_GONE when the source
   is no longer the image the write was asked for, which abandons the write. current is also asked after the
   last piece, so a source that changed while it was read is never used. */
enum { SAVE_WRITER_PIECE_GONE = -1, SAVE_WRITER_PIECE_FAILED = 0, SAVE_WRITER_PIECE_READ = 1 };
typedef int save_writer_piece_reader(void *read_context, uint8_t *piece, size_t count, size_t offset);
int save_writer_replace_with_pieces(const struct save_writer_io *io, save_writer_piece_reader *read_piece, void *read_context,
	const char *temporary_path, const char *target_path, size_t size, int (*current)(void *context), void *context);

/* ---- the worker: one per process */

/* io NULL is save_writer_posix_io (as before any call of this); log may be NULL */
void save_writer_start(const struct save_writer_io *io, void (*log)(int warning, const char *line));
/* removes temporary files a run that ended early left in the session folder, and the checkpoint file
   (savegame.bin, the size of a saved game) that builds before the kept checkpoint wrote there */
void save_writer_clean(const struct save_writer_io *io, const char *session_folder);
/* A checkpoint, on the game's thread: the image becomes the session's checkpoint (a copy; no file is touched),
   and with a target_path also the profile's saved game, written by the worker (NULL: it stays a checkpoint
   only). session_path is any path in the session folder, where the temporary file goes. A checkpoint being
   written when the next one comes is abandoned without a wait. 0 when there is no memory for the copy: there
   is then no checkpoint to revert to. */
int save_writer_checkpoint_store(const uint8_t *image, size_t size, const char *session_path, const char *target_path);
/* A revert, on the game's thread: the session's checkpoint, copied into image. 0 when there is none of this size. */
int save_writer_checkpoint_load(uint8_t *image, size_t size);
/* Save and Quit, on the game's thread: withdraws a queued checkpoint, waits for one being written, and
   replaces the saved game with the image before it returns. 1 on success. */
int save_writer_write_now(const char *session_path, const char *target_path, const uint8_t *image, size_t size);
/* the saved game at target_path is being deleted: withdraws and waits for a write to it */
void save_writer_withdraw(const char *target_path);
/* The profile whose folder this is is about to be deleted, before the game lists the folder's files:
   withdraws and waits for a write to its saved game, so nothing lands in the folder behind the listing. */
void save_writer_withdraw_folder(const char *folder_path);
/* waits until nothing is queued or being written, at most seconds; 1 when idle */
int save_writer_wait(double seconds);
/* saved games replaced by the worker since the start (for tests and the log) */
unsigned long save_writer_replaced_count(void);

#endif
