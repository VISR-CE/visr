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
compile_progress snapshot) pass through unhashed:
they change nothing in the backend, and a queue may answer them from a cache.
*/

#ifndef __HALO_GPU_CALL_HASH_H
#define __HALO_GPU_CALL_HASH_H

#include "gpu.h"

/* frame counts the presents since the wrapper was installed; calls is the
commands hashed into hash since the last present (the present included);
second_thread is 1 on the first frame a command came from a thread other than
the one that sent the first command; parts lists each operation that ran (and a
draw's constants and uniforms apart from its packet) as name:hash, so two runs
that part can be traced to the operation that differs */
typedef void (*gpu_call_hash_report)(unsigned long frame, uint32_t calls, uint64_t hash, int second_thread,
	const char *parts);

/* how many wrappers can be installed at once: one on each side of a queue
(debug.encode_thread), so each frame's hash before the queue can be compared
with the hash after it */
enum { GPU_CALL_HASH_INSTANCES = 2 };

/* chosen, hashed by instance (0 or 1; installing an instance again starts it
over): the returned table calls chosen's functions */
const struct gpu_backend *gpu_call_hash_wrap(unsigned instance, const struct gpu_backend *chosen,
	gpu_call_hash_report report);

#endif
