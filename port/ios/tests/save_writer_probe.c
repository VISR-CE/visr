/* The saved game writer (port/runtime/save_writer.c) on a real temporary folder, through an I/O table that
   records every call and can fail any of them or end the "process" at it.

   The classes of bug this is for:
   - a file its readers depend on seen half-written, or not on the disk: the order of the calls is asserted
     (write, full sync, rename, folder sync), and after a failure or a death at every call the saved game is
     the old one or the new one, whole;
   - a writer leaving files where another reader looks: the slot's folder holds the same names at every call;
   - a copy of a file that changed under it: a checkpoint overtaken by the next one, a deleted profile and
     Save and Quit each stop the worker's write. */
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
#include <unistd.h>
#include "test_tmpdir.h"

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "%s:%d: %s failed in %s\n", __FILE__, __LINE__, #condition, test_name); exit(1); } } while (0)

enum { IMAGE_SIZE = (3 << 20) + 4096 + 77 };   /* several pieces of the copy, the last one short */

static const char *test_name = "";
static char root[512], session[600], slot[600], checkpoint_path[700], target_path[700], temporary_path[700];

/* ---- an independent check of a saved game: the game's CRC a bit at a time */

static uint32_t crc_bits(uint32_t crc, const uint8_t *data, size_t size)
{
	size_t i;
	int bit;

	for (i = 0; i < size; i++)
	{
		crc ^= data[i];
		for (bit = 0; bit < 8; bit++)
			crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
	}
	return crc;
}

static int image_whole(const uint8_t *data, size_t size)
{
	static const uint8_t zero[4] = { 0 };
	uint32_t crc = crc_bits(0xFFFFFFFFu, data, 0x148);
	uint32_t stored = data[0x148] | (uint32_t)data[0x149] << 8 | (uint32_t)data[0x14a] << 16 | (uint32_t)data[0x14b] << 24;

	crc = crc_bits(crc, zero, 4);
	crc = crc_bits(crc, data + 0x14c, size - 0x14c);
	return crc == stored;
}

/* an image as the game leaves it in its buffer or its checkpoint file: whatever is in the checksum field */
static uint8_t *make_image(int variation)
{
	uint8_t *image = malloc(IMAGE_SIZE);
	size_t i;

	CHECK(image);
	for (i = 0; i < IMAGE_SIZE; i++)
		image[i] = (uint8_t)(i * 31 + (i >> 9) + (size_t)variation * 7 + 1);
	memcpy(image + 4, "levels\\a10\\a10", 15);
	memset(image + 0x148, 0xEE, 4);
	return image;
}

static void put(const char *path, const uint8_t *data, size_t size)
{
	FILE *file = fopen(path, "wb");

	CHECK(file && fwrite(data, 1, size, file) == size && fclose(file) == 0);
}

static uint8_t *get(const char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	uint8_t *data;
	long length;

	if (!file)
		return NULL;
	CHECK(fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) >= 0 && fseek(file, 0, SEEK_SET) == 0);
	data = malloc((size_t)length + 1);
	CHECK(data && fread(data, 1, (size_t)length, file) == (size_t)length);
	fclose(file);
	*size = (size_t)length;
	return data;
}

/* 1: the file is the image with its checksum; 0: it is something else */
static int file_is_image(const char *path, const uint8_t *image)
{
	size_t size;
	uint8_t *data = get(path, &size);
	int same;

	if (!data)
		return 0;
	same = size == IMAGE_SIZE && image_whole(data, size) && !memcmp(data, image, 0x148) &&
		!memcmp(data + 0x14c, image + 0x14c, IMAGE_SIZE - 0x14c);
	free(data);
	return same;
}

static int exists(const char *path) { return access(path, F_OK) == 0; }

/* ---- the recording table */

static char calls[8192];
static int call_count;
static int fail_at = -1, dead_at = -1, dead, short_source, gone_source;
static int slot_checked, renamed;
static atomic_int gate_closed, gate_reached;
/* the call the gate holds (the worker's first write of a piece, unless a test names another) */
static const char *gate_at = "write";
/* the image a piece reader reads (replace(1, ...)) */
static const uint8_t *piece_source;

static void slot_holds_only_the_saved_game(void)
{
	DIR *folder = opendir(slot);
	struct dirent *entry;

	CHECK(folder);
	while ((entry = readdir(folder)) != NULL)
		CHECK(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") || !strcmp(entry->d_name, "savegame.bin") ||
			!strcmp(entry->d_name, "SaveMeta.xbx"));
	closedir(folder);
	slot_checked++;
}

