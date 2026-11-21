/*
GPU_BACKEND.H

One backend's table of functions (struct gpu_backend): gpu.h's operations,
then the operations only the host calls. It holds host pointers, so it never
crosses to the guest, and it is outside the render capture's interface hash
(tools/gpu_interface_hash.py): adding a host operation leaves saved captures
valid.

Lists, each as gpu.h's GPU_OPERATIONS: F(return type, name, parameters,
arguments) and P(name, parameters, arguments).
- GPU_HOST_OPERATIONS: the operations the host calls itself, with no gpu_
  entry point and outside the guest's imports.
- GPU_BACKEND_OPERATIONS: GPU_OPERATIONS, then GPU_HOST_OPERATIONS.
- GPU_BACKEND_FUNCTIONS: GPU_BACKEND_OPERATIONS and initialize, the table.
*/

#ifndef __HALO_GPU_BACKEND_H
#define __HALO_GPU_BACKEND_H

#include "gpu.h"

/* stream_frame_end: the frame's streams end, so the next stream call fills
the next frame's set. The iOS host (port/ios/host/host_gpu_dispatch.c) calls
it before every present; a host that reaches a backend only through gpu.h's
entry points (gpu_gl.c's, elsewhere) never does, so a backend that needs it
can only be used through the iOS host */
#define GPU_HOST_OPERATIONS(F, P) \
	P(stream_frame_end, (void), ())

#define GPU_BACKEND_OPERATIONS(F, P) \
	GPU_OPERATIONS(F, P) \
	GPU_HOST_OPERATIONS(F, P)

#define GPU_BACKEND_FUNCTIONS(F, P) \
	GPU_BACKEND_OPERATIONS(F, P) \
	P(initialize, (uint32_t flags, struct gpu_capabilities *capabilities), (flags, capabilities))

#define GPU_BACKEND_FUNCTION(type, name, parameters, arguments) type (*name) parameters;
#define GPU_BACKEND_PROCEDURE(name, parameters, arguments) void (*name) parameters;
struct gpu_backend
{
	GPU_BACKEND_FUNCTIONS(GPU_BACKEND_FUNCTION, GPU_BACKEND_PROCEDURE)
};
#undef GPU_BACKEND_FUNCTION
#undef GPU_BACKEND_PROCEDURE

#endif
