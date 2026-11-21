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

#include "gpu_queue.h"
#include "gpu_operation.h"
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the rest of the chunk is unused: go on in the next one */
enum { RECORD_WRAP = GPU_OPERATION_COUNT };

int gpu_queue_mode_parse(const char *value)
{
	if (!strcmp(value, "off") || !strcmp(value, "false") || !value[0])
		return GPU_QUEUE_OFF;
	if (!strcmp(value, "inline"))
		return GPU_QUEUE_INLINE;
	return -1;
}

/* next: where recorded commands go (the consumer's calls); direct: the backend under any wrapper that only
recorded commands may pass (a call hash behind the queue), for the calls the producer makes itself */
static const struct gpu_backend *next, *direct;
static struct gpu_backend queued;
/* the backend's answers to the last draw and present consumed (inline: the one just written) */
static uint32_t draw_result, present_result;
/* soft_depth_prepare's calls whose backend answer was not the producer's (soft_depth_ready's), and whether one has
been logged */
static unsigned long answers_differ;
static int answer_logged;
static int mode;
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

static struct
{
	unsigned long frames;
	uint64_t records, bytes, draws, draw_bytes, registers, full_constants, uniforms, consumed_ticks;
} statistics;

static void statistics_frame(void)
{
	char line[512];
	double frames, milliseconds;
	mach_timebase_info_data_t base;

	if (++statistics.frames < STATISTICS_FRAMES)
		return;
	frames = (double)statistics.frames;
	mach_timebase_info(&base);
	milliseconds = (double)statistics.consumed_ticks * base.numer / base.denom / 1e6 / frames;
	snprintf(line, sizeof(line),
		"encode queue: %lu frames, a frame: %.0f records, %.0f KiB, %.0f draws in %.0f KiB, %.0f registers, "
		"%.1f whole constant sets, %.0f uniform sets, backend %.3f ms%s",
		statistics.frames, (double)statistics.records / frames, (double)statistics.bytes / 1024.0 / frames,
		(double)statistics.draws / frames, (double)statistics.draw_bytes / 1024.0 / frames,
		(double)statistics.registers / frames, (double)statistics.full_constants / frames,
		(double)statistics.uniforms / frames, milliseconds, timing ? "" : " (not timed)");
	if (log_line)
		log_line(line);
	memset(&statistics, 0, sizeof(statistics));
}

/* ---------- the producer */

static unsigned char *record_payload;
static uint32_t record_operation, record_size;

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

	header->operation = record_operation;
	header->size = record_size;
	write_offset += (uint32_t)sizeof(struct record) + record_size;
	atomic_store_explicit(&write_chunk->published, write_offset, memory_order_release);
	statistics.records++;
	statistics.bytes += sizeof(struct record) + record_size;
	if (mode == GPU_QUEUE_INLINE)
		consume();
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

void gpu_queue_drain(void)
{
	drains++;
	if (mode != GPU_QUEUE_OFF && write_chunk)
		consume();
}

unsigned long gpu_queue_drains(void)
{
	return drains;
}

