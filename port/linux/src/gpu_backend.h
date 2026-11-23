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
	P(stream_frame_end, (void), ()) \
	GPU_HOST_SIDE_OPERATIONS(F, P)

/* The side operations: the Metal backend's work the host asks for outside the
draw calls, so they reach the backend in order with them (through the encode
queue when it is on). The host reaches them through host_gpu_dispatch.c's
host_gpu_<name>. A backend without the feature makes each one do nothing and
answer 0.
- soft_depth_warm, ssao_warm: compile the programs and touch nothing else (a
  level's warm-up, before any render target exists); soft_depth_warm answers
  whether the program compiled.
- soft_depth_ready: whether soft_depth_prepare with these arguments can run,
  from the inputs it checks first, so a producer has the answer before the
  call is made. soft_depth_prepare answers the same, or 0 when an allocation
  it then makes fails.
- soft_depth_prepare, soft_depth_finish, ssao_apply: the guest's
  host_soft_depth_prepare, host_soft_depth_finish and host_ssao_apply with a
  nonzero count (d3d8_device.c), mid-frame.
- set_upscaler, upscaler: the MetalFX upscaler on or off, and the last one
  asked for. set_crt: the present pass's CRT effect. benchmark_realtime: a
  fixed-timestep benchmark's frames shown in real time or not.
- output, request_output_cap, output_cap_settled, output_cap_commit: the
  output's layer and drawable size, and the built-in output's cap (ios_host.h,
  host_external.h), between frames. */
/* the side operations whose one argument is an int switch, which the queue and the call hash write alike */
#define GPU_HOST_SWITCH_OPERATIONS(S) \
	S(set_upscaler) \
	S(set_crt) \
	S(benchmark_realtime)

#define GPU_HOST_SIDE_OPERATIONS(F, P) \
	F(uint32_t, soft_depth_warm, (void), ()) \
	F(uint32_t, soft_depth_ready, (gpu_texture depth, const float *values, uint32_t count), (depth, values, count)) \
	F(uint32_t, soft_depth_prepare, (gpu_texture depth, const float *values, uint32_t count), (depth, values, count)) \
	P(soft_depth_finish, (void), ()) \
	P(ssao_warm, (void), ()) \
	P(ssao_apply, (gpu_texture color, gpu_texture depth, const float *values, uint32_t count), \
		(color, depth, values, count)) \
	P(set_upscaler, (int metalfx), (metalfx)) \
	F(int, upscaler, (void), ()) \
	P(set_crt, (int enabled), (enabled)) \
	P(benchmark_realtime, (int on), (on)) \
	F(int, output, (void *layer, void *screen, int width, int height), (layer, screen, width, height)) \
	P(request_output_cap, (int cap), (cap)) \
	F(int, output_cap_settled, (void), ()) \
	P(output_cap_commit, (void), ())

#define GPU_BACKEND_OPERATIONS(F, P) \
	GPU_OPERATIONS(F, P) \
	GPU_HOST_OPERATIONS(F, P)

#define GPU_BACKEND_FUNCTIONS(F, P) \
	GPU_BACKEND_OPERATIONS(F, P) \
	P(initialize, (uint32_t flags, struct gpu_capabilities *capabilities), (flags, capabilities))

/* GPU_DO_NOTHING(list): for each operation of an operation list (GPU_HOST_OPERATIONS, GPU_HOST_SIDE_OPERATIONS),
a static GPU_NOTHING_PREFIX##name that does nothing and answers 0, for a backend without the feature or a test's;
the user defines GPU_NOTHING_PREFIX first */
#define GPU_BACKEND_PASTE_(a, b) a##b
#define GPU_BACKEND_PASTE(a, b) GPU_BACKEND_PASTE_(a, b)
#define GPU_NOTHING_F(type, name, parameters, arguments) \
	static type GPU_BACKEND_PASTE(GPU_NOTHING_PREFIX, name) parameters { return (type)0; }
#define GPU_NOTHING_P(name, parameters, arguments) static void GPU_BACKEND_PASTE(GPU_NOTHING_PREFIX, name) parameters {}
#define GPU_DO_NOTHING(list) \
	_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
	list(GPU_NOTHING_F, GPU_NOTHING_P) \
	_Pragma("GCC diagnostic pop")

#define GPU_BACKEND_FUNCTION(type, name, parameters, arguments) type (*name) parameters;
#define GPU_BACKEND_PROCEDURE(name, parameters, arguments) void (*name) parameters;
struct gpu_backend
{
	GPU_BACKEND_FUNCTIONS(GPU_BACKEND_FUNCTION, GPU_BACKEND_PROCEDURE)
};
#undef GPU_BACKEND_FUNCTION
#undef GPU_BACKEND_PROCEDURE

#endif
