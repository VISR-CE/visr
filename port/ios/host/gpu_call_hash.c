/*
GPU_CALL_HASH.C

debug.gpu_call_hash (gpu_call_hash.h). Each command mixes its operation's
number, its scalar arguments, the structs it is handed and the bytes those
point to into the frame's hash, before the call goes on to the backend.
Handles a create returns are mixed in after the call, since later commands
name them. Every operation in GPU_OPERATIONS needs a hashed_<name> below:
gpu_call_hash_wrap installs them through the list, so a new operation without
one does not compile.
*/

#include "gpu_call_hash.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

enum
{
#define OPERATION_NUMBER_F(type, name, parameters, arguments) OPERATION_##name,
#define OPERATION_NUMBER_P(name, parameters, arguments) OPERATION_##name,
	GPU_OPERATIONS(OPERATION_NUMBER_F, OPERATION_NUMBER_P)
#undef OPERATION_NUMBER_F
#undef OPERATION_NUMBER_P
	OPERATION_COUNT,
	/* parts of a draw's hash, apart from its packet (OPERATION_draw) */
	PART_DRAW_CONSTANTS = OPERATION_COUNT,
	PART_DRAW_UNIFORMS,
	PARTS
};

static const char *const part_names[PARTS] = {
#define OPERATION_NAME_F(type, name, parameters, arguments) #name,
#define OPERATION_NAME_P(name, parameters, arguments) #name,
	GPU_OPERATIONS(OPERATION_NAME_F, OPERATION_NAME_P)
#undef OPERATION_NAME_F
#undef OPERATION_NAME_P
	"draw_constants", "draw_uniforms",
};

/* one wrapper: two can be installed at once (gpu_call_hash.h), so every hashed_<name> works on the instance its
thunk named, kept per thread because one instance's call can reach the other's (a queue between them) */
struct hasher
{
	const struct gpu_backend *next;
	struct gpu_backend table;
	gpu_call_hash_report report;
	uint64_t hash;
	uint32_t calls;
	/* each operation's (and draw part's) own hash this frame, to tell which one two hashes part on */
	uint64_t parts[PARTS];
	uint32_t part_calls[PARTS];
	unsigned part;
	unsigned long frame;
	pthread_t owner;
	int owner_known, second_thread, second_thread_reported;
};

static struct hasher hashers[GPU_CALL_HASH_INSTANCES];
static _Thread_local struct hasher *current;

/* ---------- mixing: 8 bytes at a time, then the tail (a debug setting, but a
128-player frame hashes about 15 MB of constants, so not a byte at a time) */

static const uint64_t PRIME = 0x100000001b3ull;

static inline void mix64(struct hasher *h, uint64_t value)
{
	h->hash ^= value;
	h->hash *= PRIME;
	h->hash ^= h->hash >> 29;
	h->parts[h->part] ^= value;
	h->parts[h->part] *= PRIME;
	h->parts[h->part] ^= h->parts[h->part] >> 29;
}

static void mix_bytes(struct hasher *h, const void *data, uint64_t size)
{
	const unsigned char *bytes = data;
	uint64_t word;

	mix64(h, size);
	if (!data)
	{
		mix64(h, 0x6e756c6cull);
		return;
	}
	for (; size >= 8; size -= 8, bytes += 8)
	{
		memcpy(&word, bytes, 8);
		mix64(h, word);
	}
	if (size)
	{
		word = 0;
		memcpy(&word, bytes, size);
		mix64(h, word);
	}
}

static void mix_string(struct hasher *h, const char *text)
{
	mix_bytes(h, text, text ? strlen(text) : 0);
}

/* a command's start: its number, and which thread sent it */
static void command(struct hasher *h, unsigned operation)
{
	pthread_t self = pthread_self();

	if (!h->owner_known)
	{
		h->owner = self;
		h->owner_known = 1;
	}
	else if (!pthread_equal(h->owner, self))
		h->second_thread = 1;
	h->calls++;
	h->part = operation;
	h->part_calls[operation]++;
	mix64(h, 0xc0de0000ull | operation);
}

/* ---------- textures */

static gpu_texture hashed_texture_create(const struct gpu_texture_description *description)
{
	struct hasher *h = current;
	gpu_texture texture;

	command(h, OPERATION_texture_create);
	mix_bytes(h, description, sizeof(*description));
	texture = h->next->texture_create(description);
	mix64(h, texture);
	return texture;
}

