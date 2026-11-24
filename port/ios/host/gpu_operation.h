/*
GPU_OPERATION.H

Each operation in GPU_BACKEND_OPERATIONS (gpu_backend.h: gpu.h's GPU_OPERATIONS, then
the host's GPU_HOST_OPERATIONS) by number, and its class: how the
encode queue (gpu_queue.c, debug.encode_thread) carries a call and whether the
call hash (gpu_call_hash.c, debug.gpu_call_hash) mixes it. This list is the
one place an operation's class is written down. An operation without a class
here does not compile, and neither does a recorded operation without its
writer and reader in gpu_queue.c or a hashed one without its body in
gpu_call_hash.c.

Classes:
- RECORDED: a command written to the queue; the queue's consumer makes the
  call, in order with the other recorded commands. The call hash compares
  these in front of the queue and behind it. One with an answer gives the
  game an answer the producer has: soft_depth_prepare's is soft_depth_ready's.
- AT_ONCE: a command the producer makes itself, on the backend under the
  queue, once the queue is consumed up to it (creates, the streams, the
  anti-aliasing setup and the compile accounting). Hashed in front of the
  queue only.
- PRODUCER: a call the producer makes itself, on the backend under the
  queue, without waiting for the queue: the backend keeps the state it
  touches (the stream sets) for the producer alone (stream_reserve, stream,
  stream_frame_end). Hashed in front of the queue only.
- QUERY: changes nothing in the backend, so a queue may answer it from a
  cache; made like an AT_ONCE call today. Never hashed. (One exception:
  soft_depth_ready compiles its program the first time it is asked; S4 moves
  it to the producer.)
- THREAD_SAFE: a read the backend answers from atomics or other state
  safe from any thread (output_cap_settled, asked every frame): it passes the
  queue without waiting for it. Never hashed.
- OTHER_THREAD: another thread's call (vertical_blank_wait, from
  d3d8_device.c's vertical blank thread), which passes the queue without
  touching it. Never hashed.
*/

#ifndef __HALO_GPU_OPERATION_H
#define __HALO_GPU_OPERATION_H

#include "gpu_backend.h"

enum gpu_operation
{
#define GPU_OPERATION_NUMBER_F(type, name, parameters, arguments) GPU_OPERATION_##name,
#define GPU_OPERATION_NUMBER_P(name, parameters, arguments) GPU_OPERATION_##name,
	GPU_BACKEND_OPERATIONS(GPU_OPERATION_NUMBER_F, GPU_OPERATION_NUMBER_P)
#undef GPU_OPERATION_NUMBER_F
#undef GPU_OPERATION_NUMBER_P
	GPU_OPERATION_COUNT
};

