/* debug.gpu_call_hash (port/ios/host/gpu_call_hash.c): a frame's hash changes when a command is dropped, two are
swapped, a streamed byte or a constant differs under the same serial, or an argument differs; it does not change
for queries, which a queue may answer from a cache, or between two identical frames. Built and run by
tools/test_gpu_call_hash.py. */

#include "gpu_call_hash.c"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "gpu_fake_backend.h"

/* ---------- frames */

static uint64_t last_hash, last_recorded;
static uint32_t last_calls, last_recorded_calls;
static int last_second_thread;

static void record(const struct gpu_call_hash_frame *frame)
{
	last_hash = frame->hash;
	last_recorded = frame->recorded;
	last_calls = frame->calls;
	last_recorded_calls = frame->recorded_calls;
	last_second_thread = frame->second_thread;
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
	backend = gpu_call_hash_wrap(0, &fake, record);
	return frame_hash(backend, variant);
}

int main(void)
{
	uint64_t base = fresh_frame(0);
	const struct gpu_backend *backend;
	pthread_t thread;

	CHECK(base != 0, "a frame hashed to 0");
	CHECK(last_calls == 8, "a frame of eight commands counted another number");
	/* the recorded hash leaves out the calls the producer makes itself (texture_create, stream, frame_start) */
	CHECK(last_recorded_calls == 5, "a frame of five recorded commands counted another number");
	{
		uint64_t base_recorded = last_recorded;

		CHECK(fresh_frame(STREAM_BYTE) != base && last_recorded == base_recorded,
			"a streamed byte (an AT_ONCE call's) changed the recorded hash, or not the whole hash");
		CHECK(fresh_frame(DRAW_ARGUMENT) != base && last_recorded != base_recorded,
			"a changed draw argument kept the recorded hash");
	}
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
	backend = gpu_call_hash_wrap(0, &fake, record);
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
