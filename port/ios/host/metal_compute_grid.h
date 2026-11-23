/* Compute dispatches that run on every GPU the app supports.

-[MTLComputeCommandEncoder dispatchThreads:threadsPerThreadgroup:] takes a grid that need not be a whole number of
threadgroups (non-uniform threadgroups). Only Apple GPU family 4 (A11) and later and Macs have that; on A8 to A10X
Metal raises 'APINotSupported' and the app aborts. Compute passes that cover a grid therefore dispatch whole
threadgroups through metal_dispatch_grid: the grid is rounded up to whole threadgroups, and every kernel dispatched this
way returns at once for a thread outside the size it is given, so the threads that write and what they write are the
same on every GPU. (A pass with a fixed count of threadgroups, such as the stereo HUD depth's, calls
dispatchThreadgroups: itself.) port/ios/tests/metal_compute_grid_probe.m checks the helper on the Mac's GPU.
tools/test_metal_gpu_families.py keeps dispatchThreads: out of the app and checks the kernels' bounds. */
#ifndef METAL_COMPUTE_GRID_H
#define METAL_COMPUTE_GRID_H

#import <Metal/Metal.h>

/* the threadgroups that cover grid: each dimension rounded up, a zero-sized one kept zero */
static inline MTLSize metal_grid_threadgroups(MTLSize grid, MTLSize group)
{
	return MTLSizeMake((grid.width + group.width - 1) / group.width, (grid.height + group.height - 1) / group.height,
		(grid.depth + group.depth - 1) / group.depth);
}

/* grid's threads in threadgroups of group; the threads past grid's edge run too and must return at once */
static inline void metal_dispatch_grid(id<MTLComputeCommandEncoder> encoder, MTLSize grid, MTLSize group)
{
	MTLSize groups = metal_grid_threadgroups(grid, group);

	if (groups.width && groups.height && groups.depth)
		[encoder dispatchThreadgroups:groups threadsPerThreadgroup:group];
}

#endif
