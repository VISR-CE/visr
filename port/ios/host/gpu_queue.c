/*
GPU_QUEUE.C

debug.encode_thread (gpu_queue.h). Records sit in chunks: the producer writes
a record's header and payload, then publishes the chunk's new end; the
consumer reads up to the published end and follows a chunk's next pointer
once it reaches a WRAP record, handing the finished chunk back for reuse.

Each operation is carried by its class (gpu_operation.h). A RECORDED one
needs a queued_<name> below that writes its record and a consume_<name> that
reads it back and makes the call; the others' entry points are generated from
their class. gpu_queue_wrap installs every one through the list, so a new
operation without its pieces does not compile.
*/

/* (Apple builds: the uptime clock and the encode thread's QoS are Darwin extensions, which a strict ISO -std hides) */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "gpu_queue.h"
#include "gpu_operation.h"
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- the encode thread (GPU_QUEUE_THREAD): the consumer on its own thread. The producer counts the records
it writes and the consumer the records it has made; the consumer spins briefly when it runs dry, then sleeps until
the producer wakes it. A drain (an AT_ONCE or QUERY call, and every present: the barrier) waits until the consumer
has made every record written */

static struct
{
	pthread_t thread;
	int running;
	pthread_mutex_t lock;
	pthread_cond_t work, idle;
	_Atomic uint64_t written, consumed;
	_Atomic int sleeping, drain_waiting;
	/* set by a wrap that ends the thread (worker_stop) */
	_Atomic int stopping;
	_Atomic unsigned long sleeps;
	void (*started)(void);
	double barrier_waited; /* the last present's wait for the consumer, in seconds (the producer's) */
} worker = { .lock = PTHREAD_MUTEX_INITIALIZER, .work = PTHREAD_COND_INITIALIZER, .idle = PTHREAD_COND_INITIALIZER };

/* how long a dry consumer spins before it sleeps: a frame's records come microseconds apart while the game
translates, and a sleep and wake per record would cost more than the record */
enum { WORKER_SPIN_NS = 50000 };

/* records that are not operations: the rest of the chunk is unused (go on in the next one); and the second eye's
replay's span markers and constant overrides (debug.stereo_replay, gpu_queue.h) */
enum
{
	RECORD_WRAP = GPU_OPERATION_COUNT,
	RECORD_SPAN_BEGIN,
	RECORD_SPAN_END,
	RECORD_EYE_CONSTANTS,
	RECORD_EYE_CONSTANTS_END,
	RECORD_LIVE_END,
	RECORD_KINDS
};

/* a record header's operation word: the operation in the low bits, flags above it. RECORD_KEYFRAME marks a frame's
first draw, whose record carries the whole packet, store and uniforms (keyframe); the two eye bits are kept for the
second eye's replay (a record for eye 0 only or eye 1 only; neither: both) */
enum
{
	RECORD_OPERATION = 0xffffu,
	RECORD_KEYFRAME = 1u << 16,
	RECORD_EYE_0 = 1u << 17,
	RECORD_EYE_1 = 1u << 18,
};
_Static_assert(RECORD_KINDS <= RECORD_OPERATION, "the operations outgrew the header's operation bits");

int gpu_queue_mode_parse(const char *value)
{
	if (!strcmp(value, "off") || !strcmp(value, "false") || !value[0])
		return GPU_QUEUE_OFF;
	if (!strcmp(value, "inline"))
		return GPU_QUEUE_INLINE;
	if (!strcmp(value, "thread"))
		return GPU_QUEUE_THREAD;
	return -1;
}

int gpu_queue_mode_choose(const char *debug, const char *display, const char **from)
{
	int mode;

	if (debug && debug[0])
	{
		if (from)
			*from = "debug.encode_thread";
		return gpu_queue_mode_parse(debug);
	}
	if (from)
		*from = "display.encode_thread";
	mode = gpu_queue_mode_parse(display ? display : "");
	return mode == GPU_QUEUE_INLINE ? -1 : mode;
}

int gpu_queue_mode_on_main_thread(int mode, const char *debug)
{
	if (mode != GPU_QUEUE_THREAD)
		return mode;
	return debug && debug[0] ? GPU_QUEUE_INLINE : GPU_QUEUE_OFF;
}

/* direct: the backend under any wrapper that only recorded commands may pass (a call hash behind the queue), for the
calls the producer makes itself (recorded commands go to the consumer's next) */
static const struct gpu_backend *direct;
static struct gpu_backend queued;
static _Atomic int mode;
static int timing;
static void (*log_line)(const char *line);

/* ---------- chunks */

enum { CHUNK_SIZE = 256 * 1024, RECORD_ALIGNMENT = 8 };

struct chunk
{
	struct chunk *_Atomic next;
	/* the bytes of data the producer has finished writing */
	_Atomic uint32_t published;
	uint32_t capacity;
	struct chunk *free_next;
	_Alignas(16) unsigned char data[];
};

struct record
{
	uint32_t operation;
	/* the payload's bytes after this header, a multiple of RECORD_ALIGNMENT */
	uint32_t size;
};

static pthread_mutex_t free_lock = PTHREAD_MUTEX_INITIALIZER;
static struct chunk *free_chunks;

static struct chunk *chunk_new(uint32_t payload)
{
	uint32_t capacity = CHUNK_SIZE;
	struct chunk *chunk = NULL;

	/* a record bigger than a chunk (a large texture upload) gets a chunk of its own size, freed after */
	if (payload + 2 * sizeof(struct record) > capacity)
		capacity = payload + 2 * sizeof(struct record);
	else
	{
		pthread_mutex_lock(&free_lock);
		chunk = free_chunks;
		if (chunk)
			free_chunks = chunk->free_next;
		pthread_mutex_unlock(&free_lock);
	}
	if (!chunk)
	{
		chunk = malloc(sizeof(*chunk) + capacity);
		if (!chunk)
			abort();
		chunk->capacity = capacity;
	}
	atomic_store_explicit(&chunk->next, NULL, memory_order_relaxed);
	atomic_store_explicit(&chunk->published, 0, memory_order_relaxed);
	chunk->free_next = NULL;
	return chunk;
}

static void chunk_done(struct chunk *chunk)
{
	if (chunk->capacity != CHUNK_SIZE)
	{
		free(chunk);
		return;
	}
	pthread_mutex_lock(&free_lock);
	chunk->free_next = free_chunks;
	free_chunks = chunk;
	pthread_mutex_unlock(&free_lock);
}

/* the producer's place, and the consumer's */
static struct chunk *write_chunk, *read_chunk;
static uint32_t write_offset, read_offset;

/* ---------- statistics: a line every STATISTICS_FRAMES presents */

enum { STATISTICS_FRAMES = 300 };

/* the producer's (written as it records) */
static struct
{
	unsigned long frames;
	uint64_t records, bytes, draws, draw_bytes, registers, full_constants, uniforms, keyframes;
	/* the waits for the consumer: AT_ONCE and QUERY calls (frame_start's included), those that found records pending,
	and their time when timed; the present barrier's time apart */
	uint64_t drains, drains_pending, drain_ns, barrier_ns;
} statistics;
/* the consumer's (with the encode thread, written there): its time in the backend, taken by the producer's line */
static _Atomic uint64_t consumed_ns;

/* the queue's clock (debug.encode_thread_timing, the barrier's wait in the latency line, the consumer's spin): a
monotonic one on Apple platforms, so a wall-clock step cannot corrupt a sample; elsewhere C11's timespec_get (the
probe's Linux build, where strict C11 does not declare clock_gettime) */
static uint64_t clock_ns(void)
{
#ifdef __APPLE__
	return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
#else
	struct timespec now;

	timespec_get(&now, TIME_UTC);
	return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
#endif
}

static int eye_statistics_text(char *text, size_t size, double frames);

static void statistics_frame(void)
{
	char line[768];
	int length;
	double frames, milliseconds;

	if (++statistics.frames < STATISTICS_FRAMES)
		return;
	frames = (double)statistics.frames;
	milliseconds = (double)atomic_exchange(&consumed_ns, 0) / 1e6 / frames;
	length = snprintf(line, sizeof(line),
		"encode queue: %lu frames, a frame: %.0f records, %.0f KiB, %.0f draws in %.0f KiB, %.0f registers, "
		"%.1f whole constant sets, %.0f uniform sets, %.1f keyframes, %.1f drains (%.2f with records pending) in "
		"%.3f ms, barrier %.3f ms, backend %.3f ms%s",
		statistics.frames, (double)statistics.records / frames, (double)statistics.bytes / 1024.0 / frames,
		(double)statistics.draws / frames, (double)statistics.draw_bytes / 1024.0 / frames,
		(double)statistics.registers / frames, (double)statistics.full_constants / frames,
		(double)statistics.uniforms / frames, (double)statistics.keyframes / frames,
		(double)statistics.drains / frames, (double)statistics.drains_pending / frames,
		(double)statistics.drain_ns / 1e6 / frames, (double)statistics.barrier_ns / 1e6 / frames, milliseconds, timing ? "" : " (not timed)");
	if (length > 0 && length < (int)sizeof(line))
		eye_statistics_text(line + length, sizeof(line) - (size_t)length, frames);
	if (log_line)
		log_line(line);
	memset(&statistics, 0, sizeof(statistics));
}

/* ---------- the producer */

static unsigned char *record_payload;
static uint32_t record_operation, record_size, record_flags;
/* the eye bit every record carries while gpu_queue_eye_only says so (0: both eyes) */
static uint32_t record_eye;

/* room for a record of size payload bytes; returns where the payload goes */
static void *record_begin(uint32_t operation, uint32_t size)
{
	uint32_t total;

	size = (size + RECORD_ALIGNMENT - 1) & ~(uint32_t)(RECORD_ALIGNMENT - 1);
	total = (uint32_t)sizeof(struct record) + size;
	/* (room for a WRAP header after it, always) */
	if (write_offset + total + sizeof(struct record) > write_chunk->capacity)
	{
		struct chunk *fresh = chunk_new(size);
		struct record *wrap = (struct record *)(write_chunk->data + write_offset);

		wrap->operation = RECORD_WRAP;
		wrap->size = 0;
		atomic_store_explicit(&write_chunk->next, fresh, memory_order_release);
		write_offset += sizeof(struct record);
		atomic_store_explicit(&write_chunk->published, write_offset, memory_order_release);
		write_chunk = fresh;
		write_offset = 0;
	}
	record_operation = operation;
	record_size = size;
	record_payload = write_chunk->data + write_offset + sizeof(struct record);
	return record_payload;
}

