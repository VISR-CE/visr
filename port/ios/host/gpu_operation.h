/*
GPU_OPERATION.H

Each operation in GPU_OPERATIONS (gpu.h) by number, and its class: how the
encode queue (gpu_queue.c, debug.encode_thread) carries a call and whether the
call hash (gpu_call_hash.c, debug.gpu_call_hash) mixes it. This list is the
one place an operation's class is written down. An operation without a class
here does not compile, and neither does a recorded operation without its
writer and reader in gpu_queue.c or a hashed one without its body in
gpu_call_hash.c.

Classes:
- RECORDED: a command written to the queue; the queue's consumer makes the
  call, in order with the other recorded commands.
- AT_ONCE: a command the producer makes itself, once the queue is consumed up
  to it (creates, the streams, the anti-aliasing setup and the compile
  accounting).
- QUERY: changes nothing in the backend, so a queue may answer it from a
  cache; made like an AT_ONCE call today. Never hashed.
- OTHER_THREAD: another thread's call (vertical_blank_wait, from
  d3d8_device.c's vertical blank thread), which passes the queue without
  touching it. Never hashed.
*/

#ifndef __HALO_GPU_OPERATION_H
#define __HALO_GPU_OPERATION_H

#include "gpu.h"

enum gpu_operation
{
#define GPU_OPERATION_NUMBER_F(type, name, parameters, arguments) GPU_OPERATION_##name,
#define GPU_OPERATION_NUMBER_P(name, parameters, arguments) GPU_OPERATION_##name,
	GPU_OPERATIONS(GPU_OPERATION_NUMBER_F, GPU_OPERATION_NUMBER_P)
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
#define GPU_OPERATION_CLASS_stream_reserve AT_ONCE
#define GPU_OPERATION_CLASS_stream AT_ONCE
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
#define GPU_OPERATION_CLASS_frame_start RECORDED
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
};

static const unsigned char gpu_operation_classes[GPU_OPERATION_COUNT] = {
#define GPU_OPERATION_CLASS_ENTRY_F(type, name, parameters, arguments) \
	[GPU_OPERATION_##name] = GPU_OPERATION_BY_CLASS(GPU_CLASS_, name),
#define GPU_OPERATION_CLASS_ENTRY_P(name, parameters, arguments) \
	[GPU_OPERATION_##name] = GPU_OPERATION_BY_CLASS(GPU_CLASS_, name),
	GPU_OPERATIONS(GPU_OPERATION_CLASS_ENTRY_F, GPU_OPERATION_CLASS_ENTRY_P)
#undef GPU_OPERATION_CLASS_ENTRY_F
#undef GPU_OPERATION_CLASS_ENTRY_P
};

static const char *const gpu_operation_names[GPU_OPERATION_COUNT] = {
#define GPU_OPERATION_NAME_F(type, name, parameters, arguments) [GPU_OPERATION_##name] = #name,
#define GPU_OPERATION_NAME_P(name, parameters, arguments) [GPU_OPERATION_##name] = #name,
	GPU_OPERATIONS(GPU_OPERATION_NAME_F, GPU_OPERATION_NAME_P)
#undef GPU_OPERATION_NAME_F
#undef GPU_OPERATION_NAME_P
};

#endif
