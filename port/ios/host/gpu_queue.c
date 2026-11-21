/*
GPU_QUEUE.C

debug.encode_thread (gpu_queue.h). Records sit in chunks: the producer writes
a record's header and payload, then publishes the chunk's new end; the
consumer reads up to the published end and follows a chunk's next pointer
once it reaches a WRAP record, handing the finished chunk back for reuse.

Every operation in GPU_OPERATIONS needs a queued_<name> below:
gpu_queue_wrap installs them through the list, so a new operation without one
does not compile.
*/

#include "gpu_queue.h"
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
#define OPERATION_NUMBER_F(type, name, parameters, arguments) OPERATION_##name,
#define OPERATION_NUMBER_P(name, parameters, arguments) OPERATION_##name,
	GPU_OPERATIONS(OPERATION_NUMBER_F, OPERATION_NUMBER_P)
#undef OPERATION_NUMBER_F
#undef OPERATION_NUMBER_P
	OPERATION_COUNT,
	/* the rest of the chunk is unused: go on in the next one */
	RECORD_WRAP = OPERATION_COUNT,
};

int gpu_queue_mode_parse(const char *value)
{
	if (!strcmp(value, "off") || !strcmp(value, "false") || !value[0])
		return GPU_QUEUE_OFF;
	if (!strcmp(value, "inline"))
		return GPU_QUEUE_INLINE;
	return -1;
}

static const struct gpu_backend *next;
static struct gpu_backend queued;
/* the backend's answers to the last draw and present consumed (inline: the one just written) */
static uint32_t draw_result, present_result;
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