/* 1: the call goes on; 0: it fails (and, dead, so does every call after it, doing nothing) */
static int step(const char *name)
{
	int index = call_count++;

	if (strlen(calls) + strlen(name) + 2 < sizeof(calls))
	{
		strcat(calls, name);
		strcat(calls, " ");
	}
	slot_holds_only_the_saved_game();
	if (atomic_load(&gate_closed) && !strcmp(name, gate_at))
	{
		atomic_store(&gate_reached, 1);
		while (atomic_load(&gate_closed))
			usleep(1000);
	}
	if (dead || index == dead_at)
	{
		dead = 1;
		return 0;
	}
	return index != fail_at;
}

static int rec_open(void *c, const char *path, int create) { return step(create ? "create" : "open") ? save_writer_posix_io.open(c, path, create) : -1; }
static int rec_open_folder(void *c, const char *path) { return step("open_folder") ? save_writer_posix_io.open_folder(c, path) : -1; }
static long long rec_read_at(void *c, int file, void *data, size_t size, long long offset)
{
	return step("read") ? save_writer_posix_io.read_at(c, file, data, size, offset) : -1;
}
/* a source read a piece at a time: an image in memory, as the worker's is */
static int read_piece(void *context, uint8_t *piece, size_t count, size_t offset)
{
	(void)context;
	if (!step("read") || (short_source && offset + count >= IMAGE_SIZE))
		return SAVE_WRITER_PIECE_FAILED;
	if (gone_source && offset)
		return SAVE_WRITER_PIECE_GONE;
	memcpy(piece, piece_source + offset, count);
	return SAVE_WRITER_PIECE_READ;
}
static int rec_write_at(void *c, int file, const void *data, size_t size, long long offset) { return step("write") ? save_writer_posix_io.write_at(c, file, data, size, offset) : -1; }
static int rec_full_sync(void *c, int file) { return step("full_sync") ? save_writer_posix_io.full_sync(c, file) : -1; }
static int rec_sync_folder(void *c, int folder) { return step("sync_folder") ? save_writer_posix_io.sync_folder(c, folder) : -1; }
static int rec_close(void *c, int file)
{
	/* a dead process's descriptors close all the same */
	int result = save_writer_posix_io.close(c, file);

	return step("close") ? result : -1;
}
static int rec_rename(void *c, const char *from, const char *to)
{
	if (!step("rename"))
		return -1;
	renamed = save_writer_posix_io.rename(c, from, to) == 0;
	return renamed ? 0 : -1;
}
static int rec_remove(void *c, const char *path) { return step("remove") ? save_writer_posix_io.remove(c, path) : -1; }
static long long rec_size(void *c, const char *path) { return step("size") ? save_writer_posix_io.size(c, path) : -2; }

static const struct save_writer_io recording = {
	NULL, rec_open, rec_open_folder, rec_read_at, rec_write_at, rec_full_sync, rec_sync_folder, rec_close,
	rec_rename, rec_remove, rec_size,
};

static void reset(void)
{
	calls[0] = 0;
	call_count = 0;
	fail_at = dead_at = -1;
	dead = short_source = gone_source = renamed = 0;
}

static void begin(const char *name, const uint8_t *old)
{
	test_name = name;
	reset();
	unlink(temporary_path);
	put(target_path, old, IMAGE_SIZE);
}

/* ---- one replacement */

static int replace(int from_pieces, const uint8_t *image)
{
	piece_source = image;
	if (from_pieces)
		return save_writer_replace_with_pieces(&recording, read_piece, NULL, temporary_path, target_path, IMAGE_SIZE, NULL, NULL);
	return save_writer_replace_with_image(&recording, temporary_path, target_path, image, IMAGE_SIZE, NULL, NULL);
}

/* each call's name appears, in this order, with nothing of the later ones before */
static void check_order(const char *const *names, int count)
{
	const char *at = calls;
	int i;

	for (i = 0; i < count; i++)
	{
		const char *found = strstr(at, names[i]);

		CHECK(found);
		at = found + strlen(names[i]);
	}
}

