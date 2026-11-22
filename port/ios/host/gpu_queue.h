/*
GPU_QUEUE.H

debug.encode_thread: the backend's commands go through a queue of records
before they reach it. The game thread writes each command (its arguments and
the bytes they point to) as a record; a consumer reads the records in order
and makes the same calls on the backend. Draws are written as the bytes that
changed since the previous draw, and constants as the registers that changed
since the previous draw (the store's change log), so a record is usually much
smaller than the call it stands for.

Modes:
- GPU_QUEUE_INLINE: the game thread consumes each record as soon as it is
  written. Nothing moves to another thread; it measures what recording
  costs and shows the records carry everything the backend needs.
- GPU_QUEUE_THREAD: the encode thread consumes the records as they are
  published, so the backend's encoding and its frame-slot wait run beside
  the game's translation. Every present is a barrier: it returns once the
  encode thread has made it, so input is read where it is with the queue off
  and no frame is queued ahead (encode spec revision 4). A draw's answer is
  1 (the backend's is not known yet).

Calls go by their class (gpu_operation.h): RECORDED ones through the queue,
the others straight to the backend (AT_ONCE and QUERY after the queue is
consumed up to them).
*/

#ifndef __HALO_GPU_QUEUE_H
#define __HALO_GPU_QUEUE_H

#include "gpu_backend.h"

enum { GPU_QUEUE_OFF, GPU_QUEUE_INLINE, GPU_QUEUE_THREAD };

/* the setting's value ("off", "inline", "thread"), or -1 for one it does not know */
int gpu_queue_mode_parse(const char *value);

/* chosen, behind the queue: it receives the RECORDED commands. beneath (NULL: chosen) receives the calls the
producer makes itself; it is chosen without the wrappers that must see only recorded commands (the call hash
behind the queue). log takes a finished line (no newline). timing: also measure the time the consumer spends in
the backend, presents left out (two clock reads a record). consumer_started (may be NULL) runs first on the encode
thread when GPU_QUEUE_THREAD starts it (the owner check binds its consumer there) */
const struct gpu_backend *gpu_queue_wrap(const struct gpu_backend *chosen, const struct gpu_backend *beneath, int mode,
	int timing, void (*log)(const char *line), void (*consumer_started)(void));

/* every record written so far has reached the backend: for a caller that reaches the backend outside gpu.h */
void gpu_queue_drain(void);

/* the calls to gpu_queue_drain so far, the producer's own (AT_ONCE and QUERY calls) included: for a test that
checks which calls wait for the queue */
unsigned long gpu_queue_drains(void);

/* soft_depth_prepare's calls so far whose backend answer differed from the one the game was given (the producer's,
from soft_depth_ready): logged once, counted for a test */
unsigned long gpu_queue_answers_differ(void);

/* a test's check that a reader can start at a frame boundary: each frame's first draw (a keyframe, whose record
carries every part whole) is consumed again by a consumer with no state, which must end up holding what the queue's
consumer holds. Returns the keyframes checked so far and, in differ, how many left another state */
void gpu_queue_check_keyframes(int on);
unsigned long gpu_queue_keyframes_checked(unsigned long *differ);
/* for a test: how many times the encode thread has gone to sleep for want of records, and how long it spins first
(nanoseconds) */
unsigned long gpu_queue_consumer_sleeps(void);
unsigned long gpu_queue_consumer_spin_ns(void);

/* GPU_QUEUE_THREAD: how long the last present waited for the encode thread (the barrier), in seconds; -1 otherwise */
double gpu_queue_barrier_waited(void);

#endif
