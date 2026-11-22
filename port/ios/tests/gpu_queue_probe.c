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
#include "gpu_owner_check.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static unsigned lines_logged;

static void count_lines(const char *line)
{
	(void)line;
	lines_logged++;
}

/* a producer answer the backend then refuses: soft particles look possible, and an allocation fails */
static uint32_t ready_yes(gpu_texture depth, const float *values, uint32_t count)
{
	(void)depth, (void)values, (void)count;
	return 1;
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

/* a PRODUCER call (gpu_operation.h) goes straight to the backend: it never waits for the queue */
#define PRODUCER_CALL(call) \
	do \
	{ \
		unsigned long drains_before = gpu_queue_drains(); \
		call; \
		CHECK(gpu_queue_drains() == drains_before, "%s waited for the queue", #call); \
	} \
	while (0)

/* a THREAD_SAFE call never waits for the queue either: output_cap_settled is asked every frame */
#define THREAD_SAFE_CALL PRODUCER_CALL

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
	float side_values[27];
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
	memset(side_values, 0, sizeof(side_values));
	for (index = 0; index < sizeof(big); index++)
		big[index] = (unsigned char)(index * 7);

	backend->frame_context(1, 30, "bloodgulch");
	backend->warm_begin();
	pipeline.vertex_shader = backend->shader_create(0, "vertex");
	pipeline.pixel_shader = backend->shader_create(1, "pixel");
	backend->pipeline_warm(&pipeline);
	backend->warm_end();
	/* the side operations' warm-ups (gpu_side_imports.h's calls with no values) */
	(void)backend->soft_depth_warm();
	backend->ssao_warm();
	description.width = description.height = 512;
	description.levels = 1;
	texture = backend->texture_create(&description);
	buffer = backend->buffer_create(4096);
	for (frame = 0; frame < FRAMES; frame++)
	{
		uint32_t draws = 20 + random32() % 200;

		backend->frame_start();
		/* the side operations, between frames (host_graphics_apply, the benchmark, the output's changes) */
		backend->set_upscaler((int)(frame & 1));
		backend->set_crt((int)(frame & 2));
		if (frame % 6 == 1)
			backend->benchmark_realtime((int)(frame & 4));
		if (frame % 10 == 3)
		{
			backend->request_output_cap((int)frame * 10);
			backend->output_cap_commit();
			(void)backend->output(NULL, NULL, 640 + (int)frame, 480);
		}
		(void)backend->upscaler();
		THREAD_SAFE_CALL((void)backend->output_cap_settled());
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
		PRODUCER_CALL(backend->stream_reserve(draws * (uint32_t)sizeof(vertices), 0));
		for (index = 0; index < draws; index++)
		{
			uint32_t kind = random32() % 16;
			uint32_t changes, change;

			memset(vertices, (int)(index + frame), sizeof(vertices));
			PRODUCER_CALL(draw.streams[0].offset = backend->stream(0, vertices, sizeof(vertices), &draw.streams[0].buffer));
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
			/* mid-frame: soft particles' depth (values, and once with none) and SSAO */
			if (kind == 3 && index % 2)
			{
				side_values[0] = (float)frame;
				side_values[5] = (float)index;
				(void)backend->soft_depth_ready(texture + 1, side_values, 6);
				(void)backend->soft_depth_prepare(texture + 1, (frame & 8) ? NULL : side_values, 6);
				backend->draw(&draw, front_store, &uniforms);
				backend->soft_depth_finish();
			}
			if (kind == 4 && index % 3 == 0)
			{
				side_values[2] = (float)random32();
				backend->ssao_apply(texture, texture + 1, side_values, 18 + frame % 10);
				/* an output change between draws: the frame's commands so far reach the backend first */
				if (index % 6 == 0)
					(void)backend->output(NULL, NULL, 800, 600 + (int)index);
			}
		}
		backend->texture_generate_mipmaps(texture, 0);
		backend->texture_set_content(texture, 300 + frame, 200);
		backend->texture_multisample(texture, 1 + (frame & 3));
		backend->upscale(texture, texture + 1, (frame & 2) ? rects : NULL, frame & 1);
		backend->anti_alias(1, texture, (frame & 4) ? rects + 1 : NULL);
		backend->companion_size(&width, &height);
		backend->companion_set(frame & 1 ? texture : 0);
		/* a readback (a screenshot) and the host's pipeline and statistics questions mid-frame; each waits for every
		command before it */
		if (frame % 7 == 2)
		{
			unsigned char pixels[16];
			char list[32];
			struct gpu_pipeline_description built;
			struct gpu_compile_progress progress;

			(void)backend->texture_read(texture, pixels, sizeof(pixels));
			(void)backend->anti_alias_prepare(1, NULL, NULL, NULL);
			(void)backend->call_count_take();
			(void)backend->warm_list_read("bloodgulch", list, sizeof(list));
			(void)backend->pipeline_built_take(&built);
			(void)backend->compiles_outstanding();
			backend->compile_progress(0, &progress);
		}
		if (frame == FRAMES / 2)
			backend->frame_context(2, 60, NULL);
		/* a texture made and destroyed inside a frame, so its destroy is in a compared hash */
		if (frame % 11 == 3)
			backend->texture_destroy(backend->texture_create(&description));
		/* (as host_gpu_dispatch.c's present_wrapped does) */
		PRODUCER_CALL(backend->stream_frame_end());
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

/* the classes whose calls the queue handles itself (records, drains for, or makes on the producer), by name; NULL for
the others */
static const char *queue_class_name(unsigned class)
{
	switch (class)
	{
	case GPU_CLASS_RECORDED: return "RECORDED";
	case GPU_CLASS_AT_ONCE: return "AT_ONCE";
	case GPU_CLASS_QUERY: return "QUERY";
	case GPU_CLASS_PRODUCER: return "PRODUCER";
	default: return NULL;
	}
}

/* the AT_ONCE and QUERY calls that reached the backend since fake_calls was cleared */
static unsigned long calls_that_drain(void)
{
	unsigned long calls = 0;
	unsigned operation;

	for (operation = 0; operation < GPU_OPERATION_COUNT; operation++)
		if (gpu_operation_classes[operation] == GPU_CLASS_AT_ONCE || gpu_operation_classes[operation] == GPU_CLASS_QUERY)
			calls += fake_calls[operation];
	return calls;
}

/* every frame's first draw was a keyframe a stateless reader could start from (the second eye's replay) */
static void check_keyframes(const char *run)
{
	unsigned long differ, checked = gpu_queue_keyframes_checked(&differ);
	static unsigned long checked_before;

	CHECK(checked - checked_before == FRAMES && !differ,
		"the %s run checked %lu keyframes for %d frames; %lu left another state", run, checked - checked_before, FRAMES,
		differ);
	checked_before = checked;
	gpu_queue_check_keyframes(0);
}

static pthread_t consumer_thread;
static int consumer_thread_set, consumer_starts;

static void consumer_started(void)
{
	consumer_thread = pthread_self();
	consumer_thread_set = 1;
	consumer_starts++;
}

static void owner_consumer_started(void)
{
	gpu_owner_check_bind_consumer();
	consumer_started();
}

/* the encode thread's order against the game thread: a slow backend, so the consumer is behind when the game thread
calls in */
static _Atomic unsigned long slow_draws_made;
static unsigned long draws_made_at_call;
static _Atomic int slow_presented;

static void spin_seconds(double seconds)
{
	struct timespec now;
	double until;

	timespec_get(&now, TIME_UTC);
	until = (double)now.tv_sec + (double)now.tv_nsec / 1e9 + seconds;
	do
		timespec_get(&now, TIME_UTC);
	while ((double)now.tv_sec + (double)now.tv_nsec / 1e9 < until);
}

static uint32_t slow_draw(const struct gpu_draw *d, const struct gpu_constant_store *c, const struct gpu_uniforms *u)
{
	(void)d, (void)c, (void)u;
	spin_seconds(0.0005);
	atomic_fetch_add(&slow_draws_made, 1);
	return 1;
}

static void slow_frame_start(void)
{
	draws_made_at_call = atomic_load(&slow_draws_made);
}

static uint32_t slow_visibility_result(uint32_t slot, uint32_t *samples)
{
	(void)slot;
	*samples = 0;
	draws_made_at_call = atomic_load(&slow_draws_made);
	return 1;
}

static uint32_t slow_present(gpu_texture back_buffer)
{
	(void)back_buffer;
	spin_seconds(0.002);
	atomic_store(&slow_presented, 1);
	return 7;
}

int main(void)
{
	struct gpu_call_hash_frame direct[FRAMES];
	unsigned long frame;
	unsigned operation;
	unsigned long drains;
	int distinct = 0;

	/* the mode in force: debug.encode_thread when set, else display.encode_thread, which takes off and thread alone */
	{
		const char *from = NULL;

		CHECK(gpu_queue_mode_choose("", "thread", &from) == GPU_QUEUE_THREAD && !strcmp(from, "display.encode_thread"),
			"display.encode_thread = thread did not decide");
		CHECK(gpu_queue_mode_choose("", "off", NULL) == GPU_QUEUE_OFF, "display.encode_thread = off is not off");
		CHECK(gpu_queue_mode_choose("", "", NULL) == GPU_QUEUE_OFF, "an empty display.encode_thread is not off");
		CHECK(gpu_queue_mode_choose("", "inline", &from) < 0 && !strcmp(from, "display.encode_thread"),
			"display.encode_thread took the test mode");
		CHECK(gpu_queue_mode_choose("off", "thread", &from) == GPU_QUEUE_OFF && !strcmp(from, "debug.encode_thread"),
			"debug.encode_thread = off did not override the profile's thread");
		CHECK(gpu_queue_mode_choose("inline", "off", NULL) == GPU_QUEUE_INLINE, "debug.encode_thread lost the test mode");
		CHECK(gpu_queue_mode_choose("thread", "off", NULL) == GPU_QUEUE_THREAD, "debug.encode_thread = thread lost");
		CHECK(gpu_queue_mode_choose("sideways", "thread", NULL) < 0, "a bad debug.encode_thread was taken");
	}

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
	gpu_queue_check_keyframes(1);
	drains = gpu_queue_drains();
	play(gpu_queue_wrap(gpu_call_hash_wrap(0, &counted, record), &counted, GPU_QUEUE_INLINE, 1, quiet, NULL));
	/* (every AT_ONCE and QUERY call waits for the queue exactly once, by its class; nothing else does) */
	CHECK(gpu_queue_drains() - drains == calls_that_drain(), "%lu drains for %lu AT_ONCE and QUERY calls",
		gpu_queue_drains() - drains, calls_that_drain());
	check_keyframes("inline");
	CHECK(frames_seen == FRAMES, "the queued run presented %lu frames", frames_seen);
	for (frame = 0; frame < FRAMES; frame++)
		CHECK(frames[frame].hash == direct[frame].recorded && frames[frame].calls == direct[frame].recorded_calls &&
			frames[frame].recorded == frames[frame].hash,
			"frame %lu: behind the queue %u commands, hash %016llx; direct %u recorded, %016llx", frame,
			frames[frame].calls, (unsigned long long)frames[frame].hash, direct[frame].recorded_calls,
			(unsigned long long)direct[frame].recorded);
	/* (every operation the queue records, drains for or passes on by class reached the backend, so the drain count
	above cannot match because an operation was never called) */
	for (operation = 0; operation < GPU_OPERATION_COUNT; operation++)
		CHECK(!queue_class_name(gpu_operation_classes[operation]) || fake_calls[operation],
			"the probe never sent %s (class %s) through the queue", gpu_operation_names[operation],
			queue_class_name(gpu_operation_classes[operation]));

	/* one run, hashed in front of the queue and behind it, as debug.gpu_call_hash does with the queue on */
	frames_seen = front_seen = 0;
	play(gpu_call_hash_wrap(1, gpu_queue_wrap(gpu_call_hash_wrap(0, &fake, record), &fake, GPU_QUEUE_INLINE, 0, quiet, NULL),
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
		play(gpu_queue_wrap(&reader, NULL, GPU_QUEUE_INLINE, 0, quiet, NULL));
		CHECK(!stale_reads && !stale_serials, "behind the queue %lu draws read stale constants by the change log "
			"and %lu had other serials than the game's store", stale_reads, stale_serials);
	}
	/* the game is given the producer's soft-depth answer (soft_depth_ready's); a backend answer that differs is
	counted and logged once */
	CHECK(gpu_queue_answers_differ() == 0, "%lu soft-depth answers differed from a backend that agrees",
		gpu_queue_answers_differ());
	{
		struct gpu_backend disagrees = fake;
		const struct gpu_backend *queue;
		float values[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 1.0f };

		disagrees.soft_depth_ready = ready_yes;
		lines_logged = 0;
		queue = gpu_queue_wrap(&disagrees, NULL, GPU_QUEUE_INLINE, 0, count_lines, NULL);
		CHECK(queue->soft_depth_prepare(3, values, 6) == 1, "the game was not given the producer's answer");
		CHECK(queue->soft_depth_prepare(3, values, 6) == 1, "the game was not given the producer's answer");
		CHECK(gpu_queue_answers_differ() == 2 && lines_logged == 1,
			"two differing answers were counted %lu times and logged %u times", gpu_queue_answers_differ(),
			lines_logged);
	}
	/* the encode thread: the same recorded commands, made on the encode thread, every present a barrier (last: the
	thread stays, waiting, for the rest of the probe) */
	frames_seen = 0;
	memset(fake_calls, 0, sizeof(fake_calls));
	consumer_thread_set = 0;
	gpu_queue_check_keyframes(1);
	play(gpu_queue_wrap(gpu_call_hash_wrap(0, &counted, record), &counted, GPU_QUEUE_THREAD, 0, quiet, consumer_started));
	check_keyframes("threaded");
	CHECK(consumer_thread_set && !pthread_equal(consumer_thread, pthread_self()), "the encode thread did not start");
	CHECK(frames_seen == FRAMES, "the threaded run presented %lu frames", frames_seen);
	for (frame = 0; frame < FRAMES; frame++)
		CHECK(frames[frame].hash == direct[frame].recorded && frames[frame].calls == direct[frame].recorded_calls,
			"frame %lu: behind the encode thread %u commands, hash %016llx; direct %u recorded, %016llx", frame,
			frames[frame].calls, (unsigned long long)frames[frame].hash, direct[frame].recorded_calls,
			(unsigned long long)direct[frame].recorded);
	CHECK(gpu_queue_barrier_waited() >= 0.0, "a threaded present did not wait at the barrier");

	/* wrapped again, as the app's dispatch wraps (owner check under the queue): to inline and back to the encode
	thread, the old thread ends and a new one binds itself as the consumer, so every call still comes from its owner */
	{
		const struct gpu_backend *checked;
		int round;

		for (round = 0; round < 2; round++)
		{
			checked = gpu_owner_check_wrap(&counted, quiet, 0);
			play(gpu_queue_wrap(checked, checked, GPU_QUEUE_INLINE, 0, quiet, gpu_owner_check_bind_consumer));
			checked = gpu_owner_check_wrap(&counted, quiet, 0);
			consumer_thread_set = 0;
			play(gpu_queue_wrap(checked, checked, GPU_QUEUE_THREAD, 0, quiet, owner_consumer_started));
			/* (a new thread's pthread_t can equal the joined one's, so the starts are counted) */
			CHECK(consumer_thread_set && !pthread_equal(consumer_thread, pthread_self()),
				"wrapped again, round %d: no new encode thread started", round);
		}
		CHECK(consumer_starts == 3, "the encode thread started %d times for three threaded wraps", consumer_starts);
		CHECK(gpu_owner_check_violations() == 0, "wrapped again, %lu calls came from a thread other than their owner",
			gpu_owner_check_violations());
	}

	/* with the encode thread behind: the late start (frame_start) and a query each come after every record the game
	wrote before them is made, and a present returns only once it is made, with the backend's answer, the game thread
	having waited for it at the barrier */
	{
		struct gpu_backend slow = fake;
		const struct gpu_backend *queue;
		struct gpu_draw draw;
		uint32_t samples, index;

		slow.draw = slow_draw;
		slow.frame_start = slow_frame_start;
		slow.visibility_result = slow_visibility_result;
		slow.present = slow_present;
		memset(&draw, 0, sizeof(draw));
		queue = gpu_queue_wrap(&slow, NULL, GPU_QUEUE_THREAD, 0, quiet, NULL);
		for (index = 0; index < 20; index++)
			queue->draw(&draw, &stores[0], &uniforms);
		queue->frame_start();
		CHECK(draws_made_at_call == 20, "the late start came with %lu of 20 draws made", draws_made_at_call);
		for (index = 0; index < 20; index++)
			queue->draw(&draw, &stores[0], &uniforms);
		(void)queue->visibility_result(0, &samples);
		CHECK(draws_made_at_call == 40, "a query came with %lu of 40 draws made", draws_made_at_call);
		for (index = 0; index < 20; index++)
			queue->draw(&draw, &stores[0], &uniforms);
		CHECK(queue->present(0) == 7 && atomic_load(&slow_presented) && atomic_load(&slow_draws_made) == 60,
			"a present returned before it was made");
		CHECK(gpu_queue_barrier_waited() >= 0.002, "the present barrier waited %.3f ms for a 2 ms present",
			gpu_queue_barrier_waited() * 1e3);
	}

	/* the encode thread's sleep and wake: records with pauses longer than its spin, so it sleeps between them, and
	records back to back; a drain finds every one made, and a record written to a sleeping thread is made without one
	(the producer's wake) */
	{
		const struct gpu_backend *queue = gpu_queue_wrap(&counted, NULL, GPU_QUEUE_THREAD, 0, quiet, NULL);
		double pause = (double)gpu_queue_consumer_spin_ns() * 1.6 / 1e9;
		unsigned long sleeps = gpu_queue_consumer_sleeps(), before;
		struct gpu_backend slow_backend = fake;
		struct gpu_draw draw;
		int paused, tries;
		uint32_t index;

		memset(&draw, 0, sizeof(draw));
		for (paused = 1; paused >= 0; paused--)
		{
			unsigned long before = fake_calls[GPU_OPERATION_draw];

			for (index = 0; index < 3000; index++)
			{
				queue->draw(&draw, &stores[0], &uniforms);
				if (paused && index % 5 == 0)
					spin_seconds(pause);
			}
			gpu_queue_drain();
			CHECK(fake_calls[GPU_OPERATION_draw] - before == 3000, "%s pauses, %lu of 3000 draws were made",
				paused ? "with" : "without", fake_calls[GPU_OPERATION_draw] - before);
			CHECK(!paused || gpu_queue_consumer_sleeps() > sleeps, "the paused run never let the encode thread sleep");
		}
		spin_seconds(pause * 4);
		CHECK(gpu_queue_consumer_sleeps() > sleeps, "the encode thread did not sleep");
		before = atomic_load(&slow_draws_made);
		slow_backend.draw = slow_draw;
		queue = gpu_queue_wrap(&slow_backend, NULL, GPU_QUEUE_THREAD, 0, quiet, NULL);
		spin_seconds(pause * 4);
		queue->draw(&draw, &stores[0], &uniforms);
		for (tries = 0; atomic_load(&slow_draws_made) == before && tries < 1000; tries++)
			spin_seconds(0.001);
		CHECK(atomic_load(&slow_draws_made) == before + 1, "a record written to a sleeping encode thread was not made "
			"within a second without a drain");
	}
	puts("gpu_queue_probe: ok");
	return 0;
}
