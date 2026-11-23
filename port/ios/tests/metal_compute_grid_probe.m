/* metal_dispatch_grid (port/ios/host/metal_compute_grid.h) on the Mac's GPU: grids that are not a whole number of
   threadgroups write exactly their own cells of an oversized, sentinel-filled buffer, a zero-sized grid writes nothing,
   and the result equals a dispatchThreads: run (non-uniform threadgroups, Apple GPU family 4 and later; the app
   never calls it). Skips itself where there is no Metal device. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "metal_compute_grid.h"

enum { STRIDE = 40, ROWS = 40, SENTINEL = 0xdeadbeefu };

static NSString *const source = @"#include <metal_stdlib>\nusing namespace metal;\n"
	"kernel void fill(device uint *out [[buffer(0)]], constant uint4 &p [[buffer(1)]],\n"
	"\tuint2 g [[thread_position_in_grid]])\n"
	"{\n"
	"\tif (g.x >= p.x || g.y >= p.y)\n"
	"\t\treturn;\n"
	"\tout[g.y * p.z + g.x] = g.y * 1000u + g.x + 1u;\n"
	"}\n";

/* one grid into a fresh sentinel-filled buffer; uniform: through metal_dispatch_grid, else dispatchThreads: */
static void run(id<MTLDevice> device, id<MTLCommandQueue> queue, id<MTLComputePipelineState> pipeline, MTLSize grid,
	MTLSize group, BOOL uniform, uint32_t *result)
{
	id<MTLBuffer> buffer = [device newBufferWithLength:STRIDE * ROWS * sizeof(uint32_t)
		options:MTLResourceStorageModeShared];
	uint32_t *cells = buffer.contents, parameters[4] = { (uint32_t)grid.width, (uint32_t)grid.height, STRIDE, 0 };
	id<MTLCommandBuffer> commands = [queue commandBuffer];
	id<MTLComputeCommandEncoder> encoder = [commands computeCommandEncoder];
	size_t i;

	for (i = 0; i < STRIDE * ROWS; i++)
		cells[i] = SENTINEL;
	[encoder setComputePipelineState:pipeline];
	[encoder setBuffer:buffer offset:0 atIndex:0];
	[encoder setBytes:parameters length:sizeof(parameters) atIndex:1];
	if (uniform)
		metal_dispatch_grid(encoder, grid, group);
	else if (grid.width && grid.height)
		[encoder dispatchThreads:grid threadsPerThreadgroup:group];
	[encoder endEncoding];
	[commands commit];
	[commands waitUntilCompleted];
	assert(commands.status == MTLCommandBufferStatusCompleted);
	memcpy(result, cells, STRIDE * ROWS * sizeof(uint32_t));
}

int main(void)
{
	@autoreleasepool
	{
		static const struct { unsigned w, h, gw, gh; } cases[] = {
			{ 17, 13, 16, 16 }, { 16, 16, 16, 16 }, { 1, 1, 16, 16 }, { 33, 7, 8, 8 }, { 37, 1, 16, 1 },
			{ 39, 39, 16, 16 }, { 0, 5, 16, 16 }, { 5, 0, 8, 8 },
		};
		static uint32_t whole[STRIDE * ROWS], nonuniform[STRIDE * ROWS];
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		NSError *error = nil;
		id<MTLLibrary> library;
		id<MTLComputePipelineState> pipeline;
		id<MTLCommandQueue> queue;
		BOOL compare;
		size_t c;

		if (!device)
		{
			printf("metal compute grid probe: no Metal device, skipped\n");
			return 0;
		}
		library = [device newLibraryWithSource:source options:nil error:&error];
		pipeline = library ? [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"fill"]
			error:&error] : nil;
		assert(pipeline);
		queue = [device newCommandQueue];
		compare = [device supportsFamily:MTLGPUFamilyApple4];
		/* the rounding itself */
		{
			MTLSize groups = metal_grid_threadgroups(MTLSizeMake(17, 13, 1), MTLSizeMake(16, 16, 1));
			assert(groups.width == 2 && groups.height == 1 && groups.depth == 1);
			groups = metal_grid_threadgroups(MTLSizeMake(0, 3, 1), MTLSizeMake(8, 8, 1));
			assert(groups.width == 0 && groups.height == 1);
		}
		for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
		{
			MTLSize grid = MTLSizeMake(cases[c].w, cases[c].h, 1), group = MTLSizeMake(cases[c].gw, cases[c].gh, 1);
			unsigned x, y;

			run(device, queue, pipeline, grid, group, YES, whole);
			for (y = 0; y < ROWS; y++)
				for (x = 0; x < STRIDE; x++)
				{
					uint32_t expected = x < cases[c].w && y < cases[c].h ? y * 1000u + x + 1u : SENTINEL;

					if (whole[y * STRIDE + x] != expected)
					{
						fprintf(stderr, "metal compute grid probe: %ux%u in %ux%u groups: cell %u,%u is %#x, not %#x\n",
							cases[c].w, cases[c].h, cases[c].gw, cases[c].gh, x, y, whole[y * STRIDE + x], expected);
						return 1;
					}
				}
			if (compare)
			{
				run(device, queue, pipeline, grid, group, NO, nonuniform);
				assert(!memcmp(whole, nonuniform, sizeof(whole)));
			}
		}
		printf("metal compute grid probe: ok (%zu grids%s)\n", c, compare ? ", same as dispatchThreads:" : "");
	}
	return 0;
}