static void consume(void);

/* the record is written: publish it */
static void record_end(void)
{
	struct record *header = (struct record *)(write_chunk->data + write_offset);

	header->operation = record_operation | record_flags | record_eye;
	header->size = record_size;
	record_flags = 0;
	write_offset += (uint32_t)sizeof(struct record) + record_size;
	atomic_store_explicit(&write_chunk->published, write_offset, memory_order_release);
	statistics.records++;
	statistics.bytes += sizeof(struct record) + record_size;
	atomic_fetch_add(&worker.written, 1);
	if (mode == GPU_QUEUE_INLINE)
		consume();
	else if (atomic_load(&worker.sleeping))
	{
		pthread_mutex_lock(&worker.lock);
		pthread_cond_signal(&worker.work);
		pthread_mutex_unlock(&worker.lock);
	}
}

/* a record with no payload but its arguments */
static void record_simple(uint32_t operation, const void *arguments, uint32_t size)
{
	void *payload = record_begin(operation, size);

	if (size)
		memcpy(payload, arguments, size);
	record_end();
}

static unsigned long drains;

static void drain(void)
{
	if (mode == GPU_QUEUE_INLINE && write_chunk)
		consume();
	else if (mode == GPU_QUEUE_THREAD && atomic_load(&worker.consumed) != atomic_load(&worker.written))
	{
		pthread_mutex_lock(&worker.lock);
		atomic_store(&worker.drain_waiting, 1);
		pthread_cond_signal(&worker.work);
		while (atomic_load(&worker.consumed) != atomic_load(&worker.written))
			pthread_cond_wait(&worker.idle, &worker.lock);
		atomic_store(&worker.drain_waiting, 0);
		pthread_mutex_unlock(&worker.lock);
	}
}

void gpu_queue_drain(void)
{
	drains++;
	statistics.drains++;
	statistics.drains_pending += atomic_load(&worker.written) != atomic_load(&worker.consumed);
	if (timing)
	{
		uint64_t started = clock_ns();

		drain();
		statistics.drain_ns += clock_ns() - started;
	}
	else
		drain();
}

double gpu_queue_barrier_waited(void)
{
	return mode == GPU_QUEUE_THREAD ? worker.barrier_waited : -1.0;
}

unsigned long gpu_queue_drains(void)
{
	return drains;
}

/* ---------- payload helpers */

struct writer
{
	unsigned char *at;
};

static void put(struct writer *writer, const void *data, uint32_t size)
{
	memcpy(writer->at, data, size);
	writer->at += size;
}

struct consumer;

struct reader
{
	const unsigned char *at;
	struct consumer *consumer;
	/* the record is for the other eye: a draw applies its state and is not made */
	int skip;
};

static const void *take(struct reader *reader, uint32_t size)
{
	const void *at = reader->at;

	reader->at += size;
	return at;
}

#define TAKE(reader, type) (*(const type *)take((reader), sizeof(type)))

static uint32_t align8(uint32_t size)
{
	return (size + 7) & ~7u;
}

/* bytes, with their length in front, padded to 8; NULL as its length with BYTES_NULL set, so the backend is handed
the size it was called with */
static const uint64_t BYTES_NULL = 1ull << 32;

static uint32_t bytes_size(uint32_t size)
{
	return 8 + align8(size);
}

static void put_bytes(struct writer *writer, const void *data, uint32_t size)
{
	uint64_t length = data ? size : size | (uint64_t)BYTES_NULL;

	put(writer, &length, 8);
	if (data)
	{
		memcpy(writer->at, data, size);
		writer->at += align8(size);
	}
}

static const void *take_bytes(struct reader *reader, uint32_t *size)
{
	uint64_t length = TAKE(reader, uint64_t);

	*size = (uint32_t)length;
	if (length & BYTES_NULL)
		return NULL;
	return take(reader, align8((uint32_t)length));
}

/* ---------- draws: the packet as the blocks that changed since the previous draw's */

enum
{
	DRAW_BLOCK = 32,
	DRAW_BLOCKS = (sizeof(struct gpu_draw) + DRAW_BLOCK - 1) / DRAW_BLOCK,
	UNIFORM_BLOCKS = (sizeof(struct gpu_uniforms) + DRAW_BLOCK - 1) / DRAW_BLOCK,
};
_Static_assert(DRAW_BLOCKS <= 32, "a draw's changed blocks no longer fit one mask word");
_Static_assert(UNIFORM_BLOCKS <= 32, "the uniforms' changed blocks no longer fit one mask word");

/* the blocks of now that differ from copy (all of them when copy is not known yet), which copy then takes */
static uint32_t blocks_changed(const unsigned char *now, unsigned char *copy, unsigned count, int known)
{
	uint32_t mask = 0;
	unsigned block;

	for (block = 0; block < count; block++)
		if (!known || memcmp(now + block * DRAW_BLOCK, copy + block * DRAW_BLOCK, DRAW_BLOCK))
			mask |= 1u << block;
	memcpy(copy, now, count * DRAW_BLOCK);
	return mask;
}

/* the producer's copies of what the consumer holds */
static struct
{
	_Alignas(16) unsigned char draw[DRAW_BLOCKS * DRAW_BLOCK];
	int draw_known;
	const struct gpu_constant_store *store;
	uint64_t constants_serial;
	const struct gpu_uniforms *uniforms;
	uint32_t uniforms_serial;
	_Alignas(16) unsigned char uniform_blocks[UNIFORM_BLOCKS * DRAW_BLOCK];
	int uniforms_known;
} sent;

/* A consumer: where its calls go, the delta state its draw records apply to (the packet, and the store and uniforms
the backend is handed) and the backend's last answers. Consuming a record is a function of the record and this state
alone, so a second consumer (the second eye's replay, which starts from a copy of the primary's state at a frame
boundary or a span's start) reads the same records without disturbing the primary. */
struct consumer
{
	const struct gpu_backend *next;
	struct
	{
		_Alignas(16) unsigned char draw[DRAW_BLOCKS * DRAW_BLOCK];
		struct gpu_constant_store store;
		union
		{
			struct gpu_uniforms uniforms;
			unsigned char blocks[UNIFORM_BLOCKS * DRAW_BLOCK];
		} uniforms;
	} held;
	/* the backend's answers to the last draw and present consumed (inline: the one just written) */
	uint32_t draw_result, present_result;
	/* soft_depth_prepare's calls whose backend answer was not the producer's (soft_depth_ready's), and whether one
	has been logged */
	unsigned long answers_differ;
	int answer_logged;
	/* the eye bit of the records this consumer skips (the queue's: eye 1's; the replay: eye 0's). A skipped draw's
	state is still applied, since the next draw is coded as changes from it; it is not made */
	uint32_t skip;
	/* the replay's: its texture map, and its constant overrides (NULL for the queue's own consumer) */
	const struct gpu_queue_eye_map *map;
	struct overrides *overrides;
};

/* The replay's constant overrides (gpu_queue_eye_constants): an overridden register holds eye 1's value in the
store the backend is handed, and eye 0's value as eye 0's records store it beside it, so an override that ends puts
back what eye 0's stream holds. Every change to the store the backend sees moves its serial, which is the replay's
own (REPLAY_SERIAL and up), never one the queue's consumer hands the backend: the Metal backend keeps a draw's
constants while the serial stays */
struct overrides
{
	uint8_t active[GPU_CONSTANT_COUNT];
	float eye_0[GPU_CONSTANT_COUNT][4], eye_1[GPU_CONSTANT_COUNT][4];
	uint64_t serial;
};
static const uint64_t REPLAY_SERIAL = 1ull << 62;

static void overrides_changed(struct consumer *consumer)
{
	consumer->held.store.serial = REPLAY_SERIAL + ++consumer->overrides->serial;
}

/* a register eye 0's records store: under an override, eye 0's value is kept beside eye 1's */
static void register_store(struct consumer *consumer, uint32_t index, const float value[4])
{
	if (consumer->overrides && consumer->overrides->active[index])
		memcpy(consumer->overrides->eye_0[index], value, sizeof(consumer->overrides->eye_0[index]));
	else
		memcpy(consumer->held.store.c[index], value, sizeof(consumer->held.store.c[index]));
}

/* a texture as this consumer makes its calls: eye 1's for eye 0's under the replay's map */
static gpu_texture mapped(const struct consumer *consumer, gpu_texture texture)
{
	uint32_t entry;

	if (!consumer->map || !texture)
		return texture;
	for (entry = 0; entry < consumer->map->count; entry++)
		if (consumer->map->from[entry] == texture)
			return consumer->map->to[entry];
	return texture;
}

/* the queue's own consumer */
static struct consumer primary;

/* ---------- the second eye's replay, the producer's side (debug.stereo_replay, gpu_queue.h). While a span is open
the producer marks the buffers and textures that records the replay makes again read; a later upload, write,
destroy or resize of a marked one would have the replay read the new contents where eye 0's call read the old, so
the span cannot be replayed and its end answers 0. The marks are by handle, folded into a fixed bitmap: a false
match only gives up a replay */

enum { USED_BITS = 1 << 16 };

static struct
{
	int open, verify, unreplayable;
	const char *reason;
	uint64_t buffers[USED_BITS / 64], textures[USED_BITS / 64];
} span;

/* why the last spans could not be replayed, for the statistics line */
static const char *unreplayable_reason;

static void used_mark(uint64_t *bits, uint32_t handle)
{
	if (handle)
		bits[(handle % USED_BITS) / 64] |= 1ull << (handle % 64);
}

static int used(const uint64_t *bits, uint32_t handle)
{
	return handle && (bits[(handle % USED_BITS) / 64] & 1ull << (handle % 64));
}

