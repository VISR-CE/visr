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

Calls go by their class (gpu_operation.h): RECORDED ones through the queue,
the others straight to the backend (AT_ONCE and QUERY after the queue is
consumed up to them).
*/

#ifndef __HALO_GPU_QUEUE_H
#define __HALO_GPU_QUEUE_H

#include "gpu.h"

enum { GPU_QUEUE_OFF, GPU_QUEUE_INLINE };

/* the setting's value ("off", "inline"), or -1 for one it does not know */
int gpu_queue_mode_parse(const char *value);

/* chosen, behind the queue: it receives the RECORDED commands. beneath (NULL: chosen) receives the calls the
producer makes itself; it is chosen without the wrappers that must see only recorded commands (the call hash
behind the queue). log takes a finished line (no newline). timing: also measure the time the consumer spends in
the backend, presents left out (two clock reads a record) */
const struct gpu_backend *gpu_queue_wrap(const struct gpu_backend *chosen, const struct gpu_backend *beneath, int mode,
	int timing, void (*log)(const char *line));

/* every record written so far has reached the backend: for a caller that reaches the backend outside gpu.h */
void gpu_queue_drain(void);

#endif
