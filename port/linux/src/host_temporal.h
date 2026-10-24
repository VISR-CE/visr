/*
HOST_TEMPORAL.H

Spike: what d3d8_device.c sends gpu_metal.m for MetalFX's temporal scaler
(debug.metalfx_temporal), through the guest import host_metalfx_temporal_frame
(port/runtime/guest/runtime/guest_host.h).
*/

#ifndef __HOST_TEMPORAL_H
#define __HOST_TEMPORAL_H

#include <stdint.h>

/* Spike (debug.metalfx_temporal): the scene's camera this frame and last,
as world-to-clip rows (c[-96..-93]), the jitter it was drawn with, and
whether the temporal scaler starts over; gpu_metal.m takes it at the next
gpu_upscale. Floats and 32-bit words only, laid out alike on both sides. */
struct host_temporal_frame
{
	float world_to_clip[16];
	float previous_world_to_clip[16];
	/* in the scene's pixels, +x right, +y down; and as added to NDC */
	float jitter_pixels[2];
	float jitter_ndc[2];
	float motion_scale;
	uint32_t enabled;
	uint32_t reset;
	uint32_t pad;
};

#endif