/* a record the replay makes again reads these (not one for eye 0 alone, which it skips) */
static void span_close(const struct gpu_queue_eye_map *map);

static int span_replays_record(void)
{
	return span.open && record_eye != RECORD_EYE_0;
}

static void span_rewrite(const uint64_t *bits, uint32_t handle, const char *reason)
{
	if (span.open && !span.unreplayable && used(bits, handle))
	{
		span.unreplayable = 1;
		span.reason = reason;
	}
}

/* a draw record: its changed-block mask and flags, the changed packet blocks, then with DRAW_CONSTANTS either the
whole store (DRAW_ALL_CONSTANTS: values, serials and log as they are, so a reader that follows the log finds what
the game's store would give it) or a count, the serial and one register_entry per register changed, then with
DRAW_UNIFORMS a block mask and the changed uniform blocks */
enum { DRAW_CONSTANTS = 1, DRAW_ALL_CONSTANTS = 2, DRAW_UNIFORMS = 4 };

struct register_entry
{
	uint32_t index, pad;
	uint64_t serial;
	float value[4];
};

static uint32_t queued_draw(const struct gpu_draw *draw, const struct gpu_constant_store *constants,
	const struct gpu_uniforms *uniforms)
{
	_Alignas(16) unsigned char packet[DRAW_BLOCKS * DRAW_BLOCK];
	uint32_t mask, blocks, flags = 0, registers = 0, size, block, uniform_mask = 0;
	uint8_t changed[GPU_CONSTANT_COUNT];
	int sent_was_known;
	uint64_t seen[(GPU_CONSTANT_COUNT + 63) / 64] = {0};
	struct writer writer;

	memcpy(packet, draw, sizeof(*draw));
	memset(packet + sizeof(*draw), 0, sizeof(packet) - sizeof(*draw));
	if (span_replays_record())
	{
		unsigned index;

		for (index = 0; index < sizeof(draw->streams) / sizeof(draw->streams[0]); index++)
			used_mark(span.buffers, draw->streams[index].buffer);
		used_mark(span.buffers, draw->index_buffer);
		for (index = 0; index < sizeof(draw->stages) / sizeof(draw->stages[0]); index++)
			used_mark(span.textures, draw->stages[index].texture);
		used_mark(span.textures, draw->color_target);
		used_mark(span.textures, draw->depth_target);
	}
	sent_was_known = sent.draw_known;
	mask = blocks_changed(packet, sent.draw, DRAW_BLOCKS, sent.draw_known);
	blocks = (uint32_t)__builtin_popcount(mask);
	sent.draw_known = 1;

	/* the registers that changed since the last draw, from the store's log while it still reaches back that far */
	if (constants != sent.store || constants->serial < sent.constants_serial ||
		constants->serial - sent.constants_serial >= GPU_CONSTANT_LOG_SIZE)
		flags |= DRAW_CONSTANTS | DRAW_ALL_CONSTANTS;
	else if (constants->serial != sent.constants_serial)
	{
		uint64_t serial;

		flags |= DRAW_CONSTANTS;
		for (serial = sent.constants_serial + 1; serial <= constants->serial; serial++)
		{
			uint32_t index = constants->log[serial % GPU_CONSTANT_LOG_SIZE];

			if (index < GPU_CONSTANT_COUNT && !(seen[index / 64] & 1ull << (index % 64)))
			{
				seen[index / 64] |= 1ull << (index % 64);
				changed[registers++] = (uint8_t)index;
			}
		}
	}
	sent.store = constants;
	sent.constants_serial = constants->serial;
	/* uniforms: the blocks that changed, when the serial says anything did */
	if (uniforms != sent.uniforms || uniforms->serial != sent.uniforms_serial)
	{
		_Alignas(16) unsigned char now[UNIFORM_BLOCKS * DRAW_BLOCK] = {0};

		memcpy(now, uniforms, sizeof(*uniforms));
		uniform_mask = blocks_changed(now, sent.uniform_blocks, UNIFORM_BLOCKS, sent.uniforms_known);
		sent.uniforms_known = 1;
		flags |= DRAW_UNIFORMS;
	}
	sent.uniforms = uniforms;
	sent.uniforms_serial = uniforms->serial;

	size = 8 + blocks * DRAW_BLOCK;
	if (flags & DRAW_ALL_CONSTANTS)
		size += (uint32_t)sizeof(*constants);
	else if (flags & DRAW_CONSTANTS)
		size += 16 + registers * (uint32_t)sizeof(struct register_entry);
	if (flags & DRAW_UNIFORMS)
		size += 8 + (uint32_t)__builtin_popcount(uniform_mask) * DRAW_BLOCK;
	writer.at = record_begin(GPU_OPERATION_draw, size);
	/* (every part whole: the frame's first draw, or the first after the queue was installed) */
	if (!sent_was_known)
	{
		record_flags = RECORD_KEYFRAME;
		statistics.keyframes++;
	}
	put(&writer, &mask, 4);
	put(&writer, &flags, 4);
	for (block = 0; block < DRAW_BLOCKS; block++)
		if (mask & 1u << block)
			put(&writer, packet + block * DRAW_BLOCK, DRAW_BLOCK);
	if (flags & DRAW_ALL_CONSTANTS)
	{
		put(&writer, constants, sizeof(*constants));
		statistics.registers += GPU_CONSTANT_COUNT;
		statistics.full_constants++;
	}
	else if (flags & DRAW_CONSTANTS)
	{
		uint64_t count = registers;
		uint32_t entry;

		put(&writer, &count, 8);
		put(&writer, &constants->serial, 8);
		for (entry = 0; entry < registers; entry++)
		{
			struct register_entry value;
			uint32_t index = changed[entry];

			value.index = index;
			value.pad = 0;
			value.serial = constants->serials[index];
			memcpy(value.value, constants->c[index], sizeof(value.value));
			put(&writer, &value, sizeof(value));
		}
		statistics.registers += registers;
	}
	if (flags & DRAW_UNIFORMS)
	{
		uint64_t word = uniform_mask;

		put(&writer, &word, 8);
		for (block = 0; block < UNIFORM_BLOCKS; block++)
			if (uniform_mask & 1u << block)
				put(&writer, sent.uniform_blocks + block * DRAW_BLOCK, DRAW_BLOCK);
		statistics.uniforms++;
	}
	statistics.draws++;
	statistics.draw_bytes += sizeof(struct record) + size;
	record_end();
	/* inline: the draw has reached the backend; its answer is the backend's. With the thread it has not: the game
	is told it drew (a skipped draw only counts in the game's statistics and the TV companion's list, whose redraw
	the backend skips again) */
	return mode == GPU_QUEUE_THREAD ? 1 : primary.draw_result;
}

/* ---------- the other recorded commands */

static void queued_texture_upload(gpu_texture texture, uint32_t face, uint32_t level, const void *data, uint32_t size)
{
	struct writer writer;
	uint32_t header[4] = { texture, face, level, 0 };

	span_rewrite(span.textures, texture, "a texture upload");

	writer.at = record_begin(GPU_OPERATION_texture_upload, 16 + bytes_size(size));
	put(&writer, header, 16);
	put_bytes(&writer, data, size);
	record_end();
}

static void queued_texture_copy_level(gpu_texture source, gpu_texture destination, uint32_t level)
{
	uint32_t arguments[4] = { source, destination, level, 0 };

	if (span_replays_record())
		used_mark(span.textures, source);
	record_simple(GPU_OPERATION_texture_copy_level, arguments, sizeof(arguments));
}

static void queued_texture_copy_rects(gpu_texture source, const struct gpu_texture_copy *copies, uint32_t count)
{
	struct writer writer;
	uint64_t first = source;
	uint32_t size = count * (uint32_t)sizeof(*copies);

	if (span_replays_record())
		used_mark(span.textures, source);
	writer.at = record_begin(GPU_OPERATION_texture_copy_rects, 8 + bytes_size(size));
	put(&writer, &first, 8);
	put_bytes(&writer, copies, size);
	record_end();
}

static void queued_texture_generate_mipmaps(gpu_texture texture, uint32_t base_level)
{
	uint32_t arguments[2] = { texture, base_level };

	record_simple(GPU_OPERATION_texture_generate_mipmaps, arguments, sizeof(arguments));
}

static void queued_texture_destroy(gpu_texture texture)
{
	uint64_t argument = texture;

	span_rewrite(span.textures, texture, "a texture destroyed");
	record_simple(GPU_OPERATION_texture_destroy, &argument, sizeof(argument));
}

static void queued_texture_set_content(gpu_texture texture, uint32_t width, uint32_t height)
{
	uint32_t arguments[4] = { texture, width, height, 0 };

	span_rewrite(span.textures, texture, "a texture's content size");
	record_simple(GPU_OPERATION_texture_set_content, arguments, sizeof(arguments));
}

static void queued_texture_multisample(gpu_texture texture, uint32_t samples)
{
	uint32_t arguments[2] = { texture, samples };

	span_rewrite(span.textures, texture, "a texture's samples");
	record_simple(GPU_OPERATION_texture_multisample, arguments, sizeof(arguments));
}

static void queued_buffer_write(gpu_buffer buffer, uint32_t offset, uint32_t size, const void *data, uint32_t flags)
{
	struct writer writer;
	uint32_t header[4] = { buffer, offset, flags, 0 };

	/* (a range no queued draw reads, GPU_WRITE_UNUSED, is new to every draw before it) */
	if (!(flags & GPU_WRITE_UNUSED))
		span_rewrite(span.buffers, buffer, "a buffer rewritten");
	writer.at = record_begin(GPU_OPERATION_buffer_write, 16 + bytes_size(size));
	put(&writer, header, 16);
	put_bytes(&writer, data, size);
	record_end();
}

static void queued_clear(const struct gpu_clear *clear, const struct gpu_rect *rectangles, uint32_t count)
{
	struct writer writer;
	uint32_t size = count * (uint32_t)sizeof(*rectangles);

	writer.at = record_begin(GPU_OPERATION_clear, align8(sizeof(*clear)) + bytes_size(size));
	put(&writer, clear, sizeof(*clear));
	writer.at += align8(sizeof(*clear)) - sizeof(*clear);
	put_bytes(&writer, rectangles, size);
	record_end();
}

