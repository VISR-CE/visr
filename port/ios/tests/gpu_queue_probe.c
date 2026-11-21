/* debug.encode_thread (port/ios/host/gpu_queue.c): a backend behind the queue receives the same commands, with the
same bytes, in the same order, as one called directly. Plays the same made-up frames both ways into
debug.gpu_call_hash (gpu_call_hash.c) and compares the frames' hashes: draws whose packets change a few bytes or
many, constants changed one register at a time, in bursts past the store's change log and under a second store,
uniforms changed whole and one value at a time, uploads bigger than a chunk, a write with no data, and every
recorded operation, each of which must reach the backend behind the queue inside a compared frame. Behind the
queue the hash sees only the recorded commands, and must match the direct run's recorded hash; in front it must
match the direct run's whole hash. Then, behind the queue, a backend that reads the constants as gpu_gl.c does
(following the store's change log) must see the game's values and serials on every draw. Built and run by tools/test_gpu_queue.py with
gpu_call_hash.c and gpu_queue.c. */

#include "gpu_call_hash.h"
#include "gpu_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_fake_backend.h"

enum { FRAMES = 60, MAXIMUM_FRAMES = 128 };

/* each frame's report from a wrapper: instance 0 (direct, or behind the queue), and instance 1 in front of it */
static struct gpu_call_hash_frame frames[MAXIMUM_FRAMES], front_frames[MAXIMUM_FRAMES];
static unsigned long frames_seen, front_seen;

static void record(const struct gpu_call_hash_frame *frame)
{
	if (frame->frame < MAXIMUM_FRAMES)
		frames[frame->frame] = *frame;
	frames_seen = frame->frame + 1;
}

static void record_front(const struct gpu_call_hash_frame *frame)
{
	if (frame->frame < MAXIMUM_FRAMES)
		front_frames[frame->frame] = *frame;
	front_seen = frame->frame + 1;
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

/* ---------- a backend that reads the constants by their change log (gpu_gl.c's program_constants), checked
against the store the draw came from */

static const struct gpu_constant_store *front_store;
static unsigned long stale_reads, stale_serials;

static uint32_t log_reader_draw(const struct gpu_draw *draw, const struct gpu_constant_store *store,
	const struct gpu_uniforms *uniforms_now)
{
	/* one reader per store the game draws from (a program's copy, as gpu_gl.c keeps one a program) */
	static float copies[2][GPU_CONSTANT_COUNT][4];
	static uint64_t serials_read[2];
	static int known[2];
	unsigned which = front_store == &stores[1], index;
	uint64_t serial;

	(void)draw, (void)uniforms_now;
	if (known[which] && store->serial >= serials_read[which] &&
		store->serial - serials_read[which] <= GPU_CONSTANT_COUNT)
	{
		for (serial = serials_read[which] + 1; serial <= store->serial; serial++)
		{
			index = store->log[serial % GPU_CONSTANT_LOG_SIZE];
			memcpy(copies[which][index], store->c[index], sizeof(copies[which][index]));
		}
	}
	else
		for (index = 0; index < GPU_CONSTANT_COUNT; index++)
			if (!known[which] || store->serial < serials_read[which] || store->serials[index] > serials_read[which])
				memcpy(copies[which][index], store->c[index], sizeof(copies[which][index]));
	known[which] = 1;
	serials_read[which] = store->serial;
	stale_reads += memcmp(copies[which], front_store->c, sizeof(copies[which])) != 0;
	stale_serials += store->serial != front_store->serial ||
		memcmp(store->serials, front_store->serials, sizeof(store->serials)) != 0;
	return 1;
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
		/* no data: the backend must still be handed the size */
		if (frame % 9 == 2)
			backend->buffer_write(buffer, 0, 48, NULL, 0);
		backend->stream_reserve(draws * (uint32_t)sizeof(vertices), 0);
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
			if (kind == 15)
			{
				/* one value: a single block of the uniforms changes */
				uniforms.serial++;
				((float *)&uniforms)[5 + random32() % 40] = (float)random32();
			}
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
				front_store = &stores[1];
				backend->draw(&draw, &stores[1], &uniforms);
			}
			else
			{
				front_store = &stores[0];
				backend->draw(&draw, &stores[0], &uniforms);
			}
			if (kind == 11)
			{
				backend->visibility_begin();
				front_store = &stores[0];
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
		/* a texture made and destroyed inside a frame, so its destroy is in a compared hash */
		if (frame % 11 == 3)
			backend->texture_destroy(backend->texture_create(&description));
		/* (as host_gpu_dispatch.c's present_wrapped does) */
		backend->stream_frame_end();
		if (frame % 5 == 4)
		{
			stereo.eye_color[0] = texture;
			stereo.fade[1] = (float)frame;
			backend->present_stereo(&stereo);
		}
		else
			backend->present(texture);
	}
	/* (nothing after the last present: every command is in a compared frame) */
}

