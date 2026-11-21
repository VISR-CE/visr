/*
GPU_CALL_HASH.H

debug.gpu_call_hash: a backend wrapper that hashes, frame by frame, every
command the backend receives (the call, its arguments and the bytes they point
to) and hands each frame's hash to a report function at gpu_present or
gpu_present_stereo. Two runs whose backends received the same commands, with
the same bytes, in the same order, report the same hashes; a dropped,
reordered or stale command changes the frame's hash. It is how a change to the
path from the front end to the backend (the encode thread's queue) is shown
not to change what the backend does.

Queries (the QUERY and OTHER_THREAD classes in gpu_operation.h, and a
compile_progress snapshot) pass through unhashed: they change nothing in the
backend, and a queue may answer them from a cache.

With the encode queue (debug.encode_thread) a wrapper sits on each side of
it. The one in front sees every command in the order the game sends them, so
its hash compares with a run without the queue. The one behind sees only the
RECORDED commands, the ones the queue carries; a frame's recorded hash in
front must equal the hash behind. Calls the producer makes itself (AT_ONCE)
do not pass the wrapper behind: they may run while the consumer is in it, and
their order against recorded commands is not the backend's.
*/

#ifndef __HALO_GPU_CALL_HASH_H
#define __HALO_GPU_CALL_HASH_H

#include "gpu_backend.h"

/* frame counts the presents since the wrapper was installed; calls is the
commands hashed into hash since the last present (the present included), and
recorded_calls and recorded the RECORDED ones among them alone; second_thread
is 1 on the first frame a command came from a thread other than the one that
sent the first command; parts lists each operation that ran (and a draw's
constants and uniforms apart from its packet) as name:hash, so two runs that
part can be traced to the operation that differs */
struct gpu_call_hash_frame
{
	unsigned long frame;
	uint32_t calls, recorded_calls;
	uint64_t hash, recorded;
	int second_thread;
	const char *parts;
};

typedef void (*gpu_call_hash_report)(const struct gpu_call_hash_frame *frame);

/* how many wrappers can be installed at once: one on each side of a queue
(debug.encode_thread), so each frame's hash before the queue can be compared
with the hash after it */
enum { GPU_CALL_HASH_INSTANCES = 2 };

/* chosen, hashed by instance (0 or 1; installing an instance again starts it
over): the returned table calls chosen's functions */
const struct gpu_backend *gpu_call_hash_wrap(unsigned instance, const struct gpu_backend *chosen,
	gpu_call_hash_report report);

#endif