static void hashed_texture_upload(gpu_texture texture, uint32_t face, uint32_t level, const void *data, uint32_t size)
{
	struct hasher *h = current;
	command(h, OPERATION_texture_upload);
	mix64(h, texture);
	mix64(h, ((uint64_t)face << 32) | level);
	mix_bytes(h, data, size);
	h->next->texture_upload(texture, face, level, data, size);
}

static void hashed_texture_copy_level(gpu_texture source, gpu_texture destination, uint32_t level)
{
	struct hasher *h = current;
	command(h, OPERATION_texture_copy_level);
	mix64(h, ((uint64_t)source << 32) | destination);
	mix64(h, level);
	h->next->texture_copy_level(source, destination, level);
}

static void hashed_texture_copy_rects(gpu_texture source, const struct gpu_texture_copy *copies, uint32_t count)
{
	struct hasher *h = current;
	command(h, OPERATION_texture_copy_rects);
	mix64(h, source);
	mix_bytes(h, copies, (uint64_t)count * sizeof(*copies));
	h->next->texture_copy_rects(source, copies, count);
}

static void hashed_texture_generate_mipmaps(gpu_texture texture, uint32_t base_level)
{
	struct hasher *h = current;
	command(h, OPERATION_texture_generate_mipmaps);
	mix64(h, ((uint64_t)texture << 32) | base_level);
	h->next->texture_generate_mipmaps(texture, base_level);
}

static void hashed_texture_destroy(gpu_texture texture)
{
	struct hasher *h = current;
	command(h, OPERATION_texture_destroy);
	mix64(h, texture);
	h->next->texture_destroy(texture);
}

/* a query */
static uint32_t hashed_texture_read(gpu_texture texture, void *pixels, uint32_t size)
{
	struct hasher *h = current;
	return h->next->texture_read(texture, pixels, size);
}

static void hashed_texture_set_content(gpu_texture texture, uint32_t width, uint32_t height)
{
	struct hasher *h = current;
	command(h, OPERATION_texture_set_content);
	mix64(h, texture);
	mix64(h, ((uint64_t)width << 32) | height);
	h->next->texture_set_content(texture, width, height);
}

static void hashed_texture_multisample(gpu_texture texture, uint32_t samples)
{
	struct hasher *h = current;
	command(h, OPERATION_texture_multisample);
	mix64(h, ((uint64_t)texture << 32) | samples);
	h->next->texture_multisample(texture, samples);
}

/* ---------- buffers and streams */

static gpu_buffer hashed_buffer_create(uint32_t size)
{
	struct hasher *h = current;
	gpu_buffer buffer;

	command(h, OPERATION_buffer_create);
	mix64(h, size);
	buffer = h->next->buffer_create(size);
	mix64(h, buffer);
	return buffer;
}

static void hashed_buffer_write(gpu_buffer buffer, uint32_t offset, uint32_t size, const void *data, uint32_t flags)
{
	struct hasher *h = current;
	command(h, OPERATION_buffer_write);
	mix64(h, ((uint64_t)buffer << 32) | offset);
	mix64(h, flags);
	mix_bytes(h, data, size);
	h->next->buffer_write(buffer, offset, size, data, flags);
}

static void hashed_stream_reserve(uint32_t vertex_bytes, uint32_t index_bytes)
{
	struct hasher *h = current;
	command(h, OPERATION_stream_reserve);
	mix64(h, ((uint64_t)vertex_bytes << 32) | index_bytes);
	h->next->stream_reserve(vertex_bytes, index_bytes);
}

/* the bytes and where they went: later draws name that buffer and offset */
static uint32_t hashed_stream(uint32_t kind, const void *data, uint32_t size, gpu_buffer *buffer)
{
	struct hasher *h = current;
	uint32_t offset;

	command(h, OPERATION_stream);
	mix64(h, kind);
	mix_bytes(h, data, size);
	offset = h->next->stream(kind, data, size, buffer);
	mix64(h, ((uint64_t)*buffer << 32) | offset);
	return offset;
}

static gpu_shader hashed_shader_create(uint32_t stage, const char *source)
{
	struct hasher *h = current;
	gpu_shader shader;

	command(h, OPERATION_shader_create);
	mix64(h, stage);
	mix_string(h, source);
	shader = h->next->shader_create(stage, source);
	mix64(h, shader);
	return shader;
}

/* ---------- drawing */

static void hashed_clear(const struct gpu_clear *clear, const struct gpu_rect *rectangles, uint32_t count)
{
	struct hasher *h = current;
	command(h, OPERATION_clear);
	mix_bytes(h, clear, sizeof(*clear));
	mix_bytes(h, rectangles, (uint64_t)count * sizeof(*rectangles));
	h->next->clear(clear, rectangles, count);
}