static void queued_visibility_begin(void)
{
	record_simple(GPU_OPERATION_visibility_begin, NULL, 0);
}

static void queued_visibility_end(uint32_t slot)
{
	uint64_t argument = slot;

	record_simple(GPU_OPERATION_visibility_end, &argument, sizeof(argument));
}

static void queued_flush(void)
{
	record_simple(GPU_OPERATION_flush, NULL, 0);
}

static void queued_upscale(gpu_texture source, gpu_texture destination, const struct gpu_rect *area,
	uint32_t anti_alias)
{
	struct writer writer;
	uint32_t header[4] = { source, destination, anti_alias, 0 };

	writer.at = record_begin(GPU_OPERATION_upscale, 16 + bytes_size(area ? sizeof(*area) : 0));
	put(&writer, header, 16);
	put_bytes(&writer, area, area ? sizeof(*area) : 0);
	record_end();
}

static void queued_anti_alias(uint32_t method, gpu_texture target, const struct gpu_rect *rect)
{
	struct writer writer;
	uint32_t header[2] = { method, target };

	writer.at = record_begin(GPU_OPERATION_anti_alias, 8 + bytes_size(rect ? sizeof(*rect) : 0));
	put(&writer, header, 8);
	put_bytes(&writer, rect, rect ? sizeof(*rect) : 0);
	record_end();
}

static void queued_companion_set(gpu_texture target)
{
	uint64_t argument = target;

	record_simple(GPU_OPERATION_companion_set, &argument, sizeof(argument));
}

/* the present is a barrier (encode spec revision 4): the game reads the next frame's input only after the consumer
has made this present, its display waits included, so input is sampled where it is without the queue */
static void present_barrier(void)
{
	uint64_t started;

	if (mode != GPU_QUEUE_THREAD)
		return;
	drains++;
	started = clock_ns();
	drain();
	started = clock_ns() - started;
	statistics.barrier_ns += started;
	worker.barrier_waited = (double)started / 1e9;
}

/* the frame's first draw is a keyframe: its record carries the whole packet, store and uniforms, so a reader that
starts at a frame boundary with no state of its own (a second consumer) gets the same calls as the primary */
static void keyframe(void)
{
	sent.draw_known = 0;
	sent.store = NULL;
	sent.uniforms = NULL;
	sent.uniforms_known = 0;
}

static uint32_t queued_present(gpu_texture back_buffer)
{
	uint64_t argument = back_buffer;

	/* (a span the game left open ends unreplayed: its frame is over) */
	if (span.open)
	{
		span.reason = "a present inside the span";
		span.unreplayable = 1;
		span_close(NULL);
	}

	record_simple(GPU_OPERATION_present, &argument, sizeof(argument));
	keyframe();
	present_barrier();
	statistics_frame();
	return primary.present_result;
}

static uint32_t queued_present_stereo(const struct gpu_stereo_present *present)
{

	/* (a span the game left open ends unreplayed: its frame is over) */
	if (span.open)
	{
		span.reason = "a present inside the span";
		span.unreplayable = 1;
		span_close(NULL);
	}
	record_simple(GPU_OPERATION_present_stereo, present, sizeof(*present));
	keyframe();
	present_barrier();
	statistics_frame();
	return primary.present_result;
}

static void queued_warm_begin(void)
{
	record_simple(GPU_OPERATION_warm_begin, NULL, 0);
}

static void queued_pipeline_warm(const struct gpu_pipeline_description *description)
{
	record_simple(GPU_OPERATION_pipeline_warm, description, sizeof(*description));
}

static void queued_warm_end(void)
{
	record_simple(GPU_OPERATION_warm_end, NULL, 0);
}

static void queued_frame_context(uint32_t scene_class, uint32_t frame_rate, const char *level)
{
	struct writer writer;
	uint32_t header[2] = { scene_class, frame_rate };
	uint32_t size = level ? (uint32_t)strlen(level) + 1 : 0;

	writer.at = record_begin(GPU_OPERATION_frame_context, 8 + bytes_size(size));
	put(&writer, header, 8);
	put_bytes(&writer, level, size);
	record_end();
}

/* ---------- the side operations (gpu_backend.h's GPU_HOST_SIDE_OPERATIONS) */

static uint32_t queued_soft_depth_ready(gpu_texture depth, const float *values, uint32_t count);

/* the answer is the producer's, from soft_depth_ready, so the game has it before the backend makes the call; the
record carries it, and the consumer counts a backend answer that differs (an allocation that failed).

Records: { depth, answer } then the values (d3d8_device.c's halo_soft_particles_prepare: near, far, the projection's
depth row p22 and p32, the fade distance, the scale), and for ssao_apply { color, depth } then its values
(d3d8_device.c's halo_ssao_after_lightmaps). Targets are handles and the view's terms sit at fixed indexes, so a replay of one
eye's records for the other (the visionOS review's item 4) can substitute the targets and patch the terms by index;
the answer belongs to the recorded target, so such a replay recomputes it or leaves the count alone. */
static uint32_t queued_soft_depth_prepare(gpu_texture depth, const float *values, uint32_t count)
{
	struct writer writer;
	uint32_t answer = queued_soft_depth_ready(depth, values, count);
	uint32_t header[2] = { depth, answer };
	/* (the count is the bytes' size, kept when values is NULL) */
	uint32_t size = count * (uint32_t)sizeof(*values);

	writer.at = record_begin(GPU_OPERATION_soft_depth_prepare, 8 + bytes_size(size));
	put(&writer, header, 8);
	put_bytes(&writer, values, size);
	record_end();
	return answer;
}

static void queued_soft_depth_finish(void)
{
	record_simple(GPU_OPERATION_soft_depth_finish, NULL, 0);
}

static void queued_ssao_apply(gpu_texture color, gpu_texture depth, const float *values, uint32_t count)
{
	struct writer writer;
	uint32_t header[2] = { color, depth };
	/* (the count is the bytes' size, kept when values is NULL) */
	uint32_t size = count * (uint32_t)sizeof(*values);

	writer.at = record_begin(GPU_OPERATION_ssao_apply, 8 + bytes_size(size));
	put(&writer, header, 8);
	put_bytes(&writer, values, size);
	record_end();
}

/* the switches (gpu_backend.h's GPU_HOST_SWITCH_OPERATIONS) */
#define QUEUED_SWITCH(name) \
	static void queued_##name(int value) \
	{ \
		int64_t argument = value; \
		record_simple(GPU_OPERATION_##name, &argument, sizeof(argument)); \
	}
GPU_HOST_SWITCH_OPERATIONS(QUEUED_SWITCH)
#undef QUEUED_SWITCH

/* ---------- the calls the producer makes itself, by class (gpu_operation.h): AT_ONCE and QUERY after the queue
is consumed up to them, PRODUCER at once (the backend keeps their state for the producer), OTHER_THREAD (d3d8_device.c's vertical_blank_thread, which must not touch the queue) as
they come. RECORDED ones are written above. */

#define QUEUED_F_RECORDED(type, name, parameters, arguments)
#define QUEUED_P_RECORDED(name, parameters, arguments)
#define QUEUED_F_AT_ONCE(type, name, parameters, arguments) \
	static type queued_##name parameters \
	{ \
		gpu_queue_drain(); \
		return direct->name arguments; \
	}
#define QUEUED_P_AT_ONCE(name, parameters, arguments) \
	static void queued_##name parameters \
	{ \
		gpu_queue_drain(); \
		direct->name arguments; \
	}
#define QUEUED_F_PRODUCER(type, name, parameters, arguments) \
	static type queued_##name parameters { return direct->name arguments; }
#define QUEUED_P_PRODUCER(name, parameters, arguments) \
	static void queued_##name parameters { direct->name arguments; }
#define QUEUED_F_QUERY QUEUED_F_AT_ONCE
#define QUEUED_P_QUERY QUEUED_P_AT_ONCE
#define QUEUED_F_THREAD_SAFE QUEUED_F_PRODUCER
#define QUEUED_P_THREAD_SAFE QUEUED_P_PRODUCER
#define QUEUED_F_OTHER_THREAD QUEUED_F_PRODUCER
#define QUEUED_P_OTHER_THREAD QUEUED_P_PRODUCER
#define QUEUED_F(type, name, parameters, arguments) \
	GPU_OPERATION_BY_CLASS(QUEUED_F_, name)(type, name, parameters, arguments)
#define QUEUED_P(name, parameters, arguments) GPU_OPERATION_BY_CLASS(QUEUED_P_, name)(name, parameters, arguments)
GPU_BACKEND_OPERATIONS(QUEUED_F, QUEUED_P)
#undef QUEUED_F
#undef QUEUED_P

/* ---------- the second eye's replay: the producer's calls (gpu_queue.h) */

int gpu_queue_eye_span_begin(int verify)
{
	if (mode == GPU_QUEUE_OFF || span.open)
		return 0;
	record_eye = 0;
	record_simple(RECORD_SPAN_BEGIN, NULL, 0);
	memset(span.buffers, 0, sizeof(span.buffers));
	memset(span.textures, 0, sizeof(span.textures));
	span.open = 1;
	span.verify = verify;
	span.unreplayable = 0;
	span.reason = NULL;
	return 1;
}

void gpu_queue_eye_only(int eye)
{
	if (!span.open)
		return;
	record_eye = eye == GPU_QUEUE_EYE_0 ? RECORD_EYE_0 : eye == GPU_QUEUE_EYE_1 ? RECORD_EYE_1 : 0;
}

static void eye_constants_record(uint32_t kind, uint32_t first, uint32_t count, const float *values)
{
	struct writer writer;
	uint32_t header[2] = { first, count };

	if (!span.open || !count)
		return;
	writer.at = record_begin(kind, 8 + (values ? count * 16 : 0));
	put(&writer, header, 8);
	if (values)
		put(&writer, values, count * 16);
	record_end();
}

void gpu_queue_eye_constants(uint32_t first, uint32_t count, const float *values)
{
	eye_constants_record(RECORD_EYE_CONSTANTS, first, count, values);
}

void gpu_queue_eye_constants_end(uint32_t first, uint32_t count)
{
	eye_constants_record(RECORD_EYE_CONSTANTS_END, first, count, NULL);
}

/* ---------- the consumer: a consume_<name> for each RECORDED operation reads its record and makes the call */

static void consume_draw(struct reader *reader)
{
	struct consumer *consumer = reader->consumer;
	uint32_t mask = TAKE(reader, uint32_t);
	uint32_t flags = TAKE(reader, uint32_t);
	uint32_t block;

	for (block = 0; block < DRAW_BLOCKS; block++)
		if (mask & 1u << block)
			memcpy(consumer->held.draw + block * DRAW_BLOCK, take(reader, DRAW_BLOCK), DRAW_BLOCK);
	if (flags & DRAW_ALL_CONSTANTS)
	{
		memcpy(&consumer->held.store, take(reader, sizeof(consumer->held.store)), sizeof(consumer->held.store));
		if (consumer->overrides)
		{
			uint32_t index;
			struct overrides *overrides = consumer->overrides;

			for (index = 0; index < GPU_CONSTANT_COUNT; index++)
				if (overrides->active[index])
				{
					memcpy(overrides->eye_0[index], consumer->held.store.c[index], sizeof(overrides->eye_0[index]));
					memcpy(consumer->held.store.c[index], overrides->eye_1[index], sizeof(overrides->eye_1[index]));
				}
		}
	}
	else if (flags & DRAW_CONSTANTS)
	{
		uint64_t count = TAKE(reader, uint64_t);
		uint64_t serial = TAKE(reader, uint64_t);
		uint64_t entry;

		for (entry = 0; entry < count; entry++)
		{
			const struct register_entry *value = take(reader, sizeof(*value));

			register_store(consumer, value->index, value->value);
			consumer->held.store.serials[value->index] = value->serial;
			consumer->held.store.log[value->serial % GPU_CONSTANT_LOG_SIZE] = (uint8_t)value->index;
		}
		consumer->held.store.serial = serial;
	}
	if ((flags & DRAW_CONSTANTS) && consumer->overrides)
		overrides_changed(consumer);
	if (flags & DRAW_UNIFORMS)
	{
		uint32_t uniform_mask = (uint32_t)TAKE(reader, uint64_t);

		for (block = 0; block < UNIFORM_BLOCKS; block++)
			if (uniform_mask & 1u << block)
				memcpy(consumer->held.uniforms.blocks + block * DRAW_BLOCK, take(reader, DRAW_BLOCK), DRAW_BLOCK);
	}
	if (reader->skip)
		return;
	if (consumer->map)
	{
		struct gpu_draw *draw = (struct gpu_draw *)consumer->held.draw;
		unsigned stage;

		draw->color_target = mapped(consumer, draw->color_target);
		draw->depth_target = mapped(consumer, draw->depth_target);
		for (stage = 0; stage < sizeof(draw->stages) / sizeof(draw->stages[0]); stage++)
			draw->stages[stage].texture = mapped(consumer, draw->stages[stage].texture);
	}
	consumer->draw_result = consumer->next->draw((const struct gpu_draw *)consumer->held.draw, &consumer->held.store,
		&consumer->held.uniforms.uniforms);
}

static void consume_texture_upload(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	reader->consumer->next->texture_upload(words[0], words[1], words[2], data, size);
}

static void consume_texture_copy_level(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);

	reader->consumer->next->texture_copy_level(mapped(reader->consumer, words[0]), mapped(reader->consumer, words[1]),
		words[2]);
}

