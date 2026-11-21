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

static const struct gpu_backend *next;
static struct gpu_backend hashed;
static gpu_call_hash_report report;
static uint64_t hash;
static uint32_t calls;
/* each operation's (and draw part's) own hash this frame, to tell which one two runs part on */
static uint64_t parts[PARTS];
static uint32_t part_calls[PARTS];
static unsigned part;
static unsigned long frame;
static pthread_t owner;
static int owner_known, second_thread, second_thread_reported;

/* ---------- mixing: 8 bytes at a time, then the tail (a debug setting, but a
128-player frame hashes about 15 MB of constants, so not a byte at a time) */

static const uint64_t PRIME = 0x100000001b3ull;

static inline void mix64(uint64_t value)
{
	hash ^= value;
	hash *= PRIME;
	hash ^= hash >> 29;
	parts[part] ^= value;
	parts[part] *= PRIME;
	parts[part] ^= parts[part] >> 29;
}

static void mix_bytes(const void *data, uint64_t size)
{
	const unsigned char *bytes = data;
	uint64_t word;

	mix64(size);
	if (!data)
	{
		mix64(0x6e756c6cull);
		return;
	}
	for (; size >= 8; size -= 8, bytes += 8)
	{
		memcpy(&word, bytes, 8);
		mix64(word);
	}
	if (size)
	{
		word = 0;
		memcpy(&word, bytes, size);
		mix64(word);
	}
}

static void mix_string(const char *text)
{
	mix_bytes(text, text ? strlen(text) : 0);
}

/* a command's start: its number, and which thread sent it */
static void command(unsigned operation)
{
	pthread_t self = pthread_self();

	if (!owner_known)
	{
		owner = self;
		owner_known = 1;
	}
	else if (!pthread_equal(owner, self))
		second_thread = 1;
	calls++;
	part = operation;
	part_calls[operation]++;
	mix64(0xc0de0000ull | operation);
}

/* ---------- textures */

static gpu_texture hashed_texture_create(const struct gpu_texture_description *description)
{
	gpu_texture texture;

	command(OPERATION_texture_create);
	mix_bytes(description, sizeof(*description));
	texture = next->texture_create(description);
	mix64(texture);
	return texture;
}

static void hashed_texture_upload(gpu_texture texture, uint32_t face, uint32_t level, const void *data, uint32_t size)
{
	command(OPERATION_texture_upload);
	mix64(texture);
	mix64(((uint64_t)face << 32) | level);
	mix_bytes(data, size);
	next->texture_upload(texture, face, level, data, size);
}

static void hashed_texture_copy_level(gpu_texture source, gpu_texture destination, uint32_t level)
{
	command(OPERATION_texture_copy_level);
	mix64(((uint64_t)source << 32) | destination);
	mix64(level);
	next->texture_copy_level(source, destination, level);
}

static void hashed_texture_copy_rects(gpu_texture source, const struct gpu_texture_copy *copies, uint32_t count)
{
	command(OPERATION_texture_copy_rects);
	mix64(source);
	mix_bytes(copies, (uint64_t)count * sizeof(*copies));
	next->texture_copy_rects(source, copies, count);
}

static void hashed_texture_generate_mipmaps(gpu_texture texture, uint32_t base_level)
{
	command(OPERATION_texture_generate_mipmaps);
	mix64(((uint64_t)texture << 32) | base_level);
	next->texture_generate_mipmaps(texture, base_level);
}

static void hashed_texture_destroy(gpu_texture texture)
{
	command(OPERATION_texture_destroy);
	mix64(texture);
	next->texture_destroy(texture);
}

/* a query */
static uint32_t hashed_texture_read(gpu_texture texture, void *pixels, uint32_t size)
{
	return next->texture_read(texture, pixels, size);
}

static void hashed_texture_set_content(gpu_texture texture, uint32_t width, uint32_t height)
{
	command(OPERATION_texture_set_content);
	mix64(texture);
	mix64(((uint64_t)width << 32) | height);
	next->texture_set_content(texture, width, height);
}

static void hashed_texture_multisample(gpu_texture texture, uint32_t samples)
{
	command(OPERATION_texture_multisample);
	mix64(((uint64_t)texture << 32) | samples);
	next->texture_multisample(texture, samples);
}

/* ---------- buffers and streams */