/* the packet, and the constants and uniforms the draw reads as they are now:
the values whatever the serials say, so a draw that sees stale constants under
a current serial changes the hash */
static uint32_t hashed_draw(const struct gpu_draw *draw, const struct gpu_constant_store *constants,
	const struct gpu_uniforms *uniforms)
{
	struct hasher *h = current;
	command(h, OPERATION_draw);
	mix_bytes(h, draw, sizeof(*draw));
	h->part = PART_DRAW_CONSTANTS;
	mix64(h, constants->serial);
	mix_bytes(h, constants->c, sizeof(constants->c));
	h->part = PART_DRAW_UNIFORMS;
	mix_bytes(h, uniforms, sizeof(*uniforms));
	return h->next->draw(draw, constants, uniforms);
}

static void hashed_visibility_begin(void)
{
	struct hasher *h = current;
	command(h, OPERATION_visibility_begin);
	h->next->visibility_begin();
}

static void hashed_visibility_end(uint32_t slot)
{
	struct hasher *h = current;
	command(h, OPERATION_visibility_end);
	mix64(h, slot);
	h->next->visibility_end(slot);
}

/* a query */
static uint32_t hashed_visibility_result(uint32_t slot, uint32_t *samples)
{
	struct hasher *h = current;
	return h->next->visibility_result(slot, samples);
}

static void hashed_flush(void)
{
	struct hasher *h = current;
	command(h, OPERATION_flush);
	h->next->flush();
}

static void hashed_upscale(gpu_texture source, gpu_texture destination, const struct gpu_rect *area,
	uint32_t anti_alias)
{
	struct hasher *h = current;
	command(h, OPERATION_upscale);
	mix64(h, ((uint64_t)source << 32) | destination);
	mix_bytes(h, area, area ? sizeof(*area) : 0);
	mix64(h, anti_alias);
	h->next->upscale(source, destination, area, anti_alias);
}

/* the shader's text; the lookup textures are the same constant tables every time, and the call does not say their
size */
static uint32_t hashed_anti_alias_prepare(uint32_t method, const char *smaa_shader, const void *area_texels,
	const void *search_texels)
{
	struct hasher *h = current;
	command(h, OPERATION_anti_alias_prepare);
	mix64(h, method);
	mix_string(h, smaa_shader);
	mix64(h, (area_texels != 0) | (search_texels != 0) << 1);
	return h->next->anti_alias_prepare(method, smaa_shader, area_texels, search_texels);
}

static void hashed_anti_alias(uint32_t method, gpu_texture target, const struct gpu_rect *rect)
{
	struct hasher *h = current;
	command(h, OPERATION_anti_alias);
	mix64(h, ((uint64_t)method << 32) | target);
	mix_bytes(h, rect, rect ? sizeof(*rect) : 0);
	h->next->anti_alias(method, target, rect);
}

/* a query, and from another thread (d3d8_device.c's vertical_blank_thread): it touches nothing here */
static uint32_t hashed_vertical_blank_wait(void)
{
	struct hasher *h = current;
	return h->next->vertical_blank_wait();
}

/* a query */
static uint32_t hashed_companion_size(uint32_t *width, uint32_t *height)
{
	struct hasher *h = current;
	return h->next->companion_size(width, height);
}

static void hashed_companion_set(gpu_texture target)
{
	struct hasher *h = current;
	command(h, OPERATION_companion_set);
	mix64(h, target);
	h->next->companion_set(target);
}

static void hashed_frame_start(void)
{
	struct hasher *h = current;
	command(h, OPERATION_frame_start);
	h->next->frame_start();
}

/* the frame's last command: its hash goes to the report, and the next frame starts from nothing */
static void frame_end(struct hasher *h)
{
	int report_second_thread = h->second_thread && !h->second_thread_reported;
	char text[PARTS * 40];
	size_t length = 0;
	unsigned index;

	if (report_second_thread)
		h->second_thread_reported = 1;
	text[0] = 0;
	for (index = 0; index < PARTS && length < sizeof(text); index++)
		if (h->part_calls[index] || (index >= OPERATION_COUNT && h->part_calls[OPERATION_draw]))
			length += (size_t)snprintf(text + length, sizeof(text) - length, "%s%s:%08x", length ? " " : "",
				part_names[index], (unsigned)(h->parts[index] ^ h->parts[index] >> 32));
	if (h->report)
		h->report(h->frame, h->calls, h->hash, report_second_thread, text);
	h->frame++;
	h->calls = 0;
	h->hash = 0;
	memset(h->parts, 0, sizeof(h->parts));
	memset(h->part_calls, 0, sizeof(h->part_calls));
}

