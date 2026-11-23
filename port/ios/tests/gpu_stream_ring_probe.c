/*
GPU_STREAM_RING_PROBE.C

The stream ring (port/ios/host/gpu_stream_ring.h) never hands out memory the
GPU may still read: every stream of a frame the GPU has not completed keeps its
bytes until it has, with the queue off (producer and consumer in step) and with
the producer two presents ahead of the consumer and three frames uncompleted.
A present without stream_frame_end, and a producer a whole ring ahead, are
reported.
*/

#include "gpu_stream_ring.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAMES 3

#define CHECK(condition, ...) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "gpu_stream_ring_probe: " __VA_ARGS__); \
			fputc('\n', stderr); \
			exit(1); \
		} \
	} while (0)

/* ---------- a GPU that completes command buffers when told to */

static uint64_t completed_serial;
static unsigned long waits_called;
static unsigned char *chunk_memory[GPU_STREAM_RING_SETS][GPU_STREAM_RING_CHUNKS];
static unsigned lines_logged;

static int grow(void *context, unsigned set, unsigned index, uint32_t size, struct gpu_stream_chunk *chunk)
{
	uint32_t length = size > 4096 ? size : 4096;

	(void)context;
	chunk_memory[set][index] = realloc(chunk_memory[set][index], length);
	chunk->handle = 1000 + set * GPU_STREAM_RING_CHUNKS + index;
	chunk->contents = chunk_memory[set][index];
	chunk->length = length;
	return chunk->contents != NULL;
}

static uint64_t completed(void *context)
{
	(void)context;
	return completed_serial;
}

static void wait_for(void *context, uint64_t serial)
{
	(void)context;
	CHECK(serial > completed_serial, "waited for serial %llu, already completed", (unsigned long long)serial);
	waits_called++;
	completed_serial = serial;
}

static void log_line(void *context, const char *line)
{
	(void)context;
	(void)line;
	lines_logged++;
}

/* the producer met an unstamped set: back to the case that expects it */
static jmp_buf unstamped_return;
static unsigned long unstamped_frame = ~0ul;

static void unstamped(void *context, unsigned long frame)
{
	(void)context;
	unstamped_frame = frame;
	longjmp(unstamped_return, 1);
}

static struct gpu_stream_ring_owner owner = {
	.grow = grow, .completed = completed, .wait = wait_for, .log = log_line, .unstamped = unstamped,
};

/* ---------- every stream still in flight, with the bytes it should still hold */

struct stream
{
	unsigned long frame;
	uint64_t serial;     /* its present's command buffer; 0 until the consumer encodes it */
	unsigned char *at;
	uint32_t size;
	unsigned char value;
};

static struct stream streams[4096];
static unsigned stream_count;
static uint64_t seed = 7;

static uint32_t random32(void)
{
	seed = seed * 6364136223846793005ull + 1442695040888963407ull;
	return (uint32_t)(seed >> 33);
}

/* drops the streams the GPU has completed and checks the rest still hold their bytes */
static void check_in_flight(void)
{
	unsigned index, kept = 0;

	for (index = 0; index < stream_count; index++)
	{
		struct stream *stream = &streams[index];
		uint32_t byte;

		if (stream->serial && stream->serial <= completed_serial)
			continue;
		for (byte = 0; byte < stream->size; byte++)
			CHECK(stream->at[byte] == stream->value, "frame %lu's stream was overwritten before the GPU completed it",
				stream->frame);
		streams[kept++] = *stream;
	}
	stream_count = kept;
}

static void stream_frame(struct gpu_stream_ring *ring, unsigned long frame)
{
	unsigned count = 5 + random32() % 40, index;

	if (frame % 3 == 0)
		gpu_stream_ring_reserve(ring);
	for (index = 0; index < count; index++)
	{
		unsigned char data[700];
		uint32_t size = 1 + random32() % (uint32_t)sizeof(data), handle, offset;
		unsigned char value = (unsigned char)(frame * 31 + index);
		struct stream *stream;

		memset(data, value, size);
		offset = gpu_stream_ring_copy(ring, data, size, 16, &handle);
		CHECK(offset != UINT32_MAX && offset % 16 == 0, "frame %lu: no room or a misaligned stream", frame);
		CHECK(stream_count < sizeof(streams) / sizeof(streams[0]), "too many streams in flight");
		stream = &streams[stream_count++];
		stream->frame = frame;
		stream->serial = 0;
		stream->at = chunk_memory[(handle - 1000) / GPU_STREAM_RING_CHUNKS][(handle - 1000) % GPU_STREAM_RING_CHUNKS] +
			offset;
		stream->size = size;
		stream->value = value;
		check_in_flight();
	}
}

/* the consumer encodes frame's present in command buffer serial */
static void present(struct gpu_stream_ring *ring, unsigned long frame, uint64_t serial)
{
	unsigned index;

	gpu_stream_ring_presented(ring, serial);
	for (index = 0; index < stream_count; index++)
		if (streams[index].frame == frame)
			streams[index].serial = serial;
}

static void reset(void)
{
	completed_serial = 0;
	waits_called = 0;
	stream_count = 0;
	lines_logged = 0;
}