static gpu_buffer hashed_buffer_create(uint32_t size)
{
	gpu_buffer buffer;

	command(OPERATION_buffer_create);
	mix64(size);
	buffer = next->buffer_create(size);
	mix64(buffer);
	return buffer;
}

static void hashed_buffer_write(gpu_buffer buffer, uint32_t offset, uint32_t size, const void *data, uint32_t flags)
{
	command(OPERATION_buffer_write);
	mix64(((uint64_t)buffer << 32) | offset);
	mix64(flags);
	mix_bytes(data, size);
	next->buffer_write(buffer, offset, size, data, flags);
}

static void hashed_stream_reserve(uint32_t vertex_bytes, uint32_t index_bytes)
{
	command(OPERATION_stream_reserve);
	mix64(((uint64_t)vertex_bytes << 32) | index_bytes);
	next->stream_reserve(vertex_bytes, index_bytes);
}

/* the bytes and where they went: later draws name that buffer and offset */
static uint32_t hashed_stream(uint32_t kind, const void *data, uint32_t size, gpu_buffer *buffer)
{
	uint32_t offset;

	command(OPERATION_stream);
	mix64(kind);
	mix_bytes(data, size);
	offset = next->stream(kind, data, size, buffer);
	mix64(((uint64_t)*buffer << 32) | offset);
	return offset;
}

static gpu_shader hashed_shader_create(uint32_t stage, const char *source)
{
	gpu_shader shader;

	command(OPERATION_shader_create);
	mix64(stage);
	mix_string(source);
	shader = next->shader_create(stage, source);
	mix64(shader);
	return shader;
}

/* ---------- drawing */

static void hashed_clear(const struct gpu_clear *clear, const struct gpu_rect *rectangles, uint32_t count)
{
	command(OPERATION_clear);
	mix_bytes(clear, sizeof(*clear));
	mix_bytes(rectangles, (uint64_t)count * sizeof(*rectangles));
	next->clear(clear, rectangles, count);
}

/* the packet, and the constants and uniforms the draw reads as they are now:
the values whatever the serials say, so a draw that sees stale constants under
a current serial changes the hash */
static uint32_t hashed_draw(const struct gpu_draw *draw, const struct gpu_constant_store *constants,
	const struct gpu_uniforms *uniforms)
{
	command(OPERATION_draw);
	mix_bytes(draw, sizeof(*draw));
	part = PART_DRAW_CONSTANTS;
	mix64(constants->serial);
	mix_bytes(constants->c, sizeof(constants->c));
	part = PART_DRAW_UNIFORMS;
	mix_bytes(uniforms, sizeof(*uniforms));
	return next->draw(draw, constants, uniforms);
}

static void hashed_visibility_begin(void)
{
	command(OPERATION_visibility_begin);
	next->visibility_begin();
}

static void hashed_visibility_end(uint32_t slot)
{
	command(OPERATION_visibility_end);
	mix64(slot);
	next->visibility_end(slot);
}

/* a query */
static uint32_t hashed_visibility_result(uint32_t slot, uint32_t *samples)
{
	return next->visibility_result(slot, samples);
}

static void hashed_flush(void)
{
	command(OPERATION_flush);
	next->flush();
}

static void hashed_upscale(gpu_texture source, gpu_texture destination, const struct gpu_rect *area,
	uint32_t anti_alias)
{
	command(OPERATION_upscale);
	mix64(((uint64_t)source << 32) | destination);
	mix_bytes(area, area ? sizeof(*area) : 0);
	mix64(anti_alias);
	next->upscale(source, destination, area, anti_alias);
}

/* the shader's text; the lookup textures are the same constant tables every time, and the call does not say their
size */
static uint32_t hashed_anti_alias_prepare(uint32_t method, const char *smaa_shader, const void *area_texels,
	const void *search_texels)
{
	command(OPERATION_anti_alias_prepare);
	mix64(method);
	mix_string(smaa_shader);
	mix64((area_texels != 0) | (search_texels != 0) << 1);
	return next->anti_alias_prepare(method, smaa_shader, area_texels, search_texels);
}

static void hashed_anti_alias(uint32_t method, gpu_texture target, const struct gpu_rect *rect)
{
	command(OPERATION_anti_alias);
	mix64(((uint64_t)method << 32) | target);
	mix_bytes(rect, rect ? sizeof(*rect) : 0);
	next->anti_alias(method, target, rect);
}