#define GPU_OPERATION_CLASS_texture_create AT_ONCE
#define GPU_OPERATION_CLASS_texture_upload RECORDED
#define GPU_OPERATION_CLASS_texture_copy_level RECORDED
#define GPU_OPERATION_CLASS_texture_copy_rects RECORDED
#define GPU_OPERATION_CLASS_texture_generate_mipmaps RECORDED
#define GPU_OPERATION_CLASS_texture_destroy RECORDED
#define GPU_OPERATION_CLASS_texture_read QUERY
#define GPU_OPERATION_CLASS_texture_set_content RECORDED
#define GPU_OPERATION_CLASS_texture_multisample RECORDED
#define GPU_OPERATION_CLASS_buffer_create AT_ONCE
#define GPU_OPERATION_CLASS_buffer_write RECORDED
#define GPU_OPERATION_CLASS_stream_reserve PRODUCER
#define GPU_OPERATION_CLASS_stream PRODUCER
#define GPU_OPERATION_CLASS_shader_create AT_ONCE
#define GPU_OPERATION_CLASS_clear RECORDED
#define GPU_OPERATION_CLASS_draw RECORDED
#define GPU_OPERATION_CLASS_visibility_begin RECORDED
#define GPU_OPERATION_CLASS_visibility_end RECORDED
#define GPU_OPERATION_CLASS_visibility_result QUERY
#define GPU_OPERATION_CLASS_flush RECORDED
#define GPU_OPERATION_CLASS_upscale RECORDED
#define GPU_OPERATION_CLASS_anti_alias_prepare AT_ONCE
#define GPU_OPERATION_CLASS_anti_alias RECORDED
#define GPU_OPERATION_CLASS_vertical_blank_wait OTHER_THREAD
#define GPU_OPERATION_CLASS_companion_size QUERY
#define GPU_OPERATION_CLASS_companion_set RECORDED
/* (the pacer's late start sleeps before the game reads input, so on the thread that reads it. It touches the pacer, the
frame's waits and the frame-deadline interval, which the encoder's frame_begin and present also touch, and the game
can write records between a present and its next controller read, so it waits for those first: with nothing pending
that is two atomic loads, and otherwise the game thread waits where it would have encoded them itself with the queue
off, so input is read no earlier and no later) */
#define GPU_OPERATION_CLASS_frame_start AT_ONCE
#define GPU_OPERATION_CLASS_present RECORDED
#define GPU_OPERATION_CLASS_present_stereo RECORDED
#define GPU_OPERATION_CLASS_call_count_take QUERY
#define GPU_OPERATION_CLASS_warm_list_read QUERY
#define GPU_OPERATION_CLASS_warm_begin RECORDED
#define GPU_OPERATION_CLASS_pipeline_warm RECORDED
#define GPU_OPERATION_CLASS_warm_end RECORDED
#define GPU_OPERATION_CLASS_pipeline_built_take QUERY
#define GPU_OPERATION_CLASS_frame_context RECORDED
#define GPU_OPERATION_CLASS_compiles_outstanding QUERY
#define GPU_OPERATION_CLASS_compile_progress AT_ONCE
/* GPU_HOST_OPERATIONS */
#define GPU_OPERATION_CLASS_stream_frame_end PRODUCER
#define GPU_OPERATION_CLASS_soft_depth_warm AT_ONCE
#define GPU_OPERATION_CLASS_soft_depth_ready QUERY
#define GPU_OPERATION_CLASS_soft_depth_prepare RECORDED
#define GPU_OPERATION_CLASS_soft_depth_finish RECORDED
#define GPU_OPERATION_CLASS_ssao_warm AT_ONCE
#define GPU_OPERATION_CLASS_ssao_apply RECORDED
#define GPU_OPERATION_CLASS_set_upscaler RECORDED
#define GPU_OPERATION_CLASS_upscaler QUERY
#define GPU_OPERATION_CLASS_set_crt RECORDED
#define GPU_OPERATION_CLASS_benchmark_realtime RECORDED
#define GPU_OPERATION_CLASS_output AT_ONCE
#define GPU_OPERATION_CLASS_request_output_cap AT_ONCE
#define GPU_OPERATION_CLASS_output_cap_settled THREAD_SAFE
#define GPU_OPERATION_CLASS_output_cap_commit AT_ONCE

