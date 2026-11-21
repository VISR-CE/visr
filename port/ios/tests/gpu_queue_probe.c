/* debug.encode_thread (port/ios/host/gpu_queue.c): a backend behind the queue receives the same commands, with the
same bytes, in the same order, as one called directly. Plays the same made-up frames both ways into
debug.gpu_call_hash (gpu_call_hash.c) and compares the frames' hashes: draws whose packets change a few bytes or
many, constants changed one register at a time, in bursts past the store's change log and under a second store,
uniforms, uploads bigger than a chunk, and every recorded operation. Built and run by tools/test_gpu_queue.py with
gpu_call_hash.c and gpu_queue.c. */

#include "gpu_call_hash.h"
#include "gpu_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_fake_backend.h"

enum { FRAMES = 60, MAXIMUM_FRAMES = 128 };

static uint64_t hashes[MAXIMUM_FRAMES];
static uint32_t frame_calls[MAXIMUM_FRAMES];
static unsigned long frames_seen;

static void record(unsigned long frame, uint32_t calls, uint64_t hash, int second_thread, const char *parts)
{
	(void)parts;
	(void)second_thread;
	if (frame < MAXIMUM_FRAMES)
	{
		hashes[frame] = hash;
		frame_calls[frame] = calls;
	}
	frames_seen = frame + 1;
}

static void quiet(const char *line)
{
	(void)line;
}

#define CHECK(condition, ...) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "gpu_queue_probe: " __VA_ARGS__); \
			fputc('\n', stderr); \
			exit(1); \
		} \
	} while (0)

/* ---------- the front end's side: a constant store kept as d3d8_device.c's constants_store keeps it */

static struct gpu_constant_store stores[2];
static struct gpu_uniforms uniforms;
static uint64_t seed;

static uint32_t random32(void)
{
	seed = seed * 6364136223846793005ull + 1442695040888963407ull;
	return (uint32_t)(seed >> 33);
}

static void store_set(struct gpu_constant_store *store, uint32_t index, float value)
{
	if (store->c[index][0] == value)
		return;
	store->c[index][0] = value;
	store->c[index][3] = -value;
	store->serials[index] = ++store->serial;
	store->log[store->serial % GPU_CONSTANT_LOG_SIZE] = (uint8_t)index;
}

static unsigned char big[600 * 1024];