static void consume_texture_copy_rects(struct reader *reader)
{
	uint64_t source = TAKE(reader, uint64_t);
	uint32_t size, count, copy;
	const struct gpu_texture_copy *copies = take_bytes(reader, &size);
	struct gpu_texture_copy *mapped_copies;

	count = size / (uint32_t)sizeof(struct gpu_texture_copy);
	if (!reader->consumer->map || !copies)
	{
		reader->consumer->next->texture_copy_rects((gpu_texture)source, copies, count);
		return;
	}
	mapped_copies = malloc(size ? size : 1);
	if (!mapped_copies)
		abort();
	for (copy = 0; copy < count; copy++)
	{
		mapped_copies[copy] = copies[copy];
		mapped_copies[copy].destination = mapped(reader->consumer, copies[copy].destination);
	}
	reader->consumer->next->texture_copy_rects(mapped(reader->consumer, (gpu_texture)source), mapped_copies, count);
	free(mapped_copies);
}

static void consume_texture_generate_mipmaps(struct reader *reader)
{
	const uint32_t *words = take(reader, 8);

	reader->consumer->next->texture_generate_mipmaps(mapped(reader->consumer, words[0]), words[1]);
}

static void consume_texture_destroy(struct reader *reader)
{
	reader->consumer->next->texture_destroy((gpu_texture)TAKE(reader, uint64_t));
}

static void consume_texture_set_content(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);

	reader->consumer->next->texture_set_content(words[0], words[1], words[2]);
}

static void consume_texture_multisample(struct reader *reader)
{
	const uint32_t *words = take(reader, 8);

	reader->consumer->next->texture_multisample(words[0], words[1]);
}

static void consume_buffer_write(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	reader->consumer->next->buffer_write(words[0], words[1], size, data, words[2]);
}

static void consume_clear(struct reader *reader)
{
	struct gpu_clear clear = *(const struct gpu_clear *)take(reader, align8(sizeof(struct gpu_clear)));
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	clear.color_target = mapped(reader->consumer, clear.color_target);
	clear.depth_target = mapped(reader->consumer, clear.depth_target);
	reader->consumer->next->clear(&clear, data, size / (uint32_t)sizeof(struct gpu_rect));
}

static void consume_visibility_begin(struct reader *reader)
{
	(void)reader;
	reader->consumer->next->visibility_begin();
}

static void consume_visibility_end(struct reader *reader)
{
	reader->consumer->next->visibility_end((uint32_t)TAKE(reader, uint64_t));
}

static void consume_flush(struct reader *reader)
{
	(void)reader;
	reader->consumer->next->flush();
}

static void consume_upscale(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	reader->consumer->next->upscale(mapped(reader->consumer, words[0]), mapped(reader->consumer, words[1]), data,
		words[2]);
}

static void consume_anti_alias(struct reader *reader)
{
	const uint32_t *words = take(reader, 8);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	reader->consumer->next->anti_alias(words[0], mapped(reader->consumer, words[1]), data);
}

static void consume_companion_set(struct reader *reader)
{
	reader->consumer->next->companion_set((gpu_texture)TAKE(reader, uint64_t));
}

static void consume_present(struct reader *reader)
{
	reader->consumer->present_result = reader->consumer->next->present((gpu_texture)TAKE(reader, uint64_t));
}

static void consume_present_stereo(struct reader *reader)
{
	reader->consumer->present_result = reader->consumer->next->present_stereo(take(reader, sizeof(struct gpu_stereo_present)));
}

static void consume_warm_begin(struct reader *reader)
{
	(void)reader;
	reader->consumer->next->warm_begin();
}

static void consume_pipeline_warm(struct reader *reader)
{
	reader->consumer->next->pipeline_warm(take(reader, sizeof(struct gpu_pipeline_description)));
}

static void consume_warm_end(struct reader *reader)
{
	(void)reader;
	reader->consumer->next->warm_end();
}

static void consume_frame_context(struct reader *reader)
{
	const uint32_t *words = take(reader, 8);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	reader->consumer->next->frame_context(words[0], words[1], data);
}

static void consume_soft_depth_prepare(struct reader *reader)
{
	const uint32_t *header = take(reader, 8);
	uint32_t size, answer;
	const void *values = take_bytes(reader, &size);

	answer = reader->consumer->next->soft_depth_prepare(mapped(reader->consumer, header[0]), values,
		size / (uint32_t)sizeof(float));
	if (answer != header[1])
	{
		reader->consumer->answers_differ++;
		if (!reader->consumer->answer_logged && log_line)
		{
			char line[160];

			snprintf(line, sizeof(line), "encode queue: soft particle depth answered %u where the game was told %u "
				"(logged once)", answer, header[1]);
			log_line(line);
		}
		reader->consumer->answer_logged = 1;
	}
}

static void consume_soft_depth_finish(struct reader *reader)
{
	(void)reader;
	reader->consumer->next->soft_depth_finish();
}

static void consume_ssao_apply(struct reader *reader)
{
	const uint32_t *header = take(reader, 8);
	uint32_t size;
	const void *values = take_bytes(reader, &size);

	reader->consumer->next->ssao_apply(mapped(reader->consumer, header[0]), mapped(reader->consumer, header[1]), values,
		size / (uint32_t)sizeof(float));
}

#define CONSUME_SWITCH(name) \
	static void consume_##name(struct reader *reader) { reader->consumer->next->name((int)TAKE(reader, int64_t)); }
GPU_HOST_SWITCH_OPERATIONS(CONSUME_SWITCH)
#undef CONSUME_SWITCH

/* each RECORDED operation's reader, by number; NULL for the other classes, which the queue never writes */
#define CONSUMER_RECORDED(name) consume_##name
#define CONSUMER_AT_ONCE(name) NULL
#define CONSUMER_QUERY(name) NULL
#define CONSUMER_PRODUCER(name) NULL
#define CONSUMER_OTHER_THREAD(name) NULL
#define CONSUMER_THREAD_SAFE(name) NULL
#define CONSUMER_F(type, name, parameters, arguments) [GPU_OPERATION_##name] = GPU_OPERATION_BY_CLASS(CONSUMER_, name)(name),
#define CONSUMER_P(name, parameters, arguments) [GPU_OPERATION_##name] = GPU_OPERATION_BY_CLASS(CONSUMER_, name)(name),
static void (*const consumers[GPU_OPERATION_COUNT])(struct reader *reader) = {
	GPU_BACKEND_OPERATIONS(CONSUMER_F, CONSUMER_P)
};
#undef CONSUMER_F
#undef CONSUMER_P