static void test_order_and_content(void)
{
	static const char *const image_order[] = { "size ", "create ", "write write write ", "full_sync ", "close ", "rename ",
		"open_folder ", "sync_folder ", "close " };
	static const char *const file_order[] = { "size ", "create ", "read write ", "read write ", "write ", "full_sync ", "close ",
		"rename ", "open_folder ", "sync_folder ", "close " };
	uint8_t *old = make_image(1), *new = make_image(2), *before = make_image(2);
	int from_file;

	for (from_file = 0; from_file < 2; from_file++)
	{
		begin(from_file ? "order, a piece at a time" : "order, from the game's buffer", old);
		CHECK(!image_whole(new, IMAGE_SIZE));
		CHECK(replace(from_file, new) == SAVE_WRITER_REPLACED);
		check_order(from_file ? file_order : image_order,
			(int)(from_file ? sizeof(file_order) / sizeof(*file_order) : sizeof(image_order) / sizeof(*image_order)));
		/* nothing is synced after the rename but the folder, and nothing renamed before the full sync */
		CHECK(strstr(calls, "full_sync ") < strstr(calls, "rename "));
		CHECK(!strstr(strstr(calls, "rename "), "full_sync "));
		CHECK(!strstr(calls, "remove "));
		CHECK(file_is_image(target_path, new));
		CHECK(!exists(temporary_path));
		/* the game's buffer is as it was */
		CHECK(!memcmp(new, before, IMAGE_SIZE));
		/* and the two ways write the same file, checksum and all */
		{
			static uint8_t *whole_way;
			size_t size;
			uint8_t *written = get(target_path, &size);

			CHECK(written && size == IMAGE_SIZE && image_whole(written, size));
			if (whole_way)
				CHECK(!memcmp(written, whole_way, size));
			else
				whole_way = written;
		}
	}
	free(old); free(new); free(before);
}

static void test_every_failure_and_every_death(void)
{
	uint8_t *old = make_image(1), *new = make_image(2), *stamped_old;
	int from_file, mode, total, index;
	size_t size;

	/* the old saved game as a file: whole, with its checksum */
	begin("failures", old);
	CHECK(replace(0, old) == SAVE_WRITER_REPLACED);
	stamped_old = get(target_path, &size);
	CHECK(stamped_old && size == IMAGE_SIZE && image_whole(stamped_old, size));
	for (from_file = 0; from_file < 2; from_file++)
	{
		begin("failures: a clean run", stamped_old);
		CHECK(replace(from_file, new) == SAVE_WRITER_REPLACED);
		total = call_count;
		CHECK(total > 8);
		for (mode = 0; mode < 2; mode++)
			for (index = 0; index < total; index++)
			{
				int result, is_old, is_new;

				begin(mode ? "a death at every call" : "a failure at every call", stamped_old);
				if (mode)
					dead_at = index;
				else
					fail_at = index;
				result = replace(from_file, new);
				is_old = file_is_image(target_path, old);
				is_new = file_is_image(target_path, new);
				/* the old one or the new one, whole */
				CHECK(is_old != is_new);
				/* the new one exactly when the rename was made, and never before the full sync */
				CHECK(is_new == renamed);
				if (renamed)
					CHECK(strstr(calls, "full_sync ") && strstr(calls, "full_sync ") < strstr(calls, "rename "));
				/* and the caller is told which, unless the process ended */
				if (!mode)
					CHECK(result == (is_new ? SAVE_WRITER_REPLACED : SAVE_WRITER_FAILED));
				/* a failure that is not the end of the process leaves no temporary file behind
				   (unless what failed was its removal) */
				if (!mode && !is_new && strcmp(calls + (strlen(calls) > 7 ? strlen(calls) - 7 : 0), "remove "))
					CHECK(!exists(temporary_path));
			}
	}
	free(old); free(new); free(stamped_old);
}

/* any other save file (a profile, a game type): the same sequence, whatever the old and new sizes */
static int file_is(const char *path, const char *text)
{
	size_t size;
	uint8_t *data = get(path, &size);
	int same = data && size == strlen(text) && !memcmp(data, text, size);

	free(data);
	return same;
}