void gpu_queue_drain(void)
{
	if (mode != GPU_QUEUE_OFF && write_chunk)
		consume();
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

/* bytes, with their length in front, padded to 8; NULL as a length of UINT32_MAX */
static uint32_t bytes_size(uint32_t size)
{
	return 8 + align8(size);
}

static void put_bytes(struct writer *writer, const void *data, uint32_t size)
{
	uint64_t length = data ? size : UINT32_MAX;

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

	if (length == UINT32_MAX)
	{
		*size = 0;
		return NULL;
	}
	*size = (uint32_t)length;
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
	writer.at = record_begin(OPERATION_draw, size);
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

	writer.at = record_begin(OPERATION_texture_upload, 16 + bytes_size(size));
	put(&writer, header, 16);
	put_bytes(&writer, data, size);
	record_end();
}

static void queued_texture_copy_level(gpu_texture source, gpu_texture destination, uint32_t level)
{
	uint32_t arguments[4] = { source, destination, level, 0 };

	record_simple(OPERATION_texture_copy_level, arguments, sizeof(arguments));
}

static void queued_texture_copy_rects(gpu_texture source, const struct gpu_texture_copy *copies, uint32_t count)
{
	struct writer writer;
	uint64_t first = source;
	uint32_t size = count * (uint32_t)sizeof(*copies);

	writer.at = record_begin(OPERATION_texture_copy_rects, 8 + bytes_size(size));
	put(&writer, &first, 8);
	put_bytes(&writer, copies, size);
	record_end();
}

static void queued_texture_generate_mipmaps(gpu_texture texture, uint32_t base_level)
{
	uint32_t arguments[2] = { texture, base_level };

	record_simple(OPERATION_texture_generate_mipmaps, arguments, sizeof(arguments));
}

static void queued_texture_destroy(gpu_texture texture)
{
	uint64_t argument = texture;

	record_simple(OPERATION_texture_destroy, &argument, sizeof(argument));
}

static void queued_texture_set_content(gpu_texture texture, uint32_t width, uint32_t height)
{
	uint32_t arguments[4] = { texture, width, height, 0 };

	record_simple(OPERATION_texture_set_content, arguments, sizeof(arguments));
}

static void queued_texture_multisample(gpu_texture texture, uint32_t samples)
{
	uint32_t arguments[2] = { texture, samples };

	record_simple(OPERATION_texture_multisample, arguments, sizeof(arguments));
}

static void queued_buffer_write(gpu_buffer buffer, uint32_t offset, uint32_t size, const void *data, uint32_t flags)
{
	struct writer writer;
	uint32_t header[4] = { buffer, offset, flags, 0 };

	writer.at = record_begin(OPERATION_buffer_write, 16 + bytes_size(size));
	put(&writer, header, 16);
	put_bytes(&writer, data, size);
	record_end();
}

static void queued_clear(const struct gpu_clear *clear, const struct gpu_rect *rectangles, uint32_t count)
{
	struct writer writer;
	uint32_t size = count * (uint32_t)sizeof(*rectangles);

	writer.at = record_begin(OPERATION_clear, align8(sizeof(*clear)) + bytes_size(size));
	put(&writer, clear, sizeof(*clear));
	writer.at += align8(sizeof(*clear)) - sizeof(*clear);
	put_bytes(&writer, rectangles, size);
	record_end();
}

static void queued_visibility_begin(void)
{
	record_simple(OPERATION_visibility_begin, NULL, 0);
}

static void queued_visibility_end(uint32_t slot)
{
	uint64_t argument = slot;

	record_simple(OPERATION_visibility_end, &argument, sizeof(argument));
}

static void queued_flush(void)
{
	record_simple(OPERATION_flush, NULL, 0);
}

static void queued_upscale(gpu_texture source, gpu_texture destination, const struct gpu_rect *area,
	uint32_t anti_alias)
{
	struct writer writer;
	uint32_t header[4] = { source, destination, anti_alias, 0 };

	writer.at = record_begin(OPERATION_upscale, 16 + bytes_size(area ? sizeof(*area) : 0));
	put(&writer, header, 16);
	put_bytes(&writer, area, area ? sizeof(*area) : 0);
	record_end();
}

static void queued_anti_alias(uint32_t method, gpu_texture target, const struct gpu_rect *rect)
{
	struct writer writer;
	uint32_t header[2] = { method, target };

	writer.at = record_begin(OPERATION_anti_alias, 8 + bytes_size(rect ? sizeof(*rect) : 0));
	put(&writer, header, 8);
	put_bytes(&writer, rect, rect ? sizeof(*rect) : 0);
	record_end();
}

static void queued_companion_set(gpu_texture target)
{
	uint64_t argument = target;

	record_simple(OPERATION_companion_set, &argument, sizeof(argument));
}

static void queued_frame_start(void)
{
	record_simple(OPERATION_frame_start, NULL, 0);
}

static uint32_t queued_present(gpu_texture back_buffer)
{
	uint64_t argument = back_buffer;

	record_simple(OPERATION_present, &argument, sizeof(argument));
	statistics_frame();
	return present_result;
}

static uint32_t queued_present_stereo(const struct gpu_stereo_present *present)
{
	record_simple(OPERATION_present_stereo, present, sizeof(*present));
	statistics_frame();
	return present_result;
}

static void queued_warm_begin(void)
{
	record_simple(OPERATION_warm_begin, NULL, 0);
}

static void queued_pipeline_warm(const struct gpu_pipeline_description *description)
{
	record_simple(OPERATION_pipeline_warm, description, sizeof(*description));
}

static void queued_warm_end(void)
{
	record_simple(OPERATION_warm_end, NULL, 0);
}

static void queued_frame_context(uint32_t scene_class, uint32_t frame_rate, const char *level)
{
	struct writer writer;
	uint32_t header[2] = { scene_class, frame_rate };
	uint32_t size = level ? (uint32_t)strlen(level) + 1 : 0;

	writer.at = record_begin(OPERATION_frame_context, 8 + bytes_size(size));
	put(&writer, header, 8);
	put_bytes(&writer, level, size);
	record_end();
}

/* ---------- called at once, after the queue is consumed up to them */

#define AT_ONCE_F(type, name, parameters, arguments) \
	static type queued_##name parameters \
	{ \
		gpu_queue_drain(); \
		return next->name arguments; \
	}
#define AT_ONCE_P(name, parameters, arguments) \
	static void queued_##name parameters \
	{ \
		gpu_queue_drain(); \
		next->name arguments; \
	}
AT_ONCE_F(gpu_texture, texture_create, (const struct gpu_texture_description *description), (description))
AT_ONCE_F(uint32_t, texture_read, (gpu_texture texture, void *pixels, uint32_t size), (texture, pixels, size))
AT_ONCE_F(gpu_buffer, buffer_create, (uint32_t size), (size))
AT_ONCE_P(stream_reserve, (uint32_t vertex_bytes, uint32_t index_bytes), (vertex_bytes, index_bytes))
AT_ONCE_F(uint32_t, stream, (uint32_t kind, const void *data, uint32_t size, gpu_buffer *buffer),
	(kind, data, size, buffer))
AT_ONCE_F(gpu_shader, shader_create, (uint32_t stage, const char *source), (stage, source))
AT_ONCE_F(uint32_t, visibility_result, (uint32_t slot, uint32_t *samples), (slot, samples))
AT_ONCE_F(uint32_t, anti_alias_prepare, (uint32_t method, const char *smaa_shader, const void *area_texels,
	const void *search_texels), (method, smaa_shader, area_texels, search_texels))
AT_ONCE_F(uint32_t, companion_size, (uint32_t *width, uint32_t *height), (width, height))
AT_ONCE_F(uint32_t, call_count_take, (void), ())
AT_ONCE_F(uint32_t, warm_list_read, (const char *name, char *text, uint32_t size), (name, text, size))
AT_ONCE_F(uint32_t, pipeline_built_take, (struct gpu_pipeline_description *description), (description))
AT_ONCE_F(uint32_t, compiles_outstanding, (void), ())
AT_ONCE_P(compile_progress, (uint32_t action, struct gpu_compile_progress *progress), (action, progress))
#undef AT_ONCE_F
#undef AT_ONCE_P

/* d3d8_device.c's vertical_blank_thread: another thread, which must not touch the queue */
static uint32_t queued_vertical_blank_wait(void)
{
	return next->vertical_blank_wait();
}

/* ---------- the consumer */

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

static void consume_record(uint32_t operation, const unsigned char *payload)
{
	struct reader reader = { payload };
	const uint32_t *words = (const uint32_t *)payload;
	const void *data;
	uint32_t size;

	switch (operation)
	{
	case OPERATION_draw:
		consume_draw(&reader);
		break;
	case OPERATION_texture_upload:
		take(&reader, 16);
		data = take_bytes(&reader, &size);
		next->texture_upload(words[0], words[1], words[2], data, size);
		break;
	case OPERATION_texture_copy_level:
		next->texture_copy_level(words[0], words[1], words[2]);
		break;
	case OPERATION_texture_copy_rects:
		take(&reader, 8);
		data = take_bytes(&reader, &size);
		next->texture_copy_rects(words[0], data, size / (uint32_t)sizeof(struct gpu_texture_copy));
		break;
	case OPERATION_texture_generate_mipmaps:
		next->texture_generate_mipmaps(words[0], words[1]);
		break;
	case OPERATION_texture_destroy:
		next->texture_destroy(words[0]);
		break;
	case OPERATION_texture_set_content:
		next->texture_set_content(words[0], words[1], words[2]);
		break;
	case OPERATION_texture_multisample:
		next->texture_multisample(words[0], words[1]);
		break;
	case OPERATION_buffer_write:
		take(&reader, 16);
		data = take_bytes(&reader, &size);
		next->buffer_write(words[0], words[1], size, data, words[2]);
		break;
	case OPERATION_clear:
	{
		const struct gpu_clear *clear = take(&reader, align8(sizeof(struct gpu_clear)));

		data = take_bytes(&reader, &size);
		next->clear(clear, data, size / (uint32_t)sizeof(struct gpu_rect));
		break;
	}
	case OPERATION_visibility_begin:
		next->visibility_begin();
		break;
	case OPERATION_visibility_end:
		next->visibility_end(words[0]);
		break;
	case OPERATION_flush:
		next->flush();
		break;
	case OPERATION_upscale:
		take(&reader, 16);
		data = take_bytes(&reader, &size);
		next->upscale(words[0], words[1], data, words[2]);
		break;
	case OPERATION_anti_alias:
		take(&reader, 8);
		data = take_bytes(&reader, &size);
		next->anti_alias(words[0], words[1], data);
		break;
	case OPERATION_companion_set:
		next->companion_set(words[0]);
		break;
	case OPERATION_frame_start:
		next->frame_start();
		break;
	case OPERATION_present:
		present_result = next->present(words[0]);
		break;
	case OPERATION_present_stereo:
		present_result = next->present_stereo((const struct gpu_stereo_present *)payload);
		break;
	case OPERATION_warm_begin:
		next->warm_begin();
		break;
	case OPERATION_pipeline_warm:
		next->pipeline_warm((const struct gpu_pipeline_description *)payload);
		break;
	case OPERATION_warm_end:
		next->warm_end();
		break;
	case OPERATION_frame_context:
		take(&reader, 8);
		data = take_bytes(&reader, &size);
		next->frame_context(words[0], words[1], data);
		break;
	default:
		fprintf(stderr, "encode queue: record %u is not one the queue writes\n", operation);
		abort();
	}
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
		if (timing && header->operation != OPERATION_present && header->operation != OPERATION_present_stereo)
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

const struct gpu_backend *gpu_queue_wrap(const struct gpu_backend *chosen, int queue_mode, int queue_timing,
	void (*log)(const char *line))
{
	if (queue_mode == GPU_QUEUE_OFF)
		return chosen;
	next = chosen;
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
	queued = *chosen;
	/* (a backend without an operation keeps it missing: frame_context is optional) */
#define INSTALL_F(type, name, parameters, arguments) if (chosen->name) queued.name = queued_##name;
#define INSTALL_P(name, parameters, arguments) if (chosen->name) queued.name = queued_##name;
	GPU_OPERATIONS(INSTALL_F, INSTALL_P)
#undef INSTALL_F
#undef INSTALL_P
	return &queued;
}
