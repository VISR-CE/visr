/* debug.stereo_replay (port/ios/host/gpu_queue.c): a span of records is made again into eye 1's targets with eye 1's
constants. Plays one made-up eye pass into a backend that logs what it is handed, inline and on the encode thread,
and checks the calls in order: the span's calls as written (eye-1-only records skipped), then the replay's, with the
texture map applied, eye-0-only records skipped, eye 1's constant overrides held over eye 0's stores (a whole
constant set included) until they end, and a replay-only serial; the span crosses chunks, so its finished chunks must
be kept for the replay. A buffer rewritten after a replayed draw read it makes the span unreplayable, a write the
backend marks unused does not. Verify mode compares the replay's calls with the live eye's: equal calls pass, a
differing constant is counted; a draw reading streamed bytes is compared by those bytes, so the live eye's own copy at
another ring offset matches and different bytes do not. Built and run by tools/test_gpu_queue.py with gpu_queue.c. */

#include "gpu_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_fake_backend.h"

#define CHECK(condition, ...) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "gpu_queue_replay_probe: " __VA_ARGS__); \
			fputc('\n', stderr); \
			exit(1); \
		} \
	} while (0)

struct made
{
	uint32_t operation;
	gpu_texture color, depth, stage;
	float c0, c1;
	uint64_t serial;
};

static struct made made[256];
static unsigned made_count;

static uint32_t log_draw(const struct gpu_draw *draw, const struct gpu_constant_store *constants,
	const struct gpu_uniforms *uniforms)
{
	(void)uniforms;
	if (made_count < 256)
		made[made_count++] = (struct made){ GPU_OPERATION_draw, draw->color_target, draw->depth_target,
			draw->stages[0].texture, constants->c[0][0], constants->c[1][0], constants->serial };
	return 1;
}

static void log_clear(const struct gpu_clear *clear, const struct gpu_rect *rectangles, uint32_t count)
{
	(void)rectangles, (void)count;
	if (made_count < 256)
		made[made_count++] = (struct made){ GPU_OPERATION_clear, clear->color_target, clear->depth_target, 0, 0, 0, 0 };
}

static void quiet(const char *line)
{
	(void)line;
}

static struct gpu_constant_store store, other_store;
static struct gpu_uniforms uniforms;
static const struct gpu_backend *queue;

static void store_set(struct gpu_constant_store *into, uint32_t index, float value)
{
	if (into->c[index][0] == value)
		return;
	into->c[index][0] = value;
	into->serials[index] = ++into->serial;
	into->log[into->serial % GPU_CONSTANT_LOG_SIZE] = (uint8_t)index;
}

static void draw(gpu_texture color, gpu_texture stage, gpu_buffer buffer, const struct gpu_constant_store *from)
{
	struct gpu_draw packet;

	memset(&packet, 0, sizeof(packet));
	packet.color_target = color;
	packet.depth_target = color + 2;
	packet.stages[0].texture = stage;
	packet.streams[0].buffer = buffer;
	packet.count = 3;
	queue->draw(&packet, from, &uniforms);
}

/* an indexed draw of streamed vertices and indices, each eye streaming its own copy (the ring offsets differ) */
static void streamed_draw(gpu_texture color, float shift, const struct gpu_constant_store *from)
{
	const uint16_t indices[3] = { 2, 3, 4 };
	float vertices[5][2];
	struct gpu_draw packet;
	gpu_buffer buffer;
	int vertex;

	for (vertex = 0; vertex < 5; vertex++)
	{
		vertices[vertex][0] = (float)vertex + shift;
		vertices[vertex][1] = 1.0f;
	}
	memset(&packet, 0, sizeof(packet));
	packet.color_target = color;
	packet.depth_target = color + 2;
	packet.streams[0].offset = queue->stream(GPU_STREAM_KIND_VERTEX, vertices, sizeof(vertices), &buffer);
	packet.streams[0].buffer = buffer;
	packet.streams[0].stride = sizeof(vertices[0]);
	packet.index_offset = queue->stream(GPU_STREAM_KIND_INDEX, indices, sizeof(indices), &packet.index_buffer);
	packet.primitive = GPU_PRIMITIVE_TRIANGLES;
	packet.count = 3;
	queue->draw(&packet, from, &uniforms);
}

static void clear(gpu_texture color)
{
	struct gpu_clear request;

	memset(&request, 0, sizeof(request));
	request.color_target = color;
	request.flags = GPU_CLEAR_COLOR;
	queue->clear(&request, NULL, 0);
}

static const struct gpu_queue_eye_map map = { 2, { 10, 12 }, { 11, 13 } };
static unsigned char big[300 * 1024];