unsigned long gpu_queue_answers_differ(void)
{
	return answers_differ;
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

struct reader
{
	const unsigned char *at;
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

/* the consumer's: the packet, and the store and uniforms the backend is handed */
static struct
{
	_Alignas(16) unsigned char draw[DRAW_BLOCKS * DRAW_BLOCK];
	struct gpu_constant_store store;
	union
	{
		struct gpu_uniforms uniforms;
		unsigned char blocks[UNIFORM_BLOCKS * DRAW_BLOCK];
	} uniforms;
} held;

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
	uint64_t seen[(GPU_CONSTANT_COUNT + 63) / 64] = {0};
	struct writer writer;

	memcpy(packet, draw, sizeof(*draw));
	memset(packet + sizeof(*draw), 0, sizeof(packet) - sizeof(*draw));
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
	/* inline: the draw has reached the backend; its answer is the backend's */
	return draw_result;
}

/* ---------- the other recorded commands */

static void queued_texture_upload(gpu_texture texture, uint32_t face, uint32_t level, const void *data, uint32_t size)
{
	struct writer writer;
	uint32_t header[4] = { texture, face, level, 0 };

	writer.at = record_begin(GPU_OPERATION_texture_upload, 16 + bytes_size(size));
	put(&writer, header, 16);
	put_bytes(&writer, data, size);
	record_end();
}

static void queued_texture_copy_level(gpu_texture source, gpu_texture destination, uint32_t level)
{
	uint32_t arguments[4] = { source, destination, level, 0 };

	record_simple(GPU_OPERATION_texture_copy_level, arguments, sizeof(arguments));
}

static void queued_texture_copy_rects(gpu_texture source, const struct gpu_texture_copy *copies, uint32_t count)
{
	struct writer writer;
	uint64_t first = source;
	uint32_t size = count * (uint32_t)sizeof(*copies);

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

	record_simple(GPU_OPERATION_texture_destroy, &argument, sizeof(argument));
}

static void queued_texture_set_content(gpu_texture texture, uint32_t width, uint32_t height)
{
	uint32_t arguments[4] = { texture, width, height, 0 };

	record_simple(GPU_OPERATION_texture_set_content, arguments, sizeof(arguments));
}

static void queued_texture_multisample(gpu_texture texture, uint32_t samples)
{
	uint32_t arguments[2] = { texture, samples };

	record_simple(GPU_OPERATION_texture_multisample, arguments, sizeof(arguments));
}

static void queued_buffer_write(gpu_buffer buffer, uint32_t offset, uint32_t size, const void *data, uint32_t flags)
{
	struct writer writer;
	uint32_t header[4] = { buffer, offset, flags, 0 };

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

static void queued_frame_start(void)
{
	record_simple(GPU_OPERATION_frame_start, NULL, 0);
}

static uint32_t queued_present(gpu_texture back_buffer)
{
	uint64_t argument = back_buffer;

	record_simple(GPU_OPERATION_present, &argument, sizeof(argument));
	statistics_frame();
	return present_result;
}

static uint32_t queued_present_stereo(const struct gpu_stereo_present *present)
{
	record_simple(GPU_OPERATION_present_stereo, present, sizeof(*present));
	statistics_frame();
	return present_result;
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
#define QUEUED_F_OTHER_THREAD(type, name, parameters, arguments) \
	static type queued_##name parameters { return direct->name arguments; }
#define QUEUED_P_OTHER_THREAD(name, parameters, arguments) \
	static void queued_##name parameters { direct->name arguments; }
#define QUEUED_F(type, name, parameters, arguments) \
	GPU_OPERATION_BY_CLASS(QUEUED_F_, name)(type, name, parameters, arguments)
#define QUEUED_P(name, parameters, arguments) GPU_OPERATION_BY_CLASS(QUEUED_P_, name)(name, parameters, arguments)
GPU_BACKEND_OPERATIONS(QUEUED_F, QUEUED_P)
#undef QUEUED_F
#undef QUEUED_P

/* ---------- the consumer: a consume_<name> for each RECORDED operation reads its record and makes the call */

static void consume_draw(struct reader *reader)
{
	uint32_t mask = TAKE(reader, uint32_t);
	uint32_t flags = TAKE(reader, uint32_t);
	uint32_t block;

	for (block = 0; block < DRAW_BLOCKS; block++)
		if (mask & 1u << block)
			memcpy(held.draw + block * DRAW_BLOCK, take(reader, DRAW_BLOCK), DRAW_BLOCK);
	if (flags & DRAW_ALL_CONSTANTS)
		memcpy(&held.store, take(reader, sizeof(held.store)), sizeof(held.store));
	else if (flags & DRAW_CONSTANTS)
	{
		uint64_t count = TAKE(reader, uint64_t);
		uint64_t serial = TAKE(reader, uint64_t);
		uint64_t entry;

		for (entry = 0; entry < count; entry++)
		{
			const struct register_entry *value = take(reader, sizeof(*value));

			memcpy(held.store.c[value->index], value->value, sizeof(value->value));
			held.store.serials[value->index] = value->serial;
			held.store.log[value->serial % GPU_CONSTANT_LOG_SIZE] = (uint8_t)value->index;
		}
		held.store.serial = serial;
	}
	if (flags & DRAW_UNIFORMS)
	{
		uint32_t uniform_mask = (uint32_t)TAKE(reader, uint64_t);

		for (block = 0; block < UNIFORM_BLOCKS; block++)
			if (uniform_mask & 1u << block)
				memcpy(held.uniforms.blocks + block * DRAW_BLOCK, take(reader, DRAW_BLOCK), DRAW_BLOCK);
	}
	draw_result = next->draw((const struct gpu_draw *)held.draw, &held.store, &held.uniforms.uniforms);
}

static void consume_texture_upload(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	next->texture_upload(words[0], words[1], words[2], data, size);
}

static void consume_texture_copy_level(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);

	next->texture_copy_level(words[0], words[1], words[2]);
}

static void consume_texture_copy_rects(struct reader *reader)
{
	uint64_t source = TAKE(reader, uint64_t);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	next->texture_copy_rects((gpu_texture)source, data, size / (uint32_t)sizeof(struct gpu_texture_copy));
}

static void consume_texture_generate_mipmaps(struct reader *reader)
{
	const uint32_t *words = take(reader, 8);

	next->texture_generate_mipmaps(words[0], words[1]);
}

static void consume_texture_destroy(struct reader *reader)
{
	next->texture_destroy((gpu_texture)TAKE(reader, uint64_t));
}

static void consume_texture_set_content(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);

	next->texture_set_content(words[0], words[1], words[2]);
}

static void consume_texture_multisample(struct reader *reader)
{
	const uint32_t *words = take(reader, 8);

	next->texture_multisample(words[0], words[1]);
}

static void consume_buffer_write(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	next->buffer_write(words[0], words[1], size, data, words[2]);
}

static void consume_clear(struct reader *reader)
{
	const struct gpu_clear *clear = take(reader, align8(sizeof(struct gpu_clear)));
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	next->clear(clear, data, size / (uint32_t)sizeof(struct gpu_rect));
}

static void consume_visibility_begin(struct reader *reader)
{
	(void)reader;
	next->visibility_begin();
}

static void consume_visibility_end(struct reader *reader)
{
	next->visibility_end((uint32_t)TAKE(reader, uint64_t));
}

static void consume_flush(struct reader *reader)
{
	(void)reader;
	next->flush();
}

static void consume_upscale(struct reader *reader)
{
	const uint32_t *words = take(reader, 16);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	next->upscale(words[0], words[1], data, words[2]);
}

static void consume_anti_alias(struct reader *reader)
{
	const uint32_t *words = take(reader, 8);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	next->anti_alias(words[0], words[1], data);
}

static void consume_companion_set(struct reader *reader)
{
	next->companion_set((gpu_texture)TAKE(reader, uint64_t));
}

static void consume_frame_start(struct reader *reader)
{
	(void)reader;
	next->frame_start();
}

static void consume_present(struct reader *reader)
{
	present_result = next->present((gpu_texture)TAKE(reader, uint64_t));
}

static void consume_present_stereo(struct reader *reader)
{
	present_result = next->present_stereo(take(reader, sizeof(struct gpu_stereo_present)));
}

static void consume_warm_begin(struct reader *reader)
{
	(void)reader;
	next->warm_begin();
}

static void consume_pipeline_warm(struct reader *reader)
{
	next->pipeline_warm(take(reader, sizeof(struct gpu_pipeline_description)));
}

static void consume_warm_end(struct reader *reader)
{
	(void)reader;
	next->warm_end();
}

static void consume_frame_context(struct reader *reader)
{
	const uint32_t *words = take(reader, 8);
	uint32_t size;
	const void *data = take_bytes(reader, &size);

	next->frame_context(words[0], words[1], data);
}

static void consume_soft_depth_prepare(struct reader *reader)
{
	const uint32_t *header = take(reader, 8);
	uint32_t size, answer;
	const void *values = take_bytes(reader, &size);

	answer = next->soft_depth_prepare(header[0], values, size / (uint32_t)sizeof(float));
	if (answer != header[1])
	{
		answers_differ++;
		if (!answer_logged && log_line)
		{
			char line[160];

			snprintf(line, sizeof(line), "encode queue: soft particle depth answered %u where the game was told %u "
				"(logged once)", answer, header[1]);
			log_line(line);
		}
		answer_logged = 1;
	}
}

static void consume_soft_depth_finish(struct reader *reader)
{
	(void)reader;
	next->soft_depth_finish();
}

static void consume_ssao_apply(struct reader *reader)
{
	const uint32_t *header = take(reader, 8);
	uint32_t size;
	const void *values = take_bytes(reader, &size);

	next->ssao_apply(header[0], header[1], values, size / (uint32_t)sizeof(float));
}

#define CONSUME_SWITCH(name) \
	static void consume_##name(struct reader *reader) { next->name((int)TAKE(reader, int64_t)); }
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

static void consume_record(uint32_t operation, const unsigned char *payload)
{
	struct reader reader = { payload };

	if (operation >= GPU_OPERATION_COUNT || !consumers[operation])
	{
		fprintf(stderr, "encode queue: record %u is not one the queue writes\n", operation);
		abort();
	}
	consumers[operation](&reader);
}

/* everything published so far */
static void consume(void)
{
	for (;;)
	{
		uint32_t published = atomic_load_explicit(&read_chunk->published, memory_order_acquire);
		const struct record *header;

		if (read_offset >= published)
			break;
		header = (const struct record *)(read_chunk->data + read_offset);
		if (header->operation == RECORD_WRAP)
		{
			struct chunk *finished = read_chunk;

			read_chunk = atomic_load_explicit(&finished->next, memory_order_acquire);
			read_offset = 0;
			chunk_done(finished);
			continue;
		}
		/* (a present's time is mostly the pacer's wait for the display, not encoding: left out) */
		if (timing && header->operation != GPU_OPERATION_present && header->operation != GPU_OPERATION_present_stereo)
		{
			uint64_t started = mach_absolute_time();

			consume_record(header->operation, (const unsigned char *)(header + 1));
			statistics.consumed_ticks += mach_absolute_time() - started;
		}
		else
			consume_record(header->operation, (const unsigned char *)(header + 1));
		read_offset += (uint32_t)sizeof(*header) + header->size;
	}
}

const struct gpu_backend *gpu_queue_wrap(const struct gpu_backend *chosen, const struct gpu_backend *beneath,
	int queue_mode, int queue_timing, void (*log)(const char *line))
{
	if (queue_mode == GPU_QUEUE_OFF)
		return chosen;
	next = chosen;
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
	memset(&held, 0, sizeof(held));
	memset(&statistics, 0, sizeof(statistics));
	answers_differ = 0;
	answer_logged = 0;
	queued = *chosen;
	/* (a backend without an operation keeps it missing: frame_context is optional) */
#define INSTALL_F(type, name, parameters, arguments) if (chosen->name) queued.name = queued_##name;
#define INSTALL_P(name, parameters, arguments) if (chosen->name) queued.name = queued_##name;
	GPU_BACKEND_OPERATIONS(INSTALL_F, INSTALL_P)
#undef INSTALL_F
#undef INSTALL_P
	return &queued;
}
