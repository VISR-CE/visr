/*
GPU_OWNER_CHECK.H

With the encode queue on (debug.encode_thread, gpu_queue.h), every call that
reaches the backend comes from the thread its class says (gpu_operation.h):
- RECORDED: the queue's consumer;
- AT_ONCE, QUERY and PRODUCER: the producer;
both are the thread that installs the check (gpu_initialize's, the game's);
- THREAD_SAFE and OTHER_THREAD: any thread (the classes of the calls that may
  come from one).
A call from another thread is reported once per class and counted; a
development build (HALO_DEVELOPMENT_HOOKS) then stops. The wrapper is made
from the class table, so a new operation is checked by its class.
*/

#ifndef __HALO_GPU_OWNER_CHECK_H
#define __HALO_GPU_OWNER_CHECK_H

#include "gpu_backend.h"

/* chosen behind the check. report takes a finished line (no newline); stop: stop after reporting (a development
build) */
const struct gpu_backend *gpu_owner_check_wrap(const struct gpu_backend *chosen, void (*report)(const char *line),
	int stop);

/* the calling thread is the queue's consumer from now on: the encode thread, when it starts */
void gpu_owner_check_bind_consumer(void);

/* the calls so far that came from a thread other than their class's owner: for a test */
unsigned long gpu_owner_check_violations(void);

#endif
