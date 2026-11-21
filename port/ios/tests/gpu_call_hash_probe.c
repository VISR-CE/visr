/* debug.gpu_call_hash (port/ios/host/gpu_call_hash.c): a frame's hash changes when a command is dropped, two are
swapped, a streamed byte or a constant differs under the same serial, or an argument differs; it does not change
for queries, which a queue may answer from a cache, or between two identical frames. Built and run by
tools/test_gpu_call_hash.py. */

#include "gpu_call_hash.c"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------- a backend that does nothing and answers queries with fixed values */

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

static const struct gpu_backend fake = {
#define FAKE_F(type, name, parameters, arguments) .name = fake_##name,
#define FAKE_P(name, parameters, arguments) .name = fake_##name,
	GPU_FUNCTIONS(FAKE_F, FAKE_P)
#undef FAKE_F
#undef FAKE_P
};

/* ---------- frames */

static uint64_t last_hash;
static uint32_t last_calls;
static int last_second_thread;

static void record(unsigned long frame_number, uint32_t frame_calls, uint64_t frame_hash, int other_thread)
{
	(void)frame_number;
	last_hash = frame_hash;
	last_calls = frame_calls;
	last_second_thread = other_thread;
}

enum
{
	DROP_DRAW = 1,
	SWAP_DRAWS = 2,
	STREAM_BYTE = 4,
	STALE_CONSTANT = 8,
	DRAW_ARGUMENT = 16,
	EXTRA_QUERIES = 32,
	UPLOAD_BYTE = 64,
};

static struct gpu_constant_store store;
static struct gpu_uniforms uniforms;

/* a frame the front end might send, changed as variant says; its hash */
static uint64_t frame_hash(const struct gpu_backend *backend, unsigned variant)
{
	struct gpu_draw first, second;
	unsigned char vertices[40];
	unsigned char texels[16];
	struct gpu_clear clear;
	gpu_buffer buffer;
	uint32_t samples, width, height, index;
	struct gpu_texture_description description;

	memset(&first, 0, sizeof(first));
	memset(&second, 0, sizeof(second));
	memset(&clear, 0, sizeof(clear));
	memset(&description, 0, sizeof(description));
	for (index = 0; index < sizeof(vertices); index++)
		vertices[index] = (unsigned char)index;
	for (index = 0; index < sizeof(texels); index++)
		texels[index] = (unsigned char)(3 * index);
	if (variant & STREAM_BYTE)
		vertices[17] ^= 1;
	if (variant & UPLOAD_BYTE)
		texels[5] ^= 1;
	memset(&store, 0, sizeof(store));
	memset(&uniforms, 0, sizeof(uniforms));
	store.serial = 10;
	store.c[4][1] = 2.0f;

	backend->frame_start();
	description.type = 1;
	description.width = description.height = 2;
	description.levels = 1;
	first.color_target = backend->texture_create(&description);
	backend->texture_upload(first.color_target, 0, 0, texels, sizeof(texels));
	clear.color_target = first.color_target;
	clear.flags = GPU_CLEAR_COLOR;
	backend->clear(&clear, NULL, 0);
	first.streams[0].offset = backend->stream(0, vertices, sizeof(vertices), &buffer);
	first.streams[0].buffer = buffer;
	first.count = 3;
	second = first;
	second.count = (variant & DRAW_ARGUMENT) ? 7 : 6;
	if (variant & EXTRA_QUERIES)
	{
		backend->visibility_result(3, &samples);
		backend->companion_size(&width, &height);
		backend->call_count_take();
		backend->compile_progress(GPU_COMPILE_PROGRESS_SNAPSHOT, NULL);
		backend->vertical_blank_wait();
	}
	if (variant & SWAP_DRAWS)
	{
		backend->draw(&second, &store, &uniforms);
		backend->draw(&first, &store, &uniforms);
	}
	else
	{
		backend->draw(&first, &store, &uniforms);
		if (variant & STALE_CONSTANT)
			store.c[4][1] = 3.0f;      /* a register moved, the serial did not */
		if (!(variant & DROP_DRAW))
			backend->draw(&second, &store, &uniforms);
	}
	backend->present(first.color_target);
	return last_hash;
}

static void *frame_on_other_thread(void *backend)
{
	frame_hash(backend, 0);
	return NULL;
}

#define CHECK(condition, message) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "gpu_call_hash_probe: %s\n", message); \
			exit(1); \
		} \
	} while (0)

/* each frame from the same backend state: handles restart */
static uint64_t fresh_frame(unsigned variant)
{
	const struct gpu_backend *backend;

	created = streamed = 0;
	backend = gpu_call_hash_wrap(&fake, record);
	return frame_hash(backend, variant);
}

int main(void)
{
	uint64_t base = fresh_frame(0);
	const struct gpu_backend *backend;
	pthread_t thread;

	CHECK(base != 0, "a frame hashed to 0");
	CHECK(last_calls == 8, "a frame of eight commands counted another number");
	CHECK(fresh_frame(0) == base, "the same frame hashed differently");
	CHECK(fresh_frame(EXTRA_QUERIES) == base, "queries changed the hash");
	CHECK(fresh_frame(DROP_DRAW) != base, "a dropped draw kept the hash");
	CHECK(fresh_frame(SWAP_DRAWS) != base, "swapped draws kept the hash");
	CHECK(fresh_frame(STREAM_BYTE) != base, "a changed streamed byte kept the hash");
	CHECK(fresh_frame(UPLOAD_BYTE) != base, "a changed uploaded byte kept the hash");
	CHECK(fresh_frame(STALE_CONSTANT) != base, "a constant changed under the same serial kept the hash");
	CHECK(fresh_frame(DRAW_ARGUMENT) != base, "a changed draw argument kept the hash");

	/* the next frame starts over: two identical frames in a row hash alike */
	created = streamed = 0;
	backend = gpu_call_hash_wrap(&fake, record);
	frame_hash(backend, 0);
	created = streamed = 0;
	CHECK(frame_hash(backend, 0) == base, "the second of two identical frames hashed differently");
	CHECK(!last_second_thread, "one thread was reported as two");

	/* a command from another thread is reported once */
	CHECK(pthread_create(&thread, NULL, frame_on_other_thread, (void *)backend) == 0, "no thread");
	pthread_join(thread, NULL);
	CHECK(last_second_thread, "a command from a second thread was not reported");
	frame_hash(backend, 0);
	CHECK(!last_second_thread, "the second thread was reported twice");
	puts("gpu_call_hash_probe: ok");
	return 0;
}