/* a query, and from another thread (d3d8_device.c's vertical_blank_thread): it touches nothing here */
static uint32_t hashed_vertical_blank_wait(void)
{
	return next->vertical_blank_wait();
}

/* a query */
static uint32_t hashed_companion_size(uint32_t *width, uint32_t *height)
{
	return next->companion_size(width, height);
}

static void hashed_companion_set(gpu_texture target)
{
	command(OPERATION_companion_set);
	mix64(target);
	next->companion_set(target);
}

static void hashed_frame_start(void)
{
	command(OPERATION_frame_start);
	next->frame_start();
}

/* the frame's last command: its hash goes to the report, and the next frame starts from nothing */
static void frame_end(void)
{
	int report_second_thread = second_thread && !second_thread_reported;
	char text[PARTS * 40];
	size_t length = 0;
	unsigned index;

	if (report_second_thread)
		second_thread_reported = 1;
	text[0] = 0;
	for (index = 0; index < PARTS && length < sizeof(text); index++)
		if (part_calls[index] || (index >= OPERATION_COUNT && part_calls[OPERATION_draw]))
			length += (size_t)snprintf(text + length, sizeof(text) - length, "%s%s:%08x", length ? " " : "",
				part_names[index], (unsigned)(parts[index] ^ parts[index] >> 32));
	if (report)
		report(frame, calls, hash, report_second_thread, text);
	frame++;
	calls = 0;
	hash = 0;
	memset(parts, 0, sizeof(parts));
	memset(part_calls, 0, sizeof(part_calls));
}

static uint32_t hashed_present(gpu_texture back_buffer)
{
	uint32_t due;

	command(OPERATION_present);
	mix64(back_buffer);
	frame_end();
	due = next->present(back_buffer);
	return due;
}

static uint32_t hashed_present_stereo(const struct gpu_stereo_present *present)
{
	command(OPERATION_present_stereo);
	mix_bytes(present, sizeof(*present));
	frame_end();
	return next->present_stereo(present);
}

/* a query */
static uint32_t hashed_call_count_take(void)
{
	return next->call_count_take();
}

/* a query */
static uint32_t hashed_warm_list_read(const char *name, char *text, uint32_t size)
{
	return next->warm_list_read(name, text, size);
}

static void hashed_warm_begin(void)
{
	command(OPERATION_warm_begin);
	next->warm_begin();
}

static void hashed_pipeline_warm(const struct gpu_pipeline_description *description)
{
	command(OPERATION_pipeline_warm);
	mix_bytes(description, sizeof(*description));
	next->pipeline_warm(description);
}

static void hashed_warm_end(void)
{
	command(OPERATION_warm_end);
	next->warm_end();
}

/* a query */
static uint32_t hashed_pipeline_built_take(struct gpu_pipeline_description *description)
{
	return next->pipeline_built_take(description);
}

static void hashed_frame_context(uint32_t scene_class, uint32_t frame_rate, const char *level)
{
	command(OPERATION_frame_context);
	mix64(((uint64_t)scene_class << 32) | frame_rate);
	mix_string(level);
	next->frame_context(scene_class, frame_rate, level);
}

/* a query */
static uint32_t hashed_compiles_outstanding(void)
{
	return next->compiles_outstanding();
}

/* a snapshot is a query; the other actions change the backend's accounting */
static void hashed_compile_progress(uint32_t action, struct gpu_compile_progress *progress)
{
	if (action != GPU_COMPILE_PROGRESS_SNAPSHOT)
	{
		command(OPERATION_compile_progress);
		mix64(action);
	}
	next->compile_progress(action, progress);
}

const struct gpu_backend *gpu_call_hash_wrap(const struct gpu_backend *chosen, gpu_call_hash_report report_to)
{
	next = chosen;
	report = report_to;
	hashed = *chosen;
	hash = 0;
	calls = 0;
	frame = 0;
	memset(parts, 0, sizeof(parts));
	memset(part_calls, 0, sizeof(part_calls));
	owner_known = second_thread = second_thread_reported = 0;
	/* (a backend without an operation keeps it missing: frame_context is optional) */
#define INSTALL_F(type, name, parameters, arguments) if (chosen->name) hashed.name = hashed_##name;
#define INSTALL_P(name, parameters, arguments) if (chosen->name) hashed.name = hashed_##name;
	GPU_OPERATIONS(INSTALL_F, INSTALL_P)
#undef INSTALL_F
#undef INSTALL_P
	return &hashed;
}