static void consume_record(struct consumer *consumer, uint32_t operation, const unsigned char *payload, int skip)
{
	struct reader reader = { payload, consumer, skip };

	if (operation >= GPU_OPERATION_COUNT || !consumers[operation])
	{
		fprintf(stderr, "encode queue: record %u is not one the queue writes\n", operation);
		abort();
	}
	/* (a skipped draw still applies its state; anything else skipped is not read at all) */
	if (skip && operation != GPU_OPERATION_draw)
		return;
	consumers[operation](&reader);
}

/* debug: each keyframe draw, consumed again by a consumer with no state of its own, must leave the state the primary
holds after it (a reader can start at a frame boundary) */
static int keyframe_check;
static unsigned long keyframes_checked, keyframes_differ;

static uint32_t draw_nothing(const struct gpu_draw *draw, const struct gpu_constant_store *constants,
	const struct gpu_uniforms *uniforms)
{
	(void)draw;
	(void)constants;
	(void)uniforms;
	return 1;
}

static void check_keyframe(const unsigned char *payload)
{
	static struct gpu_backend nothing = { .draw = draw_nothing };
	static struct consumer fresh;

	memset(&fresh, 0, sizeof(fresh));
	fresh.next = &nothing;
	consume_record(&fresh, GPU_OPERATION_draw, payload, 0);
	keyframes_checked++;
	if (memcmp(&fresh.held, &primary.held, sizeof(fresh.held)))
		keyframes_differ++;
}

unsigned long gpu_queue_answers_differ(void)
{
	return primary.answers_differ;
}

void gpu_queue_check_keyframes(int on)
{
	keyframe_check = on;
}

unsigned long gpu_queue_consumer_sleeps(void)
{
	return atomic_load(&worker.sleeps);
}

unsigned long gpu_queue_consumer_spin_ns(void)
{
	return WORKER_SPIN_NS;
}

unsigned long gpu_queue_keyframes_checked(unsigned long *differ)
{
	if (differ)
		*differ = keyframes_differ;
	return keyframes_checked;
}

/* ---------- the second eye's replay, the consumer's side (debug.stereo_replay, gpu_queue.h) */

/* the records a replay makes again: what draws into or reads the eye's targets in order. The rest of a span
(uploads, buffer writes, destroys, flushes, settings) the queue's consumer makes once */
static const uint8_t replayed[GPU_OPERATION_COUNT] = {
	[GPU_OPERATION_draw] = 1,
	[GPU_OPERATION_clear] = 1,
	[GPU_OPERATION_visibility_begin] = 1,
	[GPU_OPERATION_visibility_end] = 1,
	[GPU_OPERATION_texture_copy_level] = 1,
	[GPU_OPERATION_texture_copy_rects] = 1,
	[GPU_OPERATION_texture_generate_mipmaps] = 1,
	[GPU_OPERATION_upscale] = 1,
	[GPU_OPERATION_anti_alias] = 1,
	[GPU_OPERATION_soft_depth_prepare] = 1,
	[GPU_OPERATION_soft_depth_finish] = 1,
	[GPU_OPERATION_ssao_apply] = 1,
};

/* a span end's record: whether the producer found it replayable, verify, then the map */
struct span_end
{
	uint32_t replay, verify;
	struct gpu_queue_eye_map map;
};

static struct
{
	/* between a span's beginning and the end of its replay: finished chunks are kept for the replay to read */
	int open;
	struct chunk *first_chunk, *kept;
	uint32_t first_offset;
	/* verify: the live eye's calls are being logged (between the span's end and the live end) */
	int capturing, verify;
	struct gpu_queue_eye_map map;
	struct consumer replay;
	struct overrides overrides;
	const struct gpu_backend *live_next;
} eye;

static _Atomic unsigned long eye_spans, eye_replayed, eye_unreplayable, eye_verified, eye_verify_differ;
/* the replays' time (the walk and the backend calls it makes), for the statistics line */
static _Atomic uint64_t replay_ns, replay_records;

static void eye_release(void)
{
	while (eye.kept)
	{
		struct chunk *chunk = eye.kept;

		eye.kept = chunk->free_next;
		chunk_done(chunk);
	}
	eye.open = 0;
}

static void override_begin(const unsigned char *payload)
{
	const uint32_t *words = (const uint32_t *)payload;
	const float *values = (const float *)(payload + 8);
	uint32_t entry;
	struct overrides *overrides = &eye.overrides;
	struct consumer *replay = &eye.replay;

	for (entry = 0; entry < words[1] && words[0] + entry < GPU_CONSTANT_COUNT; entry++)
	{
		uint32_t index = words[0] + entry;

		if (!overrides->active[index])
			memcpy(overrides->eye_0[index], replay->held.store.c[index], sizeof(overrides->eye_0[index]));
		overrides->active[index] = 1;
		memcpy(overrides->eye_1[index], values + entry * 4, sizeof(overrides->eye_1[index]));
		memcpy(replay->held.store.c[index], values + entry * 4, sizeof(overrides->eye_1[index]));
	}
	overrides_changed(replay);
}

static void override_end(const unsigned char *payload)
{
	const uint32_t *words = (const uint32_t *)payload;
	uint32_t entry;
	struct overrides *overrides = &eye.overrides;
	struct consumer *replay = &eye.replay;

	for (entry = 0; entry < words[1] && words[0] + entry < GPU_CONSTANT_COUNT; entry++)
	{
		uint32_t index = words[0] + entry;

		if (!overrides->active[index])
			continue;
		memcpy(replay->held.store.c[index], overrides->eye_0[index], sizeof(overrides->eye_0[index]));
		overrides->active[index] = 0;
	}
	overrides_changed(replay);
}

/* the span's records again, from its beginning to its end, into next */
static void replay_span(const struct gpu_backend *next)
{
	struct chunk *chunk = eye.first_chunk;
	uint32_t offset = eye.first_offset;
	uint64_t started = clock_ns(), records = 0;

	eye.replay.next = next;
	for (;;)
	{
		const struct record *header = (const struct record *)(chunk->data + offset);
		uint32_t operation = header->operation & RECORD_OPERATION;
		const unsigned char *payload = (const unsigned char *)(header + 1);

		if (operation == RECORD_WRAP)
		{
			chunk = atomic_load_explicit(&chunk->next, memory_order_acquire);
			offset = 0;
			continue;
		}
		if (operation == RECORD_SPAN_END)
			break;
		if (operation == RECORD_EYE_CONSTANTS)
			override_begin(payload);
		else if (operation == RECORD_EYE_CONSTANTS_END)
			override_end(payload);
		else if (operation < GPU_OPERATION_COUNT && replayed[operation])
		{
			consume_record(&eye.replay, operation, payload, (header->operation & eye.replay.skip) != 0);
			records++;
		}
		offset += (uint32_t)sizeof(*header) + header->size;
	}
	atomic_fetch_add_explicit(&replay_ns, clock_ns() - started, memory_order_relaxed);
	atomic_fetch_add_explicit(&replay_records, records, memory_order_relaxed);
}

/* ---------- verify: each call a replay makes, as bytes, against the live eye's calls the game made itself. Equal
calls with equal bytes give an equal picture; the first differences are logged by call and field */

struct call_log
{
	unsigned char *bytes;
	size_t size, capacity;
};

static struct call_log live_calls, replay_call;
static size_t compare_at;
static unsigned long compare_index, compare_differ;
static int verify_tee;
static unsigned verify_spans_logged;
enum { VERIFY_SPANS_LOGGED = 4, VERIFY_LINES_A_SPAN = 6 };

static void log_put(struct call_log *log, const void *data, size_t size)
{
	if (log->size + size > log->capacity)
	{
		size_t capacity = log->capacity ? log->capacity * 2 : 1 << 20;

		while (capacity < log->size + size)
			capacity *= 2;
		log->bytes = realloc(log->bytes, capacity);
		if (!log->bytes)
			abort();
		log->capacity = capacity;
	}
	if (data)
		memcpy(log->bytes + log->size, data, size);
	else
		memset(log->bytes + log->size, 0, size);
	log->size += size;
}

/* an entry: its operation and its bytes' size, then the bytes */
static size_t entry_begin(struct call_log *log, uint32_t operation)
{
	uint32_t header[2] = { operation, 0 };
	size_t at = log->size;

	log_put(log, header, sizeof(header));
	return at;
}

static void entry_end(struct call_log *log, size_t at)
{
	uint32_t size = (uint32_t)(log->size - at - 8);

	memcpy(log->bytes + at + 4, &size, 4);
}

static const char *draw_field(size_t offset)
{
#define FIELD(name) if (offset < offsetof(struct gpu_draw, name) + sizeof(((struct gpu_draw *)0)->name)) return #name;
	FIELD(color_target) FIELD(depth_target) FIELD(vertex_shader) FIELD(pixel_shader) FIELD(viewport) FIELD(scissor)
	FIELD(depth_stencil) FIELD(blend) FIELD(raster) FIELD(stages) FIELD(streams) FIELD(attributes)
	FIELD(constant_values) FIELD(index_buffer) FIELD(index_offset) FIELD(primitive) FIELD(count) FIELD(base_vertex)
#undef FIELD
	return "?";
}

static size_t first_difference(const unsigned char *a, const unsigned char *b, size_t size)
{
	size_t at;

	for (at = 0; at < size; at++)
		if (a[at] != b[at])
			return at;
	return size;
}

