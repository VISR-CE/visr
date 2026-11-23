/* A backend for probes (gpu_call_hash_probe.c, gpu_queue_probe.c) that does nothing and answers queries with
fixed values: creates count up from 1, streams count up from offset 0 in buffer 1. counted is the same backend
counting its calls by operation in fake_calls. Include it in one file. */

#ifndef __HALO_GPU_FAKE_BACKEND_H
#define __HALO_GPU_FAKE_BACKEND_H

#include "gpu.h"
#include "gpu_operation.h"


static uint32_t created;

static gpu_texture fake_texture_create(const struct gpu_texture_description *description)
{
	(void)description;
	return ++created;
}
static void fake_texture_upload(gpu_texture t, uint32_t f, uint32_t l, const void *d, uint32_t s)
{
	(void)t, (void)f, (void)l, (void)d, (void)s;
}
static void fake_texture_copy_level(gpu_texture s, gpu_texture d, uint32_t l) { (void)s, (void)d, (void)l; }
static void fake_texture_copy_rects(gpu_texture s, const struct gpu_texture_copy *c, uint32_t n)
{
	(void)s, (void)c, (void)n;
}
static void fake_texture_generate_mipmaps(gpu_texture t, uint32_t b) { (void)t, (void)b; }
static void fake_texture_destroy(gpu_texture t) { (void)t; }
static uint32_t fake_texture_read(gpu_texture t, void *p, uint32_t s)
{
	(void)t, (void)p, (void)s;
	return 1;
}
static void fake_texture_set_content(gpu_texture t, uint32_t w, uint32_t h) { (void)t, (void)w, (void)h; }
static void fake_texture_multisample(gpu_texture t, uint32_t s) { (void)t, (void)s; }
static gpu_buffer fake_buffer_create(uint32_t size)
{
	(void)size;
	return ++created;
}
static void fake_buffer_write(gpu_buffer b, uint32_t o, uint32_t s, const void *d, uint32_t f)
{
	(void)b, (void)o, (void)s, (void)d, (void)f;
}
static void fake_stream_reserve(uint32_t v, uint32_t i) { (void)v, (void)i; }
static uint32_t streamed;
static uint32_t fake_stream(uint32_t kind, const void *data, uint32_t size, gpu_buffer *buffer)
{
	uint32_t offset = streamed;

	(void)kind, (void)data;
	streamed += (size + 15) & ~15u;
	*buffer = 1;
	return offset;
}
static gpu_shader fake_shader_create(uint32_t stage, const char *source)
{
	(void)stage, (void)source;
	return ++created;
}
static void fake_clear(const struct gpu_clear *c, const struct gpu_rect *r, uint32_t n) { (void)c, (void)r, (void)n; }
static uint32_t fake_draw(const struct gpu_draw *d, const struct gpu_constant_store *c, const struct gpu_uniforms *u)
{
	(void)d, (void)c, (void)u;
	return 1;
}
static void fake_visibility_begin(void) {}
static void fake_visibility_end(uint32_t slot) { (void)slot; }
static uint32_t visibility_answer;
static uint32_t fake_visibility_result(uint32_t slot, uint32_t *samples)
{
	(void)slot;
	*samples = visibility_answer;
	return 1;
}
static void fake_flush(void) {}
static void fake_upscale(gpu_texture s, gpu_texture d, const struct gpu_rect *a, uint32_t aa)
{
	(void)s, (void)d, (void)a, (void)aa;
}
static uint32_t fake_anti_alias_prepare(uint32_t m, const char *s, const void *a, const void *t)
{
	(void)m, (void)s, (void)a, (void)t;
	return 1;
}
static void fake_anti_alias(uint32_t m, gpu_texture t, const struct gpu_rect *r) { (void)m, (void)t, (void)r; }
static uint32_t fake_vertical_blank_wait(void) { return 1; }
static uint32_t fake_companion_size(uint32_t *w, uint32_t *h)
{
	*w = *h = 0;
	return 0;
}
static void fake_companion_set(gpu_texture t) { (void)t; }
static void fake_frame_start(void) {}
static uint32_t fake_present(gpu_texture b)
{
	(void)b;
	return 0;
}
static uint32_t fake_present_stereo(const struct gpu_stereo_present *p)
{
	(void)p;
	return 0;
}
static uint32_t fake_call_count_take(void) { return 7; }
static uint32_t fake_warm_list_read(const char *n, char *t, uint32_t s)
{
	(void)n, (void)t, (void)s;
	return 0;
}
static void fake_warm_begin(void) {}
static void fake_pipeline_warm(const struct gpu_pipeline_description *d) { (void)d; }
static void fake_warm_end(void) {}
static uint32_t fake_pipeline_built_take(struct gpu_pipeline_description *d)
{
	(void)d;
	return 0;
}
static void fake_frame_context(uint32_t s, uint32_t r, const char *l) { (void)s, (void)r, (void)l; }
static uint32_t fake_compiles_outstanding(void) { return 0; }
static void fake_compile_progress(uint32_t a, struct gpu_compile_progress *p) { (void)a, (void)p; }
static void fake_initialize(uint32_t f, struct gpu_capabilities *c) { (void)f, (void)c; }
/* the host's own operations (gpu_backend.h) do nothing */
#define GPU_NOTHING_PREFIX fake_
GPU_DO_NOTHING(GPU_HOST_OPERATIONS)
#undef GPU_NOTHING_PREFIX

static const struct gpu_backend fake = {
#define FAKE_F(type, name, parameters, arguments) .name = fake_##name,
#define FAKE_P(name, parameters, arguments) .name = fake_##name,
	GPU_BACKEND_FUNCTIONS(FAKE_F, FAKE_P)
#undef FAKE_F
#undef FAKE_P
};

static unsigned long fake_calls[GPU_OPERATION_COUNT];

#define COUNTED_F(type, name, parameters, arguments) \
	static type counted_##name parameters \
	{ \
		fake_calls[GPU_OPERATION_##name]++; \
		return fake_##name arguments; \
	}
#define COUNTED_P(name, parameters, arguments) \
	static void counted_##name parameters \
	{ \
		fake_calls[GPU_OPERATION_##name]++; \
		fake_##name arguments; \
	}
GPU_BACKEND_OPERATIONS(COUNTED_F, COUNTED_P)
#undef COUNTED_F
#undef COUNTED_P

static const struct gpu_backend counted = {
#define COUNTED_ENTRY_F(type, name, parameters, arguments) .name = counted_##name,
#define COUNTED_ENTRY_P(name, parameters, arguments) .name = counted_##name,
	GPU_BACKEND_OPERATIONS(COUNTED_ENTRY_F, COUNTED_ENTRY_P)
#undef COUNTED_ENTRY_F
#undef COUNTED_ENTRY_P
	.initialize = fake_initialize,
};

#endif