static void expect(unsigned index, uint32_t operation, gpu_texture color, gpu_texture stage, float c0, float c1,
	int replay_serial, const char *run, const char *what)
{
	CHECK(index < made_count, "%s: %s is missing (%u calls)", run, what, made_count);
	CHECK(made[index].operation == operation && made[index].color == color &&
		(operation != GPU_OPERATION_draw || (made[index].stage == stage && made[index].c0 == c0 &&
		made[index].c1 == c1 && made[index].depth == color + 2 &&
		(made[index].serial >= (1ull << 62)) == (replay_serial != 0))),
		"%s: call %u (%s) is operation %u into %u, stage %u, c0 %g, c1 %g, serial %llx", run, index, what,
		made[index].operation, made[index].color, made[index].stage, made[index].c0, made[index].c1,
		(unsigned long long)made[index].serial);
}

static void run(int mode, const char *name)
{
	static struct gpu_backend logging;
	struct gpu_queue_eye_statistics before, after;
	const float eye_1_c0[4] = { 20.0f, 0.0f, 0.0f, 0.0f };
	struct gpu_stereo_present present;
	unsigned index;

	logging = fake;
	logging.draw = log_draw;
	logging.clear = log_clear;
	queue = gpu_queue_wrap(&logging, NULL, mode, 0, quiet, NULL);
	memset(&store, 0, sizeof(store));
	memset(&other_store, 0, sizeof(other_store));
	memset(&present, 0, sizeof(present));
	gpu_queue_eye_statistics(&before);

	/* ---------- a replayed span */
	made_count = 0;
	store_set(&store, 0, 1.0f);
	draw(10, 0, 1, &store);
	CHECK(gpu_queue_eye_span_begin(0), "%s: no span opened", name);
	store_set(&store, 0, 2.0f);
	gpu_queue_eye_constants(0, 1, eye_1_c0);
	draw(10, 0, 1, &store);
	store_set(&store, 1, 3.0f);
	draw(10, 0, 1, &store);
	/* (a whole constant set: another store) */
	store_set(&other_store, 0, 9.0f);
	draw(10, 0, 1, &other_store);
	gpu_queue_eye_only(GPU_QUEUE_EYE_0);
	draw(10, 50, 1, &store);
	gpu_queue_eye_only(GPU_QUEUE_EYE_1);
	draw(11, 51, 1, &store);
	gpu_queue_eye_only(GPU_QUEUE_EYE_BOTH);
	gpu_queue_eye_constants_end(0, 1);
	store_set(&store, 0, 4.0f);
	draw(10, 0, 1, &store);
	/* (bigger than what is left of a chunk: the span crosses into the next) */
	queue->texture_upload(77, 0, 0, big, sizeof(big));
	clear(10);
	draw(10, 0, 2, &store);
	CHECK(gpu_queue_eye_span_end(&map) == 1, "%s: the span is not replayed", name);
	queue->present_stereo(&present);
	gpu_queue_drain();

	index = 0;
	expect(index++, GPU_OPERATION_draw, 10, 0, 1, 0, 0, name, "the draw before the span");
	expect(index++, GPU_OPERATION_draw, 10, 0, 2, 0, 0, name, "eye 0's first draw");
	expect(index++, GPU_OPERATION_draw, 10, 0, 2, 3, 0, name, "eye 0's second draw");
	expect(index++, GPU_OPERATION_draw, 10, 0, 9, 0, 0, name, "eye 0's draw from another store");
	expect(index++, GPU_OPERATION_draw, 10, 50, 2, 3, 0, name, "eye 0's own draw");
	expect(index++, GPU_OPERATION_draw, 10, 0, 4, 3, 0, name, "eye 0's draw after the override");
	expect(index++, GPU_OPERATION_clear, 10, 0, 0, 0, 0, name, "eye 0's clear");
	expect(index++, GPU_OPERATION_draw, 10, 0, 4, 3, 0, name, "eye 0's last draw");
	expect(index++, GPU_OPERATION_draw, 11, 0, 20, 0, 1, name, "the replay's first draw");
	expect(index++, GPU_OPERATION_draw, 11, 0, 20, 3, 1, name, "the replay's second draw");
	expect(index++, GPU_OPERATION_draw, 11, 0, 20, 0, 1, name, "the replay's draw from another store");
	expect(index++, GPU_OPERATION_draw, 11, 51, 20, 3, 1, name, "eye 1's own draw");
	expect(index++, GPU_OPERATION_draw, 11, 0, 4, 3, 1, name, "the replay's draw after the override");
	expect(index++, GPU_OPERATION_clear, 11, 0, 0, 0, 0, name, "the replay's clear");
	expect(index++, GPU_OPERATION_draw, 11, 0, 4, 3, 1, name, "the replay's last draw");
	CHECK(made_count == index, "%s: %u calls, expected %u", name, made_count, index);

	/* ---------- a buffer a replayed draw read, rewritten: not replayable; a write marked unused is */
	made_count = 0;
	CHECK(gpu_queue_eye_span_begin(0), "%s: no second span", name);
	draw(10, 0, 5, &store);
	queue->buffer_write(6, 0, 4, big, 0);
	queue->buffer_write(5, 0, 4, big, GPU_WRITE_UNUSED);
	CHECK(gpu_queue_eye_span_end(&map) == 1, "%s: an unread or unused write stopped the replay", name);
	CHECK(gpu_queue_eye_span_begin(0), "%s: no third span", name);
	draw(10, 0, 5, &store);
	queue->buffer_write(5, 0, 4, big, 0);
	CHECK(gpu_queue_eye_span_end(&map) == 0, "%s: a rewritten buffer was replayed", name);
	queue->present_stereo(&present);
	gpu_queue_drain();
	CHECK(made_count == 3 && made[1].color == 11 && made[2].color == 10, "%s: %u calls around an unreplayable span", name,
		made_count);

	/* ---------- verify: the replay against the live eye, equal and then not */
	for (index = 0; index < 2; index++)
	{
		CHECK(gpu_queue_eye_span_begin(1), "%s: no verify span", name);
		store_set(&store, 0, 2.0f);
		gpu_queue_eye_constants(0, 1, eye_1_c0);
		draw(10, 0, 1, &store);
		gpu_queue_eye_only(GPU_QUEUE_EYE_1);
		draw(11, 51, 1, &store);
		gpu_queue_eye_only(GPU_QUEUE_EYE_BOTH);
		gpu_queue_eye_constants_end(0, 1);
		CHECK(gpu_queue_eye_span_end(&map) == 1, "%s: a verify span is not replayed", name);
		/* the live eye: what the replay makes, and the second time a constant it does not */
		store_set(&store, 0, index ? 21.0f : 20.0f);
		draw(11, 0, 1, &store);
		draw(11, 51, 1, &store);
		gpu_queue_eye_live_end();
		queue->present_stereo(&present);
		gpu_queue_drain();
		gpu_queue_eye_statistics(&after);
		CHECK(after.verified - before.verified == index + 1, "%s: %lu spans verified", name,
			after.verified - before.verified);
		CHECK((after.verify_differ - before.verify_differ == 0) == (index == 0),
			"%s: verify counted %lu differing calls in pass %u", name, after.verify_differ - before.verify_differ, index);
	}
	/* ---------- verify by streamed bytes: the live eye's copy at other offsets matches, other bytes do not */
	for (index = 0; index < 2; index++)
	{
		struct gpu_queue_eye_statistics streamed_before;

		gpu_queue_eye_statistics(&streamed_before);
		CHECK(gpu_queue_eye_span_begin(1), "%s: no streamed verify span", name);
		streamed_draw(10, 0.0f, &store);
		CHECK(gpu_queue_eye_span_end(&map) == 1, "%s: a streamed verify span is not replayed", name);
		streamed_draw(11, index ? 0.5f : 0.0f, &store);
		gpu_queue_eye_live_end();
		queue->present_stereo(&present);
		gpu_queue_drain();
		gpu_queue_eye_statistics(&after);
		CHECK(after.verified - streamed_before.verified == 1, "%s: the streamed span was not verified", name);
		CHECK((after.verify_differ - streamed_before.verify_differ == 0) == (index == 0),
			"%s: verify counted %lu differing calls for streamed draws in pass %u", name,
			after.verify_differ - streamed_before.verify_differ, index);
	}
	CHECK(after.spans - before.spans == 7 && after.replayed - before.replayed == 2 &&
		after.unreplayable - before.unreplayable == 1, "%s: %lu spans, %lu replayed, %lu not", name,
		after.spans - before.spans, after.replayed - before.replayed, after.unreplayable - before.unreplayable);
	/* the queue off opens no span */
	if (mode == GPU_QUEUE_INLINE)
	{
		gpu_queue_wrap(&logging, NULL, GPU_QUEUE_OFF, 0, quiet, NULL);
		CHECK(!gpu_queue_eye_span_begin(0), "a span opened with the queue off");
	}
}

int main(void)
{
	run(GPU_QUEUE_INLINE, "inline");
	/* (last: the encode thread stays) */
	run(GPU_QUEUE_THREAD, "threaded");
	puts("gpu_queue_replay_probe: ok");
	return 0;
}