int main(void)
{
	struct gpu_call_hash_frame direct[FRAMES];
	unsigned long frame;
	unsigned operation;
	int distinct = 0;

	play(gpu_call_hash_wrap(0, &fake, record));
	CHECK(frames_seen == FRAMES, "the direct run presented %lu frames", frames_seen);
	memcpy(direct, frames, sizeof(direct));
	for (frame = 1; frame < FRAMES; frame++)
		distinct += direct[frame].hash != direct[frame - 1].hash && direct[frame].recorded != direct[frame - 1].recorded;
	CHECK(distinct > FRAMES / 2, "the frames hash alike: the probe tests nothing");

	/* behind the queue: only the recorded commands, the same as the direct run's; every recorded operation reaches
	the backend */
	frames_seen = 0;
	memset(fake_calls, 0, sizeof(fake_calls));
	play(gpu_queue_wrap(gpu_call_hash_wrap(0, &counted, record), &counted, GPU_QUEUE_INLINE, 1, quiet));
	CHECK(frames_seen == FRAMES, "the queued run presented %lu frames", frames_seen);
	for (frame = 0; frame < FRAMES; frame++)
		CHECK(frames[frame].hash == direct[frame].recorded && frames[frame].calls == direct[frame].recorded_calls &&
			frames[frame].recorded == frames[frame].hash,
			"frame %lu: behind the queue %u commands, hash %016llx; direct %u recorded, %016llx", frame,
			frames[frame].calls, (unsigned long long)frames[frame].hash, direct[frame].recorded_calls,
			(unsigned long long)direct[frame].recorded);
	for (operation = 0; operation < GPU_OPERATION_COUNT; operation++)
		CHECK((gpu_operation_classes[operation] != GPU_CLASS_RECORDED &&
			gpu_operation_classes[operation] != GPU_CLASS_PRODUCER) || fake_calls[operation],
			"the probe never sent %s, a %s operation, through the queue", gpu_operation_names[operation],
			gpu_operation_classes[operation] == GPU_CLASS_RECORDED ? "recorded" : "producer");

	/* one run, hashed in front of the queue and behind it, as debug.gpu_call_hash does with the queue on */
	frames_seen = front_seen = 0;
	play(gpu_call_hash_wrap(1, gpu_queue_wrap(gpu_call_hash_wrap(0, &fake, record), &fake, GPU_QUEUE_INLINE, 0, quiet),
		record_front));
	CHECK(frames_seen == FRAMES && front_seen == FRAMES, "the paired run presented %lu and %lu frames", front_seen,
		frames_seen);
	for (frame = 0; frame < FRAMES; frame++)
		CHECK(front_frames[frame].hash == direct[frame].hash && front_frames[frame].recorded == frames[frame].hash &&
			frames[frame].hash == direct[frame].recorded,
			"frame %lu: in front of the queue %016llx (recorded %016llx), behind it %016llx", frame,
			(unsigned long long)front_frames[frame].hash, (unsigned long long)front_frames[frame].recorded,
			(unsigned long long)frames[frame].hash);

	/* behind the queue, the constants a log-following backend reads, and the serials it is handed */
	{
		struct gpu_backend reader = fake;

		reader.draw = log_reader_draw;
		play(gpu_queue_wrap(&reader, NULL, GPU_QUEUE_INLINE, 0, quiet));
		CHECK(!stale_reads && !stale_serials, "behind the queue %lu draws read stale constants by the change log "
			"and %lu had other serials than the game's store", stale_reads, stale_serials);
	}
	puts("gpu_queue_probe: ok");
	return 0;
}