static uint32_t hashed_present(gpu_texture back_buffer)
{
	struct hasher *h = current;
	uint32_t due;

	command(h, OPERATION_present);
	mix64(h, back_buffer);
	frame_end(h);
	due = h->next->present(back_buffer);
	return due;
}

static uint32_t hashed_present_stereo(const struct gpu_stereo_present *present)
{
	struct hasher *h = current;
	command(h, OPERATION_present_stereo);
	mix_bytes(h, present, sizeof(*present));
	frame_end(h);
	return h->next->present_stereo(present);
}

/* a query */
static uint32_t hashed_call_count_take(void)
{
	struct hasher *h = current;
	return h->next->call_count_take();
}

/* a query */
static uint32_t hashed_warm_list_read(const char *name, char *text, uint32_t size)
{
	struct hasher *h = current;
	return h->next->warm_list_read(name, text, size);
}

static void hashed_warm_begin(void)
{
	struct hasher *h = current;
	command(h, OPERATION_warm_begin);
	h->next->warm_begin();
}

static void hashed_pipeline_warm(const struct gpu_pipeline_description *description)
{
	struct hasher *h = current;
	command(h, OPERATION_pipeline_warm);
	mix_bytes(h, description, sizeof(*description));
	h->next->pipeline_warm(description);
}

static void hashed_warm_end(void)
{
	struct hasher *h = current;
	command(h, OPERATION_warm_end);
	h->next->warm_end();
}

/* a query */
static uint32_t hashed_pipeline_built_take(struct gpu_pipeline_description *description)
{
	struct hasher *h = current;
	return h->next->pipeline_built_take(description);
}

static void hashed_frame_context(uint32_t scene_class, uint32_t frame_rate, const char *level)
{
	struct hasher *h = current;
	command(h, OPERATION_frame_context);
	mix64(h, ((uint64_t)scene_class << 32) | frame_rate);
	mix_string(h, level);
	h->next->frame_context(scene_class, frame_rate, level);
}

/* a query */
static uint32_t hashed_compiles_outstanding(void)
{
	struct hasher *h = current;
	return h->next->compiles_outstanding();
}

/* a snapshot is a query; the other actions change the backend's accounting */
static void hashed_compile_progress(uint32_t action, struct gpu_compile_progress *progress)
{
	struct hasher *h = current;
	if (action != GPU_COMPILE_PROGRESS_SNAPSHOT)
	{
		command(h, OPERATION_compile_progress);
		mix64(h, action);
	}
	h->next->compile_progress(action, progress);
}

/* each instance's entry points: they name the instance, then call the shared body */
#define THUNK_F0(type, name, parameters, arguments) \
	static type name##_on_0 parameters { current = &hashers[0]; return hashed_##name arguments; }
#define THUNK_P0(name, parameters, arguments) \
	static void name##_on_0 parameters { current = &hashers[0]; hashed_##name arguments; }
#define THUNK_F1(type, name, parameters, arguments) \
	static type name##_on_1 parameters { current = &hashers[1]; return hashed_##name arguments; }
#define THUNK_P1(name, parameters, arguments) \
	static void name##_on_1 parameters { current = &hashers[1]; hashed_##name arguments; }
GPU_OPERATIONS(THUNK_F0, THUNK_P0)
GPU_OPERATIONS(THUNK_F1, THUNK_P1)
_Static_assert(GPU_CALL_HASH_INSTANCES == 2, "one set of thunks per instance");

const struct gpu_backend *gpu_call_hash_wrap(unsigned instance, const struct gpu_backend *chosen,
	gpu_call_hash_report report_to)
{
	struct hasher *h = &hashers[instance];

	memset(h, 0, sizeof(*h));
	h->next = chosen;
	h->report = report_to;
	h->table = *chosen;
	/* (a backend without an operation keeps it missing: frame_context is optional) */
#define INSTALL_F0(type, name, parameters, arguments) if (chosen->name) h->table.name = name##_on_0;
#define INSTALL_P0(name, parameters, arguments) if (chosen->name) h->table.name = name##_on_0;
#define INSTALL_F1(type, name, parameters, arguments) if (chosen->name) h->table.name = name##_on_1;
#define INSTALL_P1(name, parameters, arguments) if (chosen->name) h->table.name = name##_on_1;
	if (instance == 0)
	{
		GPU_OPERATIONS(INSTALL_F0, INSTALL_P0)
	}
	else
	{
		GPU_OPERATIONS(INSTALL_F1, INSTALL_P1)
	}
	return &h->table;
}