static void test_any_file(void)
{
	static const char *const order[] = { "create ", "write ", "full_sync ", "close ", "rename ", "open_folder ", "sync_folder ",
		"close " };
	static const char old[] = "the old profile", new[] = "a new profile, longer than the old one";
	int mode, total, index;

	test_name = "any file: order";
	reset();
	put(target_path, (const uint8_t *)old, strlen(old));
	CHECK(save_writer_replace_with_data(&recording, temporary_path, target_path, (const uint8_t *)new, strlen(new)) ==
		SAVE_WRITER_REPLACED);
	check_order(order, (int)(sizeof(order) / sizeof(*order)));
	CHECK(!strstr(strstr(calls, "rename "), "full_sync ") && !strstr(calls, "remove "));
	CHECK(file_is(target_path, new) && !exists(temporary_path));
	total = call_count;
	for (mode = 0; mode < 2; mode++)
		for (index = 0; index < total; index++)
		{
			int result, is_new;

			test_name = mode ? "any file: a death at every call" : "any file: a failure at every call";
			reset();
			unlink(temporary_path);
			put(target_path, (const uint8_t *)old, strlen(old));
			if (mode)
				dead_at = index;
			else
				fail_at = index;
			result = save_writer_replace_with_data(&recording, temporary_path, target_path, (const uint8_t *)new, strlen(new));
			is_new = file_is(target_path, new);
			CHECK(is_new != file_is(target_path, old));
			CHECK(is_new == renamed);
			if (!mode)
				CHECK(result == (is_new ? SAVE_WRITER_REPLACED : SAVE_WRITER_FAILED));
		}
	/* a file that was not there, and an empty one */
	reset();
	unlink(target_path);
	CHECK(save_writer_replace_with_data(&recording, temporary_path, target_path, (const uint8_t *)new, 0) == SAVE_WRITER_REPLACED);
	CHECK(file_is(target_path, ""));
}

static void test_short_source_and_other_sizes(void)
{
	uint8_t *old = make_image(1), *new = make_image(2);

	begin("a source that fails at its last piece", old);
	short_source = 1;
	CHECK(replace(1, new) == SAVE_WRITER_FAILED);
	CHECK(!memcmp(old, get(target_path, &(size_t){ 0 }), IMAGE_SIZE) && !exists(temporary_path));

	/* a source that is another checkpoint's by its second piece: the write is abandoned, which is no failure */
	begin("a source that is gone", old);
	gone_source = 1;
	CHECK(replace(1, new) == SAVE_WRITER_ABANDONED);
	CHECK(!memcmp(old, get(target_path, &(size_t){ 0 }), IMAGE_SIZE) && !exists(temporary_path) && !strstr(calls, "rename "));

	/* another build's saved game (another size) is left alone, as the game's own check of a profile leaves it */
	begin("a saved game of another size", old);
	put(target_path, old, IMAGE_SIZE - 1);
	CHECK(replace(0, new) == SAVE_WRITER_REFUSED && replace(1, new) == SAVE_WRITER_REFUSED);
	CHECK(!strstr(calls, "create ") && !exists(temporary_path));
	{
		size_t size;
		uint8_t *data = get(target_path, &size);

		CHECK(data && size == IMAGE_SIZE - 1 && !memcmp(data, old, size));
		free(data);
	}

	/* a slot with no saved game file yet gets one; a slot that is gone gets nothing */
	begin("no saved game yet", old);
	unlink(target_path);
	CHECK(replace(1, new) == SAVE_WRITER_REPLACED && file_is_image(target_path, new));
	free(old); free(new);
}

static void test_paths(void)
{
	char path[128];

	test_name = "paths";
	CHECK(save_writer_temporary_path("save/z/savegame.bin", "save/u/UDATA/0123456789AB/savegame.bin", path, sizeof(path)));
	CHECK(!strcmp(path, "save/z/persist-0123456789AB.new"));
	CHECK(!save_writer_temporary_path("savegame.bin", "save/u/UDATA/0123456789AB/savegame.bin", path, sizeof(path)));
	CHECK(!save_writer_temporary_path("save/z/savegame.bin", "savegame.bin", path, sizeof(path)));
	CHECK(!save_writer_temporary_path("save/z/savegame.bin", "save/u/UDATA/0123456789AB/", path, sizeof(path)));
	CHECK(!save_writer_temporary_path("save/z/savegame.bin", "save/u/UDATA/0123456789AB/savegame.bin", path, 16));
	/* never inside a slot */
	CHECK(save_writer_temporary_path(checkpoint_path, target_path, path, sizeof(path)) || strlen(checkpoint_path) > 90);
	CHECK(strncmp(temporary_path, slot, strlen(slot)) != 0);
}