static void play(const struct gpu_backend *backend)
{
	struct gpu_draw draw;
	struct gpu_texture_description description;
	struct gpu_clear clear;
	struct gpu_rect rects[3] = { { 1, 2, 3, 4 }, { 5, 6, 7, 8 }, { 9, 10, 11, 12 } };
	struct gpu_texture_copy copies[2] = { { 3, 0, 0, 4, 4 }, { 4, 1, 1, 2, 2 } };
	struct gpu_stereo_present stereo;
	struct gpu_pipeline_description pipeline;
	unsigned char vertices[96];
	uint32_t frame, index, samples, width, height;
	gpu_buffer buffer;
	gpu_texture texture;

	seed = 12345;
	created = streamed = 0;
	memset(stores, 0, sizeof(stores));
	memset(&uniforms, 0, sizeof(uniforms));
	memset(&draw, 0, sizeof(draw));
	memset(&description, 0, sizeof(description));
	memset(&clear, 0, sizeof(clear));
	memset(&stereo, 0, sizeof(stereo));
	memset(&pipeline, 0, sizeof(pipeline));
	for (index = 0; index < sizeof(big); index++)
		big[index] = (unsigned char)(index * 7);

	backend->frame_context(1, 30, "bloodgulch");
	backend->warm_begin();
	pipeline.vertex_shader = backend->shader_create(0, "vertex");
	pipeline.pixel_shader = backend->shader_create(1, "pixel");
	backend->pipeline_warm(&pipeline);
	backend->warm_end();
	description.width = description.height = 512;
	description.levels = 1;
	texture = backend->texture_create(&description);
	buffer = backend->buffer_create(4096);
	for (frame = 0; frame < FRAMES; frame++)
	{
		uint32_t draws = 20 + random32() % 200;

		backend->frame_start();
		clear.color_target = texture;
		clear.flags = GPU_CLEAR_COLOR | GPU_CLEAR_DEPTH;
		clear.color = frame;
		backend->clear(&clear, (frame & 1) ? rects : NULL, (frame & 1) ? 3 : 0);
		if (frame % 7 == 0)
		{
			big[frame] ^= 0x5a;
			backend->texture_upload(texture, 0, 0, big, sizeof(big));
		}
		backend->buffer_write(buffer, frame * 8, 64, big + frame, frame & 3);
		for (index = 0; index < draws; index++)
		{
			uint32_t kind = random32() % 16;
			uint32_t changes, change;

			memset(vertices, (int)(index + frame), sizeof(vertices));
			draw.streams[0].offset = backend->stream(0, vertices, sizeof(vertices), &draw.streams[0].buffer);
			draw.streams[0].stride = 24;
			draw.count = 3 + random32() % 30;
			if (kind < 4)
				draw.stages[random32() % 4].texture = random32() % 50;
			if (kind == 5)
				draw.blend.color = random32();
			if (kind == 6)
				draw.constant_values[random32() % 16][random32() % 4] = (float)random32();
			if (kind == 7)
				memset(&draw.attributes, (int)random32(), sizeof(draw.attributes));
			/* constants: a few registers, sometimes a burst longer than the change log */
			changes = kind == 8 ? 1500 : random32() % 6;
			for (change = 0; change < changes; change++)
				store_set(&stores[0], random32() % GPU_CONSTANT_COUNT, (float)(random32() % 1000));
			if (kind == 9)
			{
				uniforms.serial++;
				uniforms.pad[0] = random32();
				memset(&uniforms, (int)random32(), sizeof(uniforms) - sizeof(uniforms.serial) - sizeof(uniforms.pad));
			}
			/* now and then a draw from a second store, and back */
			if (kind == 10)
			{
				store_set(&stores[1], random32() % GPU_CONSTANT_COUNT, (float)random32());
				backend->draw(&draw, &stores[1], &uniforms);
			}
			else
				backend->draw(&draw, &stores[0], &uniforms);
			if (kind == 11)
			{
				backend->visibility_begin();
				backend->draw(&draw, &stores[0], &uniforms);
				backend->visibility_end(1 + random32() % 100);
				backend->visibility_result(1, &samples);
			}
			if (kind == 12)
				backend->texture_copy_rects(texture, copies, 2);
			if (kind == 13)
				backend->texture_copy_level(texture, texture + 1, 1);
			if (kind == 14)
				backend->flush();
		}
		backend->texture_generate_mipmaps(texture, 0);
		backend->texture_set_content(texture, 300 + frame, 200);
		backend->texture_multisample(texture, 1 + (frame & 3));
		backend->upscale(texture, texture + 1, (frame & 2) ? rects : NULL, frame & 1);
		backend->anti_alias(1, texture, (frame & 4) ? rects + 1 : NULL);
		backend->companion_size(&width, &height);
		backend->companion_set(frame & 1 ? texture : 0);
		if (frame == FRAMES / 2)
			backend->frame_context(2, 60, NULL);
		if (frame % 5 == 4)
		{
			stereo.eye_color[0] = texture;
			stereo.fade[1] = (float)frame;
			backend->present_stereo(&stereo);
		}
		else
			backend->present(texture);
	}
	backend->texture_destroy(texture);
}

int main(void)
{
	uint64_t direct[FRAMES];
	uint32_t direct_calls[FRAMES];
	unsigned long frame;
	int distinct = 0;

	play(gpu_call_hash_wrap(&fake, record));
	CHECK(frames_seen == FRAMES, "the direct run presented %lu frames", frames_seen);
	memcpy(direct, hashes, sizeof(direct));
	memcpy(direct_calls, frame_calls, sizeof(direct_calls));
	for (frame = 1; frame < FRAMES; frame++)
		distinct += direct[frame] != direct[frame - 1];
	CHECK(distinct > FRAMES / 2, "the frames hash alike: the probe tests nothing");

	frames_seen = 0;
	play(gpu_queue_wrap(gpu_call_hash_wrap(&fake, record), GPU_QUEUE_INLINE, 1, quiet));
	CHECK(frames_seen == FRAMES, "the queued run presented %lu frames", frames_seen);
	for (frame = 0; frame < FRAMES; frame++)
		CHECK(hashes[frame] == direct[frame] && frame_calls[frame] == direct_calls[frame],
			"frame %lu: queued %u commands, hash %016llx; direct %u, %016llx", frame, frame_calls[frame],
			(unsigned long long)hashes[frame], direct_calls[frame], (unsigned long long)direct[frame]);
	puts("gpu_queue_probe: ok");
	return 0;
}
