/*
GPU_SIDE_IMPORTS.H

The guest's imports for the Metal backend's side operations (guest_host.h,
d3d8_device.c), which keep their ABI and forward to host_gpu_dispatch.c's
host_gpu_<name> (gpu_backend.h's GPU_HOST_SIDE_OPERATIONS). A call with no
values is a level's warm-up (d3d8_device.c's shader warming): it only compiles
the programs, before any render target exists, and leaves the current view's
soft-depth state alone.

Defined here, not in a .c file, so a probe can include the same definitions
with host_gpu_<name> of its own (port/ios/tests/gpu_side_imports_probe.c).
Included once, by host_gpu_dispatch.c.
*/

#ifndef __HALO_GPU_SIDE_IMPORTS_H
#define __HALO_GPU_SIDE_IMPORTS_H

void host_ssao_apply(gpu_texture color, gpu_texture depth, const float *values, uint32_t count)
{
	if (count == 0)
		host_gpu_ssao_warm();
	else
		host_gpu_ssao_apply(color, depth, values, count);
}

uint32_t host_soft_depth_prepare(gpu_texture depth, const float *values, uint32_t count)
{
	return count == 0 ? host_gpu_soft_depth_warm() : host_gpu_soft_depth_prepare(depth, values, count);
}

void host_soft_depth_finish(void)
{
	host_gpu_soft_depth_finish();
}

#endif