static void test_clean(void)
{
	char leftover[700], other[700], kept[700];
	uint8_t byte = 1;

	test_name = "leftovers";
	snprintf(leftover, sizeof(leftover), "%s/persist-0123456789AB.new", session);
	snprintf(other, sizeof(other), "%s/persist-.new", session);
	snprintf(kept, sizeof(kept), "%s/cache.map", session);
	put(leftover, &byte, 1); put(other, &byte, 1); put(kept, &byte, 1);
	/* (the checkpoint file an older build left in the session folder goes too: nothing reads it now) */
	put(checkpoint_path, &byte, 1);
	save_writer_clean(NULL, session);
	CHECK(!exists(leftover) && exists(other) && exists(kept) && !exists(checkpoint_path));
	unlink(other); unlink(kept);
}

/* ---- the worker */

static void *open_gate_soon(void *unused)
{
	(void)unused;
	usleep(60000);
	atomic_store(&gate_closed, 0);
	return NULL;
}

static void wait_for_gate(void)
{
	int tries = 0;

	while (!atomic_load(&gate_reached) && tries++ < 5000)
		usleep(1000);
	CHECK(atomic_load(&gate_reached));
}

static void close_gate(void) { atomic_store(&gate_reached, 0); atomic_store(&gate_closed, 1); }

/* the game's thread at a checkpoint; it never waits for the disk */
static void checkpoint(const uint8_t *image, int save)
{
	/* (with the worker held at the gate, any call counted across this would be this thread's) */
	int held = atomic_load(&gate_closed) && atomic_load(&gate_reached);
	int calls_before = held ? call_count : 0;

	CHECK(save_writer_checkpoint_store(image, IMAGE_SIZE, checkpoint_path, save ? target_path : NULL));
	if (held)
		CHECK(call_count == calls_before);
}

static int checkpoint_is(const uint8_t *image)
{
	uint8_t *loaded = malloc(IMAGE_SIZE);
	int same = loaded && save_writer_checkpoint_load(loaded, IMAGE_SIZE) && !memcmp(loaded, image, IMAGE_SIZE);

	free(loaded);
	return same;
}

