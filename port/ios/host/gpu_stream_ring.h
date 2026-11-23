/*
GPU_STREAM_RING.H

The memory a backend's stream calls (gpu.h's gpu_stream) copy into: a ring of
sets, one per frame, each a list of chunks that grows to the frame's peak and
stays. It belongs to the caller of stream, the producer (the game thread, and
with debug.encode_thread the queue's writer), never to the code that encodes
draws, so streaming never waits for the queue.

A frame's set ends at stream_frame_end (gpu_backend.h), which the host calls
once before every present. The consumer, encoding that present, stamps the set
with the serial of the command buffer that carries it. The producer moves to a
set again only once its stamp has completed. A set met before it was stamped
is never reused: the owner's unstamped callback is called and must not return
(with no consumer thread the stamp can never come; a consumer thread would
wait for it instead).

Threads: the producer calls gpu_stream_ring_room, _copy, _reserve and
_frame_end; the consumer calls gpu_stream_ring_presented. Each set's stamp and
the count of ended frames are the only state both touch.
*/

#ifndef __HALO_GPU_STREAM_RING_H
#define __HALO_GPU_STREAM_RING_H

#include <stdatomic.h>
#include <stdint.h>

enum
{
	GPU_STREAM_RING_CHUNKS = 64,
	GPU_STREAM_RING_SETS = 8,
};

/* a set's stamp before its present is encoded */
#define GPU_STREAM_RING_PENDING UINT64_MAX

struct gpu_stream_chunk
{
	uint32_t handle;
	unsigned char *contents;
	uint64_t length;
};

struct gpu_stream_set
{
	struct gpu_stream_chunk chunks[GPU_STREAM_RING_CHUNKS];
	unsigned chunk_count, chunk;
	uint64_t offset;
	/* 0 (never used), GPU_STREAM_RING_PENDING, or the serial of the command buffer that carried its present */
	_Atomic uint64_t stamp;
};

/* what the backend supplies */
struct gpu_stream_ring_owner
{
	/* makes chunk index of set, at least size bytes, filling *chunk; 0 when it cannot */
	int (*grow)(void *context, unsigned set, unsigned index, uint32_t size, struct gpu_stream_chunk *chunk);
	/* the latest command buffer serial the GPU completed */
	uint64_t (*completed)(void *context);
	/* returns once serial has completed */
	void (*wait)(void *context, uint64_t serial);
	/* a finished line, no newline */
	void (*log)(void *context, const char *line);
	/* the producer met frame's set before its present was encoded; must not return */
	void (*unstamped)(void *context, unsigned long frame);
	void *context;
	/* frames the producer may run ahead of the consumer's presents (0: the present is a barrier) */
	unsigned lead;
};

struct gpu_stream_ring
{
	struct gpu_stream_set sets[GPU_STREAM_RING_SETS];
	unsigned set_count;
	struct gpu_stream_ring_owner owner;
	/* the producer's: the set of frame `ended` is ready to stream into */
	int ready;
	_Atomic unsigned long ended;
	/* the consumer's */
	unsigned long presented;
	/* waits for the GPU, presents with no ended frame to stamp, presents that found more ended frames than the
	lead allows (a stream_frame_end whose present never came) */
	unsigned long waits, unmatched, behind;
};

void gpu_stream_ring_init(struct gpu_stream_ring *ring, unsigned set_count, const struct gpu_stream_ring_owner *owner);

/* the sets a backend gives its ring: one per frame in flight and per frame of lead, and with the encode queue on
(queued) one more, for the frame recorded while the one before it is still being encoded. With the queue off the
ring holds no more streams than the frame slots did before it */
unsigned gpu_stream_ring_set_count(unsigned frames_in_flight, unsigned lead, int queued);

/* producer: room for size bytes at alignment (a power of two) in the frame's set; returns where to write them and
sets *handle and *offset, or NULL when the set is full (GPU_STREAM_RING_CHUNKS chunks) or a chunk cannot be made */
void *gpu_stream_ring_room(struct gpu_stream_ring *ring, uint32_t size, uint32_t alignment, uint32_t *handle,
	uint32_t *offset);

/* producer: copies size bytes as gpu_stream does; returns the offset, UINT32_MAX when there is no room */
uint32_t gpu_stream_ring_copy(struct gpu_stream_ring *ring, const void *data, uint32_t size, uint32_t alignment,
	uint32_t *handle);

/* producer: the frame's set is made ready now (gpu_stream_reserve), waiting for the GPU if it must */
void gpu_stream_ring_reserve(struct gpu_stream_ring *ring);

/* producer: the frame's streams end; its set waits for the consumer's stamp */
void gpu_stream_ring_frame_end(struct gpu_stream_ring *ring);

/* consumer: the next ended frame's present is in the command buffer with this serial */
void gpu_stream_ring_presented(struct gpu_stream_ring *ring, uint64_t serial);

#endif
