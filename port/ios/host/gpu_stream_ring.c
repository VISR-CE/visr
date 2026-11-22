/*
GPU_STREAM_RING.C

The producer's stream memory (gpu_stream_ring.h).
*/

#include "gpu_stream_ring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gpu_stream_ring_init(struct gpu_stream_ring *ring, unsigned set_count, const struct gpu_stream_ring_owner *owner)
{
	unsigned set;

	memset(ring, 0, sizeof(*ring));
	ring->set_count = set_count < 1 ? 1 : set_count > GPU_STREAM_RING_SETS ? GPU_STREAM_RING_SETS : set_count;
	ring->owner = *owner;
	for (set = 0; set < GPU_STREAM_RING_SETS; set++)
		atomic_init(&ring->sets[set].stamp, 0);
	atomic_init(&ring->ended, 0);
}

unsigned gpu_stream_ring_set_count(unsigned frames_in_flight, unsigned lead, int queued)
{
	return frames_in_flight + lead + (queued ? 1 : 0);
}

static void ring_log(struct gpu_stream_ring *ring, const char *line)
{
	if (ring->owner.log)
		ring->owner.log(ring->owner.context, line);
}

/* the set of the frame being streamed, reset once its last use has completed on the GPU */
static struct gpu_stream_set *current_set(struct gpu_stream_ring *ring)
{
	unsigned long frame = atomic_load_explicit(&ring->ended, memory_order_relaxed);
	struct gpu_stream_set *set = &ring->sets[frame % ring->set_count];
	uint64_t stamp;

	if (ring->ready)
		return set;
	stamp = atomic_load_explicit(&set->stamp, memory_order_acquire);
	if (stamp == GPU_STREAM_RING_PENDING)
	{
		/* (never reused: a stamp arriving later would mark the set free a lap early) */
		ring->owner.unstamped(ring->owner.context, frame);
		abort();
	}
	if (stamp > ring->owner.completed(ring->owner.context))
	{
		ring->waits++;
		ring->owner.wait(ring->owner.context, stamp);
	}
	set->chunk = 0;
	set->offset = 0;
	ring->ready = 1;
	return set;
}

void *gpu_stream_ring_room(struct gpu_stream_ring *ring, uint32_t size, uint32_t alignment, uint32_t *handle,
	uint32_t *offset)
{
	unsigned set_index = (unsigned)(atomic_load_explicit(&ring->ended, memory_order_relaxed) % ring->set_count);
	struct gpu_stream_set *set = current_set(ring);

	for (;;)
	{
		if (set->chunk < set->chunk_count)
		{
			struct gpu_stream_chunk *chunk = &set->chunks[set->chunk];
			uint64_t at = (set->offset + alignment - 1) & ~(uint64_t)(alignment - 1);

			if (at + size <= chunk->length)
			{
				set->offset = at + size;
				*handle = chunk->handle;
				*offset = (uint32_t)at;
				return chunk->contents + at;
			}
			set->chunk++;
			set->offset = 0;
			continue;
		}
		if (set->chunk_count == GPU_STREAM_RING_CHUNKS ||
			!ring->owner.grow(ring->owner.context, set_index, set->chunk_count, size, &set->chunks[set->chunk_count]))
			return NULL;
		set->chunk_count++;
	}
}

uint32_t gpu_stream_ring_copy(struct gpu_stream_ring *ring, const void *data, uint32_t size, uint32_t alignment,
	uint32_t *handle)
{
	uint32_t offset;
	void *at = gpu_stream_ring_room(ring, size, alignment, handle, &offset);

	if (!at)
		return UINT32_MAX;
	memcpy(at, data, size);
	return offset;
}

void gpu_stream_ring_reserve(struct gpu_stream_ring *ring)
{
	current_set(ring);
}

void gpu_stream_ring_frame_end(struct gpu_stream_ring *ring)
{
	unsigned long frame = atomic_load_explicit(&ring->ended, memory_order_relaxed);

	atomic_store_explicit(&ring->sets[frame % ring->set_count].stamp, GPU_STREAM_RING_PENDING, memory_order_relaxed);
	ring->ready = 0;
	/* (release: the consumer that sees the count sees the set pending) */
	atomic_store_explicit(&ring->ended, frame + 1, memory_order_release);
}

void gpu_stream_ring_presented(struct gpu_stream_ring *ring, uint64_t serial)
{
	unsigned long ended = atomic_load_explicit(&ring->ended, memory_order_acquire);

	if (ended - ring->presented > 1 + ring->owner.lead && !ring->behind++)
		ring_log(ring, "stream ring: a present found more ended frames than the lead allows (a stream_frame_end whose "
			"present never came); later stamps are a set late");
	if (ring->presented >= ended)
	{
		if (!ring->unmatched++)
			ring_log(ring, "stream ring: a present came without stream_frame_end; its streams stay in the next set");
		return;
	}
	atomic_store_explicit(&ring->sets[ring->presented % ring->set_count].stamp, serial, memory_order_release);
	ring->presented++;
}