static void test_worker(void)
{
	uint8_t *old = make_image(1), *first = make_image(2), *second = make_image(3), *third = make_image(4);
	unsigned long count;
	pthread_t opener;

	save_writer_start(&recording, NULL);

	begin("a revert before any checkpoint", old);
	{
		uint8_t *loaded = malloc(IMAGE_SIZE);

		CHECK(loaded && !save_writer_checkpoint_load(loaded, IMAGE_SIZE));
		free(loaded);
	}

	begin("a checkpoint becomes the saved game", old);
	checkpoint(first, 1);
	CHECK(checkpoint_is(first));
	CHECK(save_writer_wait(10));
	CHECK(file_is_image(target_path, first) && !exists(temporary_path));
	CHECK(checkpoint_is(first) && !exists(checkpoint_path));
	/* a checkpoint of another size is no checkpoint to a revert that asks for this one */
	{
		uint8_t *loaded = malloc(IMAGE_SIZE + 1);

		CHECK(loaded && !save_writer_checkpoint_load(loaded, IMAGE_SIZE + 1) && !save_writer_checkpoint_load(loaded, IMAGE_SIZE - 1));
		free(loaded);
	}
	CHECK(save_writer_replaced_count() == 1);
	CHECK(strstr(calls, "full_sync ") && strstr(calls, "full_sync ") < strstr(calls, "rename "));

	begin("a checkpoint that is not to be saved", old);
	checkpoint(second, 0);
	CHECK(save_writer_wait(10));
	CHECK(!memcmp(old, get(target_path, &(size_t){ 0 }), IMAGE_SIZE) && call_count == 0);
	CHECK(checkpoint_is(second));

	/* the game keeps its next checkpoint while the worker writes this one: the write is of no checkpoint */
	begin("a checkpoint overtaken while it is copied", old);
	count = save_writer_replaced_count();
	close_gate();
	checkpoint(first, 1);
	wait_for_gate();
	CHECK(!save_writer_wait(0.05));
	checkpoint(second, 0);
	CHECK(checkpoint_is(second));
	atomic_store(&gate_closed, 0);
	CHECK(save_writer_wait(10));
	CHECK(!memcmp(old, get(target_path, &(size_t){ 0 }), IMAGE_SIZE) && !exists(temporary_path));
	CHECK(save_writer_replaced_count() == count);

	/* The class of bug this guards: the game's thread waiting for the disk at a checkpoint. The worker sits in
	   the full sync of one checkpoint (the slowest call there is) while the game keeps two more and reverts:
	   none of that waits, the sync's checkpoint never becomes the saved game, and the newest does. */
	begin("checkpoints and a revert while the worker syncs", old);
	count = save_writer_replaced_count();
	gate_at = "full_sync";
	close_gate();
	checkpoint(first, 1);
	wait_for_gate();
	checkpoint(second, 1);
	CHECK(checkpoint_is(second));
	checkpoint(third, 1);
	CHECK(checkpoint_is(third));
	CHECK(atomic_load(&gate_closed) && !save_writer_wait(0));
	atomic_store(&gate_closed, 0);
	gate_at = "write";
	CHECK(save_writer_wait(10));
	CHECK(file_is_image(target_path, third) && !exists(temporary_path) && checkpoint_is(third));
	CHECK(save_writer_replaced_count() == count + 1);

	/* and while the worker sits in the rename, past its last look at the generation: the older checkpoint
	   lands, as it always could, and the newer one follows it; the game waits for neither */
	begin("a checkpoint while the worker renames", old);
	gate_at = "rename";
	close_gate();
	checkpoint(first, 1);
	wait_for_gate();
	checkpoint(second, 1);
	CHECK(checkpoint_is(second));
	atomic_store(&gate_closed, 0);
	gate_at = "write";
	CHECK(save_writer_wait(10));
	CHECK(file_is_image(target_path, second) && !exists(temporary_path));
	CHECK(save_writer_replaced_count() == count + 3);

	/* checkpoints that arrive while one is written: only the newest is written after it */
	begin("checkpoints queue as one", old);
	close_gate();
	checkpoint(first, 1);
	wait_for_gate();
	checkpoint(second, 1);
	checkpoint(third, 1);
	atomic_store(&gate_closed, 0);
	CHECK(save_writer_wait(10));
	CHECK(file_is_image(target_path, third));
	CHECK(save_writer_replaced_count() == count + 4);

	/* Save and Quit while a checkpoint is being written: the game's buffer is what is saved, and the
	   worker's copy never lands after it */
	begin("Save and Quit over a checkpoint in progress", old);
	count = save_writer_replaced_count();
	close_gate();
	checkpoint(first, 1);
	wait_for_gate();
	CHECK(pthread_create(&opener, NULL, open_gate_soon, NULL) == 0);
	CHECK(save_writer_write_now(checkpoint_path, target_path, second, IMAGE_SIZE));
	pthread_join(opener, NULL);
	CHECK(save_writer_wait(10));
	CHECK(file_is_image(target_path, second) && !exists(temporary_path));
	CHECK(save_writer_replaced_count() == count);
	/* (the session's checkpoint is still the one before: Save and Quit writes the game's buffer) */
	CHECK(checkpoint_is(first));

	/* a profile deleted while its checkpoint is being written: nothing is written into it afterward */
	begin("a deleted profile's saved game", old);
	close_gate();
	checkpoint(first, 1);
	wait_for_gate();
	CHECK(pthread_create(&opener, NULL, open_gate_soon, NULL) == 0);
	save_writer_withdraw(target_path);
	pthread_join(opener, NULL);
	CHECK(unlink(target_path) == 0);
	CHECK(save_writer_wait(10));
	CHECK(!exists(target_path) && !exists(temporary_path));
	/* (the checkpoint itself stays, for a revert; it is never written to the deleted profile later) */
	CHECK(checkpoint_is(first));
	usleep(20000);
	CHECK(save_writer_wait(0) && !exists(target_path));
	/* and one that only waited */
	put(target_path, old, IMAGE_SIZE);
	close_gate();
	checkpoint(first, 1);
	wait_for_gate();
	checkpoint(second, 1);
	save_writer_withdraw("elsewhere/u/UDATA/FFFFFFFFFFFF/savegame.bin");
	CHECK(pthread_create(&opener, NULL, open_gate_soon, NULL) == 0);
	save_writer_withdraw(target_path);
	pthread_join(opener, NULL);
	CHECK(save_writer_wait(10));
	CHECK(!memcmp(old, get(target_path, &(size_t){ 0 }), IMAGE_SIZE));

	/* a profile about to be deleted, before the game lists its folder: a checkpoint still being written
	   is withdrawn, so nothing lands behind the listing; another profile's is left to finish */
	{
		char folder[700];

		snprintf(folder, sizeof(folder), "%s/", slot);
		begin("a deleted profile's folder, checkpoint in progress", old);
		CHECK(unlink(target_path) == 0);
		close_gate();
		checkpoint(first, 1);
		wait_for_gate();
		CHECK(pthread_create(&opener, NULL, open_gate_soon, NULL) == 0);
		save_writer_withdraw_folder(folder);
		/* (the worker is idle from here: the game may list and delete) */
		CHECK(save_writer_wait(0) && !exists(target_path) && !exists(temporary_path));
		pthread_join(opener, NULL);

		/* the form the game sends: the folder without a trailing separator; and a checkpoint that has not started */
		test_name = "a deleted profile's folder, no trailing separator";
		close_gate();
		checkpoint(first, 1);
		wait_for_gate();
		CHECK(pthread_create(&opener, NULL, open_gate_soon, NULL) == 0);
		save_writer_withdraw_folder(slot);
		CHECK(save_writer_wait(0) && !exists(target_path) && !exists(temporary_path));
		pthread_join(opener, NULL);

		test_name = "a deleted profile's folder, checkpoint not started";
		close_gate();
		checkpoint(first, 1);
		wait_for_gate();
		checkpoint(second, 1);
		CHECK(pthread_create(&opener, NULL, open_gate_soon, NULL) == 0);
		save_writer_withdraw_folder(slot);
		pthread_join(opener, NULL);
		CHECK(save_writer_wait(10) && !exists(target_path) && !exists(temporary_path));

		test_name = "another profile's folder";
		close_gate();
		checkpoint(first, 1);
		wait_for_gate();
		CHECK(pthread_create(&opener, NULL, open_gate_soon, NULL) == 0);
		save_writer_withdraw_folder("elsewhere/u/UDATA/FFFFFFFFFFFF");
		pthread_join(opener, NULL);
		CHECK(save_writer_wait(10) && file_is_image(target_path, first));
	}

	/* Save and Quit fails loudly, and keeps the old saved game, when the disk does */
	begin("Save and Quit with a failing disk", old);
	fail_at = 3;
	CHECK(!save_writer_write_now(checkpoint_path, target_path, second, IMAGE_SIZE));
	CHECK(!memcmp(old, get(target_path, &(size_t){ 0 }), IMAGE_SIZE) && !exists(temporary_path));
	free(old); free(first); free(second); free(third);
}