/* the replay's call just written to replay_call, against the live call at compare_at */
static void verify_compare(void)
{
	const unsigned char *live = live_calls.bytes + compare_at;
	const unsigned char *made = replay_call.bytes;
	uint32_t live_operation, live_size, operation, size;
	char line[512];

	memcpy(&operation, made, 4);
	memcpy(&size, made + 4, 4);
	compare_index++;
	if (compare_at + 8 > live_calls.size)
	{
		compare_differ++;
		if (verify_spans_logged < VERIFY_SPANS_LOGGED && compare_differ <= VERIFY_LINES_A_SPAN && log_line)
		{
			snprintf(line, sizeof(line), "stereo replay verify: call %lu (operation %u) has no live call", compare_index,
				operation);
			log_line(line);
		}
		return;
	}
	memcpy(&live_operation, live, 4);
	memcpy(&live_size, live + 4, 4);
	compare_at += 8 + live_size;
	if (live_operation == operation && live_size == size && !memcmp(live + 8, made + 8, size))
		return;
	compare_differ++;
	if (verify_spans_logged >= VERIFY_SPANS_LOGGED || compare_differ > VERIFY_LINES_A_SPAN || !log_line)
		return;
	if (live_operation != operation || live_size != size)
		snprintf(line, sizeof(line), "stereo replay verify: call %lu is operation %u (%u bytes), the live eye's %u (%u "
			"bytes)", compare_index, operation, size, live_operation, live_size);
	else if (operation == GPU_OPERATION_draw)
	{
		size_t at = first_difference(live + 8, made + 8, sizeof(struct gpu_draw));
		const unsigned char *live_constants = live + 8 + sizeof(struct gpu_draw);
		const unsigned char *made_constants = made + 8 + sizeof(struct gpu_draw);
		int length, index, listed = 0;

		length = snprintf(line, sizeof(line), "stereo replay verify: call %lu (draw):", compare_index);
		if (at < sizeof(struct gpu_draw))
			length += snprintf(line + length, sizeof(line) - (size_t)length, " packet %s (byte %zu)", draw_field(at), at);
		for (index = 0; index < GPU_CONSTANT_COUNT && length < (int)sizeof(line) - 40; index++)
			if (memcmp(live_constants + index * 16, made_constants + index * 16, 16))
			{
				float a[4], b[4];

				memcpy(a, live_constants + index * 16, 16);
				memcpy(b, made_constants + index * 16, 16);
				if (listed++ < 4)
					length += snprintf(line + length, sizeof(line) - (size_t)length, " c%d live %g,%g,%g,%g replay %g,%g,%g,%g",
						index - 96, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
				else
				{
					length += snprintf(line + length, sizeof(line) - (size_t)length, " c%d", index - 96);
				}
			}
		at = first_difference(live + 8 + sizeof(struct gpu_draw) + sizeof(((struct gpu_constant_store *)0)->c),
			made + 8 + sizeof(struct gpu_draw) + sizeof(((struct gpu_constant_store *)0)->c), sizeof(struct gpu_uniforms));
		if (at < sizeof(struct gpu_uniforms))
			snprintf(line + length, sizeof(line) - (size_t)length, " uniforms (byte %zu)", at);
	}
	else
		snprintf(line, sizeof(line), "stereo replay verify: call %lu (operation %u) differs at byte %zu", compare_index,
			operation, first_difference(live + 8, made + 8, size));
	log_line(line);
}

/* a call as bytes: logged (the live eye, then made) or compared (the replay, not made) */
#define VERIFY_BEGIN(name) \
	struct call_log *log = verify_tee ? &live_calls : &replay_call; \
	size_t at; \
	if (!verify_tee) \
		replay_call.size = 0; \
	at = entry_begin(log, GPU_OPERATION_##name)
#define VERIFY_END() \
	entry_end(log, at); \
	if (!verify_tee) \
		verify_compare()

static uint32_t verify_draw(const struct gpu_draw *draw, const struct gpu_constant_store *constants,
	const struct gpu_uniforms *uniforms)
{
	struct gpu_uniforms copy = *uniforms;
	VERIFY_BEGIN(draw);
	log_put(log, draw, sizeof(*draw));
	log_put(log, constants->c, sizeof(constants->c));
	copy.serial = 0;
	log_put(log, &copy, sizeof(copy));
	VERIFY_END();
	return verify_tee ? eye.live_next->draw(draw, constants, uniforms) : 1;
}

static void verify_clear(const struct gpu_clear *clear, const struct gpu_rect *rectangles, uint32_t count)
{
	VERIFY_BEGIN(clear);
	log_put(log, clear, sizeof(*clear));
	log_put(log, rectangles, count * sizeof(*rectangles));
	VERIFY_END();
	if (verify_tee)
		eye.live_next->clear(clear, rectangles, count);
}

static void verify_visibility_begin(void)
{
	VERIFY_BEGIN(visibility_begin);
	VERIFY_END();
	if (verify_tee)
		eye.live_next->visibility_begin();
}

static void verify_visibility_end(uint32_t slot)
{
	VERIFY_BEGIN(visibility_end);
	log_put(log, &slot, sizeof(slot));
	VERIFY_END();
	if (verify_tee)
		eye.live_next->visibility_end(slot);
}

static void verify_texture_copy_level(gpu_texture source, gpu_texture destination, uint32_t level)
{
	uint32_t words[3] = { source, destination, level };
	VERIFY_BEGIN(texture_copy_level);
	log_put(log, words, sizeof(words));
	VERIFY_END();
	if (verify_tee)
		eye.live_next->texture_copy_level(source, destination, level);
}

static void verify_texture_copy_rects(gpu_texture source, const struct gpu_texture_copy *copies, uint32_t count)
{
	VERIFY_BEGIN(texture_copy_rects);
	log_put(log, &source, sizeof(source));
	log_put(log, copies, count * sizeof(*copies));
	VERIFY_END();
	if (verify_tee)
		eye.live_next->texture_copy_rects(source, copies, count);
}

static void verify_texture_generate_mipmaps(gpu_texture texture, uint32_t base_level)
{
	uint32_t words[2] = { texture, base_level };
	VERIFY_BEGIN(texture_generate_mipmaps);
	log_put(log, words, sizeof(words));
	VERIFY_END();
	if (verify_tee)
		eye.live_next->texture_generate_mipmaps(texture, base_level);
}

static void verify_upscale(gpu_texture source, gpu_texture destination, const struct gpu_rect *area,
	uint32_t anti_alias)
{
	uint32_t words[3] = { source, destination, anti_alias };
	VERIFY_BEGIN(upscale);
	log_put(log, words, sizeof(words));
	if (area)
		log_put(log, area, sizeof(*area));
	VERIFY_END();
	if (verify_tee)
		eye.live_next->upscale(source, destination, area, anti_alias);
}

static void verify_anti_alias(uint32_t method, gpu_texture target, const struct gpu_rect *rect)
{
	uint32_t words[2] = { method, target };
	VERIFY_BEGIN(anti_alias);
	log_put(log, words, sizeof(words));
	if (rect)
		log_put(log, rect, sizeof(*rect));
	VERIFY_END();
	if (verify_tee)
		eye.live_next->anti_alias(method, target, rect);
}

static uint32_t verify_soft_depth_prepare(gpu_texture depth, const float *values, uint32_t count)
{
	VERIFY_BEGIN(soft_depth_prepare);
	log_put(log, &depth, sizeof(depth));
	if (values)
		log_put(log, values, count * sizeof(*values));
	VERIFY_END();
	return verify_tee ? eye.live_next->soft_depth_prepare(depth, values, count) : 1;
}

static void verify_soft_depth_finish(void)
{
	VERIFY_BEGIN(soft_depth_finish);
	VERIFY_END();
	if (verify_tee)
		eye.live_next->soft_depth_finish();
}

static void verify_ssao_apply(gpu_texture color, gpu_texture depth, const float *values, uint32_t count)
{
	uint32_t words[2] = { color, depth };
	VERIFY_BEGIN(ssao_apply);
	log_put(log, words, sizeof(words));
	if (values)
		log_put(log, values, count * sizeof(*values));
	VERIFY_END();
	if (verify_tee)
		eye.live_next->ssao_apply(color, depth, values, count);
}
#undef VERIFY_BEGIN
#undef VERIFY_END

/* next with the replayed operations through verify_<name> */
static void verify_install(struct gpu_backend *backend, const struct gpu_backend *next)
{
	*backend = *next;
	backend->draw = verify_draw;
	backend->clear = verify_clear;
	backend->visibility_begin = verify_visibility_begin;
	backend->visibility_end = verify_visibility_end;
	backend->texture_copy_level = verify_texture_copy_level;
	backend->texture_copy_rects = verify_texture_copy_rects;
	backend->texture_generate_mipmaps = verify_texture_generate_mipmaps;
	backend->upscale = verify_upscale;
	backend->anti_alias = verify_anti_alias;
	backend->soft_depth_prepare = verify_soft_depth_prepare;
	backend->soft_depth_finish = verify_soft_depth_finish;
	backend->ssao_apply = verify_ssao_apply;
}

static struct gpu_backend verify_backend;

/* the live eye's calls are logged; the span's replay is compared with them */
static void verify_live_end(void)
{
	char line[256];

	primary.next = eye.live_next;
	verify_tee = 0;
	compare_at = 0;
	compare_index = compare_differ = 0;
	replay_span(&verify_backend);
	if (compare_at != live_calls.size)
	{
		compare_differ++;
		if (verify_spans_logged < VERIFY_SPANS_LOGGED && log_line)
		{
			snprintf(line, sizeof(line), "stereo replay verify: the live eye made more calls than the replay (%lu)",
				compare_index);
			log_line(line);
		}
	}
	atomic_fetch_add(&eye_verified, 1);
	if (compare_differ)
	{
		atomic_fetch_add(&eye_verify_differ, compare_differ);
		if (verify_spans_logged < VERIFY_SPANS_LOGGED && log_line)
		{
			snprintf(line, sizeof(line), "stereo replay verify: %lu of %lu calls differ from the live eye's",
				compare_differ, compare_index);
			log_line(line);
		}
		verify_spans_logged++;
	}
}

/* a record that is not an operation, consumed by the queue's own consumer */
static void eye_record(uint32_t operation, const struct record *header)
{
	const unsigned char *payload = (const unsigned char *)(header + 1);

	switch (operation)
	{
	case RECORD_SPAN_BEGIN:
		eye.open = 1;
		eye.kept = NULL;
		eye.first_chunk = read_chunk;
		eye.first_offset = read_offset + (uint32_t)sizeof(*header) + header->size;
		memcpy(&eye.replay.held, &primary.held, sizeof(eye.replay.held));
		memset(eye.overrides.active, 0, sizeof(eye.overrides.active));
		eye.replay.skip = RECORD_EYE_0;
		eye.replay.map = &eye.map;
		eye.replay.overrides = &eye.overrides;
		overrides_changed(&eye.replay);
		atomic_fetch_add(&eye_spans, 1);
		break;
	case RECORD_SPAN_END:
	{
		const struct span_end *end = (const struct span_end *)payload;

		if (!eye.open)
			break;
		if (!end->replay)
		{
			atomic_fetch_add(&eye_unreplayable, 1);
			eye_release();
			break;
		}
		eye.map = end->map;
		if (end->verify)
		{
			/* (the live eye's calls, made by the queue's consumer, are logged until the live end) */
			eye.capturing = 1;
			eye.live_next = primary.next;
			live_calls.size = 0;
			verify_tee = 1;
			verify_install(&verify_backend, primary.next);
			primary.next = &verify_backend;
			break;
		}
		replay_span(primary.next);
		atomic_fetch_add(&eye_replayed, 1);
		eye_release();
		break;
	}
	case RECORD_LIVE_END:
		if (!eye.capturing)
			break;
		eye.capturing = 0;
		verify_live_end();
		eye_release();
		break;
	default:
		/* (eye 1's constant overrides are the replay's alone) */
		break;
	}
}

static void span_close(const struct gpu_queue_eye_map *map)
{
	struct span_end *end;
	int replay = !span.unreplayable && map && map->count <= GPU_QUEUE_EYE_MAP_SIZE;

	record_eye = 0;
	end = record_begin(RECORD_SPAN_END, sizeof(*end));
	memset(end, 0, sizeof(*end));
	end->replay = (uint32_t)replay;
	end->verify = (uint32_t)(replay && span.verify);
	if (map && replay)
		end->map = *map;
	record_end();
	span.open = 0;
	if (!replay)
		unreplayable_reason = span.reason ? span.reason : "no map";
}

int gpu_queue_eye_span_end(const struct gpu_queue_eye_map *map)
{
	if (!span.open)
		return 0;
	span_close(map);
	return !span.unreplayable;
}

void gpu_queue_eye_live_end(void)
{
	if (mode != GPU_QUEUE_OFF)
		record_simple(RECORD_LIVE_END, NULL, 0);
}

/* the statistics line's part for the replay, since the last line: nothing when no span opened */
static int eye_statistics_text(char *text, size_t size, double frames)
{
	static unsigned long last_spans, last_replayed, last_unreplayable;
	unsigned long spans = atomic_load(&eye_spans), replayed_spans = atomic_load(&eye_replayed);
	unsigned long unreplayable = atomic_load(&eye_unreplayable);
	double milliseconds = (double)atomic_exchange(&replay_ns, 0) / 1e6;
	double records = (double)atomic_exchange(&replay_records, 0);
	int length = 0;

	if (spans != last_spans)
		length = snprintf(text, size, "; stereo replay: %lu spans, %lu replayed, %lu not (%s), %.3f ms and %.0f records "
			"a replay", spans - last_spans, replayed_spans - last_replayed, unreplayable - last_unreplayable,
			unreplayable_reason ? unreplayable_reason : "-",
			replayed_spans != last_replayed ? milliseconds / (double)(replayed_spans - last_replayed) : 0.0,
			replayed_spans != last_replayed ? records / (double)(replayed_spans - last_replayed) : 0.0);
	(void)frames;
	last_spans = spans;
	last_replayed = replayed_spans;
	last_unreplayable = unreplayable;
	return length;
}

void gpu_queue_eye_statistics(struct gpu_queue_eye_statistics *statistics)
{
	statistics->spans = atomic_load(&eye_spans);
	statistics->replayed = atomic_load(&eye_replayed);
	statistics->unreplayable = atomic_load(&eye_unreplayable);
	statistics->verified = atomic_load(&eye_verified);
	statistics->verify_differ = atomic_load(&eye_verify_differ);
}

/* everything published so far */
static void consume(void)
{
	for (;;)
	{
		uint32_t published = atomic_load_explicit(&read_chunk->published, memory_order_acquire);
		const struct record *header;
		uint32_t operation;
		int skip;

		if (read_offset >= published)
			break;
		header = (const struct record *)(read_chunk->data + read_offset);
		operation = header->operation & RECORD_OPERATION;
		if (operation == RECORD_WRAP)
		{
			struct chunk *finished = read_chunk;

			read_chunk = atomic_load_explicit(&finished->next, memory_order_acquire);
			read_offset = 0;
			/* (a span's chunks are read again by its replay) */
			if (eye.open)
			{
				finished->free_next = eye.kept;
				eye.kept = finished;
			}
			else
				chunk_done(finished);
			continue;
		}
		skip = (header->operation & primary.skip) != 0;
		if (operation > RECORD_WRAP)
			eye_record(operation, header);
		/* (a present's time is mostly the pacer's wait for the display, not encoding: left out) */
		else if (timing && operation != GPU_OPERATION_present && operation != GPU_OPERATION_present_stereo)
		{
			uint64_t started = clock_ns();

			consume_record(&primary, operation, (const unsigned char *)(header + 1), skip);
			atomic_fetch_add_explicit(&consumed_ns, clock_ns() - started, memory_order_relaxed);
		}
		else
			consume_record(&primary, operation, (const unsigned char *)(header + 1), skip);
		if ((header->operation & RECORD_KEYFRAME) && keyframe_check)
			check_keyframe((const unsigned char *)(header + 1));
		read_offset += (uint32_t)sizeof(*header) + header->size;
		atomic_fetch_add(&worker.consumed, 1);
	}
}

static uint64_t worker_backlog(void)
{
	return atomic_load(&worker.written) - atomic_load(&worker.consumed);
}

static void *worker_main(void *unused)
{
	(void)unused;
#ifdef __APPLE__
	pthread_setname_np("encode");
#endif
	if (worker.started)
		worker.started();
	while (!atomic_load(&worker.stopping))
	{
		uint64_t dry;

		if (mode == GPU_QUEUE_THREAD)
			consume();
		/* (a drain waits for this: every record written is made) */
		if (atomic_load(&worker.drain_waiting) && !worker_backlog())
		{
			pthread_mutex_lock(&worker.lock);
			pthread_cond_broadcast(&worker.idle);
			pthread_mutex_unlock(&worker.lock);
		}
		dry = clock_ns();
		while (!worker_backlog() && clock_ns() - dry < WORKER_SPIN_NS && !atomic_load(&worker.stopping))
			;
		if (worker_backlog() && mode == GPU_QUEUE_THREAD)
			continue;
		pthread_mutex_lock(&worker.lock);
		atomic_store(&worker.sleeping, 1);
		atomic_fetch_add(&worker.sleeps, 1);
		while ((!worker_backlog() || mode != GPU_QUEUE_THREAD) && !atomic_load(&worker.stopping))
		{
			if (atomic_load(&worker.drain_waiting))
				pthread_cond_broadcast(&worker.idle);
			pthread_cond_wait(&worker.work, &worker.lock);
		}
		atomic_store(&worker.sleeping, 0);
		pthread_mutex_unlock(&worker.lock);
	}
	return NULL;
}

/* the encode thread, once: at the game thread's priority, since the game thread waits on it at every present
(encode spec revision 5 item 4) */
static void worker_start(void)
{
	pthread_attr_t attributes;

	if (worker.running)
		return;
	pthread_attr_init(&attributes);
#ifdef __APPLE__
	pthread_attr_set_qos_class_np(&attributes, QOS_CLASS_USER_INTERACTIVE, 0);
#endif
	if (pthread_create(&worker.thread, &attributes, worker_main, NULL))
		abort();
	pthread_attr_destroy(&attributes);
	worker.running = 1;
}

/* the encode thread ends (with nothing left to make: a wrap drains first), so a later wrap changes nothing under it and
a new thread calls the new wrap's consumer_started */
static void worker_stop(void)
{
	if (!worker.running)
		return;
	pthread_mutex_lock(&worker.lock);
	atomic_store(&worker.stopping, 1);
	pthread_cond_broadcast(&worker.work);
	pthread_mutex_unlock(&worker.lock);
	pthread_join(worker.thread, NULL);
	atomic_store(&worker.stopping, 0);
	worker.running = 0;
}

const struct gpu_backend *gpu_queue_wrap(const struct gpu_backend *chosen, const struct gpu_backend *beneath,
	int queue_mode, int queue_timing, void (*log)(const char *line), void (*consumer_started)(void))
{
	/* (an earlier wrap's records are all made first, and its encode thread ends: a wrap again starts a new one, bound
	to the new consumer_started) */
	drain();
	worker_stop();
	if (queue_mode == GPU_QUEUE_OFF)
	{
		mode = GPU_QUEUE_OFF;
		return chosen;
	}
	primary.next = chosen;
	direct = beneath ? beneath : chosen;
	mode = queue_mode;
	timing = queue_timing;
	log_line = log;
	if (!write_chunk)
	{
		write_chunk = read_chunk = chunk_new(0);
		write_offset = read_offset = 0;
	}
	memset(&sent, 0, sizeof(sent));
	memset(&primary.held, 0, sizeof(primary.held));
	memset(&statistics, 0, sizeof(statistics));
	atomic_store(&worker.written, 0);
	atomic_store(&worker.consumed, 0);
	worker.started = consumer_started;
	primary.answers_differ = 0;
	primary.answer_logged = 0;
	primary.skip = RECORD_EYE_1;
	primary.map = NULL;
	primary.overrides = NULL;
	span.open = 0;
	record_eye = 0;
	queued = *chosen;
	/* (a backend without an operation keeps it missing: frame_context is optional) */
#define INSTALL_F(type, name, parameters, arguments) if (chosen->name) queued.name = queued_##name;
#define INSTALL_P(name, parameters, arguments) if (chosen->name) queued.name = queued_##name;
	GPU_BACKEND_OPERATIONS(INSTALL_F, INSTALL_P)
#undef INSTALL_F
#undef INSTALL_P
	if (mode == GPU_QUEUE_THREAD)
		worker_start();
	return &queued;
}