int main(void)
{
	static struct gpu_stream_ring ring;
	unsigned long frame;

	/* the set count by queue mode: the queue off keeps one set per frame slot, as before the ring; the queue on one
	more */
	CHECK(gpu_stream_ring_set_count(FRAMES, 0, 0) == FRAMES, "the queue off gives %u sets, not one per frame slot",
		gpu_stream_ring_set_count(FRAMES, 0, 0));
	CHECK(gpu_stream_ring_set_count(FRAMES, 0, 1) == FRAMES + 1, "the queue on gives %u sets, not FRAMES + 1",
		gpu_stream_ring_set_count(FRAMES, 0, 1));
	CHECK(gpu_stream_ring_set_count(FRAMES, 2, 1) == FRAMES + 3, "a lead of 2 with the queue on gives %u sets",
		gpu_stream_ring_set_count(FRAMES, 2, 1));

	/* the queue off: each frame streams, ends and is presented at once; before a frame's first command the
	consumer's frame slot wait has seen frame - FRAMES complete, so the queue off's sets never wait */
	reset();
	gpu_stream_ring_init(&ring, gpu_stream_ring_set_count(FRAMES, 0, 0), &owner);
	for (frame = 0; frame < 300; frame++)
	{
		if (frame >= FRAMES && completed_serial < frame - FRAMES + 1)
			completed_serial = frame - FRAMES + 1;
		check_in_flight();
		stream_frame(&ring, frame);
		gpu_stream_ring_frame_end(&ring);
		present(&ring, frame, frame + 1);
	}
	CHECK(ring.waits == 0 && waits_called == 0, "queue off, the ring waited %lu times", ring.waits);
	CHECK(ring.unmatched == 0 && ring.behind == 0 && lines_logged == 0, "queue off, the ring logged a mismatch");

	/* the same with the queue on's sets */
	reset();
	gpu_stream_ring_init(&ring, gpu_stream_ring_set_count(FRAMES, 0, 1), &owner);
	for (frame = 0; frame < 300; frame++)
	{
		if (frame >= FRAMES && completed_serial < frame - FRAMES + 1)
			completed_serial = frame - FRAMES + 1;
		check_in_flight();
		stream_frame(&ring, frame);
		gpu_stream_ring_frame_end(&ring);
		present(&ring, frame, frame + 1);
	}
	CHECK(ring.waits == 0 && waits_called == 0, "in step, the ring waited %lu times", ring.waits);
	CHECK(ring.unmatched == 0 && ring.behind == 0 && lines_logged == 0, "in step, the ring logged a mismatch");

	/* the producer two presents ahead of the consumer, three frames uncompleted behind it: the ring waits for the
	GPU instead of overwriting, and never meets an unstamped set */
	reset();
	owner.lead = 2;
	gpu_stream_ring_init(&ring, FRAMES + 2, &owner);
	for (frame = 0; frame < 300; frame++)
	{
		stream_frame(&ring, frame);
		gpu_stream_ring_frame_end(&ring);
		if (frame >= 2)
		{
			present(&ring, frame - 2, frame - 1);
			if (frame - 1 > FRAMES && completed_serial < frame - 1 - FRAMES)
				completed_serial = frame - 1 - FRAMES;
			check_in_flight();
		}
	}
	CHECK(ring.unmatched == 0 && ring.behind == 0, "two ahead: %lu unmatched, %lu behind", ring.unmatched, ring.behind);
	owner.lead = 0;
	CHECK(ring.waits > 0 && ring.waits == waits_called, "two ahead, the ring never waited: the case tests nothing");

	/* a producer a whole ring ahead of the presents meets its own unstamped set and stops there: the set is never
	reused, so a late stamp cannot mark it free a lap early */
	reset();
	gpu_stream_ring_init(&ring, 2, &owner);
	if (!setjmp(unstamped_return))
	{
		for (frame = 0; frame < 6; frame++)
		{
			stream_frame(&ring, frame);
			gpu_stream_ring_frame_end(&ring);
		}
		CHECK(0, "a producer a ring ahead reused an unstamped set");
	}
	CHECK(unstamped_frame == 2, "the unstamped set was met at frame %lu, not 2", unstamped_frame);
	check_in_flight();

	/* a present behind more ended frames than the lead allows (a stream_frame_end whose present never came) is
	reported once */
	reset();
	gpu_stream_ring_init(&ring, FRAMES + 1, &owner);
	stream_frame(&ring, 0);
	gpu_stream_ring_frame_end(&ring);
	stream_frame(&ring, 1);
	gpu_stream_ring_frame_end(&ring);
	present(&ring, 0, 1);
	present(&ring, 1, 2);
	CHECK(ring.behind == 1 && lines_logged == 1, "a present behind its stream_frame_end went unreported");

	/* a stream larger than a chunk gets a chunk of its own; a set out of chunks says so instead of wrapping */
	reset();
	gpu_stream_ring_init(&ring, FRAMES + 1, &owner);
	{
		static unsigned char big[20000];
		uint32_t handle, offset, count;

		memset(big, 0x5a, sizeof(big));
		offset = gpu_stream_ring_copy(&ring, big, sizeof(big), 16, &handle);
		CHECK(offset == 0 && ring.sets[0].chunks[0].length >= sizeof(big), "a stream larger than a chunk got no room");
		for (count = 1; count < GPU_STREAM_RING_CHUNKS; count++)
			CHECK(gpu_stream_ring_copy(&ring, big, sizeof(big), 16, &handle) != UINT32_MAX, "the set ran out early");
		CHECK(gpu_stream_ring_copy(&ring, big, sizeof(big), 16, &handle) == UINT32_MAX,
			"a set past its last chunk still took a stream");
	}

	/* a present without stream_frame_end is reported, and stamps nothing */
	reset();
	gpu_stream_ring_init(&ring, FRAMES + 1, &owner);
	stream_frame(&ring, 0);
	gpu_stream_ring_frame_end(&ring);
	present(&ring, 0, 1);
	present(&ring, 1, 2);
	CHECK(ring.unmatched == 1 && lines_logged == 1 && ring.presented == 1, "an unmatched present went unreported");

	puts("gpu_stream_ring_probe: ok");
	return 0;
}