int main(void)
{
	char meta[700];
	uint8_t byte = 0;

	CHECK(test_mkdtemp(root, sizeof(root), "save-writer-probe"));
	snprintf(session, sizeof(session), "%s/z", root);
	snprintf(slot, sizeof(slot), "%s/u", root);
	CHECK(mkdir(session, 0755) == 0 && mkdir(slot, 0755) == 0);
	snprintf(slot, sizeof(slot), "%s/u/UDATA", root);
	CHECK(mkdir(slot, 0755) == 0);
	snprintf(slot, sizeof(slot), "%s/u/UDATA/C7201D882DAE", root);
	CHECK(mkdir(slot, 0755) == 0);
	snprintf(checkpoint_path, sizeof(checkpoint_path), "%s/savegame.bin", session);
	snprintf(target_path, sizeof(target_path), "%s/savegame.bin", slot);
	snprintf(meta, sizeof(meta), "%s/SaveMeta.xbx", slot);
	put(meta, &byte, 1);
	CHECK(save_writer_temporary_path(checkpoint_path, target_path, temporary_path, sizeof(temporary_path)));

	test_paths();
	test_order_and_content();
	test_every_failure_and_every_death();
	test_short_source_and_other_sizes();
	test_any_file();
	test_clean();
	test_worker();
	CHECK(slot_checked > 500);

	unlink(checkpoint_path); unlink(target_path); unlink(temporary_path); unlink(meta);
	rmdir(slot);
	snprintf(slot, sizeof(slot), "%s/u/UDATA", root); rmdir(slot);
	snprintf(slot, sizeof(slot), "%s/u", root); rmdir(slot);
	rmdir(session); rmdir(root);
	printf("save writer probe: ok\n");
	return 0;
}