/* gpu_metal.m's functions the host calls by name, outside the backend table, and why each may (every other function
there is static: the backend's operations are reached through its table, gpu_backend_metal, so in order with the
others). tools/gpu_metal_entry_points.py checks the built gpu_metal.o's functions against this list after every
iOS, tvOS and macOS build (tools/ios_build.py, tools/macos_build.py): a new one needs a row, or a place in the table.
- THREAD_SAFE: reads or counts through atomics, or reads a switch set for the run, from any thread
  (gpu_metal_count_gpu_time: a command buffer that host_stereo.m or host_theater.m committed itself), or sets
  such a switch with an atomic store the presenter reads at its next present (host_present_dither_set: the guest's
  knob chord).
- WORKGROUP: acts on the frame-deadline workgroup the presenting thread joined (the dispatch, the benchmark pages,
  share sheets, cloud saves, host_tv.m). They stay direct calls with the encode thread, which joins the workgroup and
  starts the interval at its present's return, before the barrier lets the game thread go: the game thread's end comes
  before its present is recorded and the encode thread's present ends it again, a non-member's end does nothing, and
  _leave and _hold return early with a game thread.
- SWITCH: the dispatch's rewrap calls it on the game thread at a frame boundary, with the queue drained and the encode
  thread idle, as the encode thread starts or ends (host_gpu_dispatch.c, encode_queue_rewrap). The encode thread's end
  hook (gpu_metal_frame_deadline_encode_thread_ending) is WORKGROUP: it leaves, on the encode thread, what that thread joined.
- SURFACE: reads the game layer's drawable size where the presenter reads it (host_picture_drawable: every pointer
  source maps through it). */
#define GPU_METAL_DIRECT_ENTRY_POINTS(E) \
	E(gpu_metal_async_compiles, THREAD_SAFE) \
	E(gpu_metal_count_gpu_time, THREAD_SAFE) \
	E(gpu_metal_gpu_spans_summary, THREAD_SAFE) \
	E(host_present_dither_set, THREAD_SAFE) \
	E(gpu_metal_frame_deadline_end, WORKGROUP) \
	E(gpu_metal_frame_deadline_leave, WORKGROUP) \
	E(gpu_metal_frame_deadline_hold, WORKGROUP) \
	E(gpu_metal_frame_deadline_before_switch, SWITCH) \
	E(gpu_metal_frame_deadline_encode_thread_ending, WORKGROUP) \
	E(gpu_metal_encode_owner_reset, SWITCH) \
	E(host_picture_drawable, SURFACE)

#define GPU_OPERATION_PASTE_(a, b) a##b
#define GPU_OPERATION_PASTE(a, b) GPU_OPERATION_PASTE_(a, b)
/* prefix followed by name's class (prefix##RECORDED, prefix##AT_ONCE, ...): a macro per class that a user defines,
so each operation expands by its class */
#define GPU_OPERATION_BY_CLASS(prefix, name) GPU_OPERATION_PASTE(prefix, GPU_OPERATION_CLASS_##name)

enum gpu_operation_class
{
	GPU_CLASS_RECORDED = 1,
	GPU_CLASS_AT_ONCE,
	GPU_CLASS_QUERY,
	GPU_CLASS_OTHER_THREAD,
	GPU_CLASS_PRODUCER,
	GPU_CLASS_THREAD_SAFE,
};

static const unsigned char gpu_operation_classes[GPU_OPERATION_COUNT] = {
#define GPU_OPERATION_CLASS_ENTRY_F(type, name, parameters, arguments) \
	[GPU_OPERATION_##name] = GPU_OPERATION_BY_CLASS(GPU_CLASS_, name),
#define GPU_OPERATION_CLASS_ENTRY_P(name, parameters, arguments) \
	[GPU_OPERATION_##name] = GPU_OPERATION_BY_CLASS(GPU_CLASS_, name),
	GPU_BACKEND_OPERATIONS(GPU_OPERATION_CLASS_ENTRY_F, GPU_OPERATION_CLASS_ENTRY_P)
#undef GPU_OPERATION_CLASS_ENTRY_F
#undef GPU_OPERATION_CLASS_ENTRY_P
};

static const char *const gpu_operation_names[GPU_OPERATION_COUNT] = {
#define GPU_OPERATION_NAME_F(type, name, parameters, arguments) [GPU_OPERATION_##name] = #name,
#define GPU_OPERATION_NAME_P(name, parameters, arguments) [GPU_OPERATION_##name] = #name,
	GPU_BACKEND_OPERATIONS(GPU_OPERATION_NAME_F, GPU_OPERATION_NAME_P)
#undef GPU_OPERATION_NAME_F
#undef GPU_OPERATION_NAME_P
};

#endif
