/* Traced object shadows' resolve (port/ios/host/traced_resolve.h) on the Mac's GPU. The class: a filter rectangle that
misses pixels the filter changes, or changes what it computes. Each case filters one term twice, over the whole of
the game's covered rectangle and over the darkened pixels' bounds (traced_bounds), and the two must agree on every
pixel of the window, where a pixel outside the bounds' rectangle keeps the white term (the composite does not draw
it). Also: no darkened pixel gives an empty rectangle, and the bounds are left empty for the next window. Run by
tools/test_traced_shadows.py; skips itself where there is no Metal device. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "traced_resolve.h"

enum { WIDTH = 96, HEIGHT = 64 };

static int failures;

#define CHECK(condition, ...) do { if (!(condition)) { fprintf(stderr, "traced resolve: " __VA_ARGS__); \
	fprintf(stderr, " (line %d)\n", __LINE__); failures++; } } while (0)

static id<MTLDevice> device;
static id<MTLCommandQueue> queue;
static id<MTLComputePipelineState> bounds_kernel, down, reach, blur;

static id<MTLComputePipelineState> pipeline(id<MTLLibrary> library, NSString *name)
{
	NSError *error = nil;
	id<MTLComputePipelineState> made = [device newComputePipelineStateWithFunction:[library newFunctionWithName:name]
		error:&error];

	if (!made)
	{
		fprintf(stderr, "traced resolve: %s: %s\n", name.UTF8String, error.localizedDescription.UTF8String);
		exit(1);
	}
	return made;
}

static id<MTLTexture> texture(MTLPixelFormat format, unsigned long width, unsigned long height)
{
	MTLTextureDescriptor *description = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width
		height:height mipmapped:NO];

	description.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
	description.storageMode = MTLStorageModePrivate;
	return [device newTextureWithDescriptor:description];
}

/* a texture's RGBA8 texels, row after row */
static void read_back(id<MTLTexture> from, uint8_t *texels)
{
	id<MTLBuffer> buffer = [device newBufferWithLength:from.width * from.height * 4 options:MTLResourceStorageModeShared];
	id<MTLCommandBuffer> commands = [queue commandBuffer];
	id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];

	[blit copyFromTexture:from sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
		sourceSize:MTLSizeMake(from.width, from.height, 1) toBuffer:buffer destinationOffset:0
		destinationBytesPerRow:from.width * 4 destinationBytesPerImage:from.width * from.height * 4];
	[blit endEncoding];
	[commands commit];
	[commands waitUntilCompleted];
	memcpy(texels, buffer.contents, from.width * from.height * 4);
}

/* the filtered term over the whole window (white past what the filter wrote), and the rectangle it covered */
static void resolve(id<MTLTexture> term, id<MTLTexture> depth, const uint8_t *texels, MTLScissorRect covered,
	float radius, int use_bounds, uint8_t *result, uint32_t rect[6], uint32_t bounds_after[4])
{
	id<MTLBuffer> bounds = [device newBufferWithLength:TRACED_BOUNDS_BYTES options:MTLResourceStorageModeShared];
	id<MTLBuffer> rect_buffer = [device newBufferWithLength:TRACED_RECT_BYTES options:MTLResourceStorageModeShared];
	id<MTLTexture> work0 = texture(MTLPixelFormatRGBA8Unorm, covered.width, covered.height);
	id<MTLTexture> work1 = texture(MTLPixelFormatRGBA8Unorm, covered.width, covered.height);
	id<MTLTexture> half0 = texture(MTLPixelFormatR16Float, (covered.width + 1) / 2, (covered.height + 1) / 2);
	id<MTLTexture> half1 = texture(MTLPixelFormatR16Float, (covered.width + 1) / 2, (covered.height + 1) / 2);
	id<MTLCommandBuffer> commands = [queue commandBuffer];
	id<MTLComputeCommandEncoder> compute;
	uint32_t *values = bounds.contents;
	uint8_t *filtered = calloc(covered.width * covered.height, 4);
	unsigned long x, y;

	/* what the apply draws record (nv2a_psh.c): the darkened pixels' bounds, the right and bottom past the edge */
	traced_bounds_clear(bounds);
	for (y = 0; y < HEIGHT; y++)
		for (x = 0; x < WIDTH; x++)
		{
			const uint8_t *t = texels + (y * WIDTH + x) * 4;

			if (t[0] < 255 || t[1] < 255 || t[2] < 255)
			{
				values[0] = x < values[0] ? (uint32_t)x : values[0];
				values[1] = y < values[1] ? (uint32_t)y : values[1];
				values[2] = x + 1 > values[2] ? (uint32_t)x + 1 : values[2];
				values[3] = y + 1 > values[3] ? (uint32_t)y + 1 : values[3];
			}
		}
	compute = [commands computeCommandEncoder];
	traced_filter_encode(compute, bounds_kernel, down, reach, blur, bounds, rect_buffer, covered, radius, use_bounds, 1,
		term, depth, work0, work1, half0, half1);
	[compute endEncoding];
	[commands commit];
	[commands waitUntilCompleted];
	memcpy(rect, rect_buffer.contents, 6 * sizeof(uint32_t));
	memcpy(bounds_after, values, 4 * sizeof(uint32_t));
	read_back(work1, filtered);
	memset(result, 255, WIDTH * HEIGHT * 4);
	for (y = rect[1]; y < rect[1] + rect[3]; y++)
		for (x = rect[0]; x < rect[0] + rect[2]; x++)
			memcpy(result + (y * WIDTH + x) * 4, filtered + ((y - rect[1]) * covered.width + (x - rect[0])) * 4, 3);
	free(filtered);
}

/* a term with a darkened disc of radius size at (cx, cy), penumbra alpha a (1: a different one each texel, so which
texels share a half resolution block matters); texels outside it stay as they are */
static void disc(uint8_t *texels, int cx, int cy, int size, uint8_t alpha)
{
	int x, y;

	for (y = 0; y < HEIGHT; y++)
		for (x = 0; x < WIDTH; x++)
			if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= size * size)
			{
				uint8_t *t = texels + (y * WIDTH + x) * 4;

				t[0] = t[1] = t[2] = 77;
				t[3] = alpha == 1 ? (uint8_t)((x * 37 + y * 11) & 255) : alpha;
			}
}

/* one case: the bounds' rectangle filters the same picture as the whole covered rectangle, and is smaller */
static void check_case(const char *name, const uint8_t *texels, MTLScissorRect covered, float radius, int expect_smaller)
{
	id<MTLTexture> term = texture(MTLPixelFormatRGBA8Unorm, WIDTH, HEIGHT), depth = texture(MTLPixelFormatDepth32Float,
		WIDTH, HEIGHT);
	uint8_t *whole = malloc(WIDTH * HEIGHT * 4), *bounded = malloc(WIDTH * HEIGHT * 4);
	uint32_t whole_rect[6], bounded_rect[6], after[4];
	id<MTLBuffer> staging = [device newBufferWithBytes:texels length:WIDTH * HEIGHT * 4
		options:MTLResourceStorageModeShared];
	id<MTLCommandBuffer> commands = [queue commandBuffer];
	id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
	MTLRenderPassDescriptor *clear = [MTLRenderPassDescriptor renderPassDescriptor];
	long differing = 0, at;

	[blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:WIDTH * 4 sourceBytesPerImage:WIDTH * HEIGHT * 4
		sourceSize:MTLSizeMake(WIDTH, HEIGHT, 1) toTexture:term destinationSlice:0 destinationLevel:0
		destinationOrigin:MTLOriginMake(0, 0, 0)];
	[blit endEncoding];
	/* a flat surface: every tap's depth is what the center's predicts */
	clear.depthAttachment.texture = depth;
	clear.depthAttachment.loadAction = MTLLoadActionClear;
	clear.depthAttachment.storeAction = MTLStoreActionStore;
	clear.depthAttachment.clearDepth = 0.5;
	[[commands renderCommandEncoderWithDescriptor:clear] endEncoding];
	[commands commit];
	[commands waitUntilCompleted];
	resolve(term, depth, texels, covered, radius, 0, whole, whole_rect, after);
	resolve(term, depth, texels, covered, radius, 1, bounded, bounded_rect, after);
	CHECK(whole_rect[0] == covered.x && whole_rect[2] == covered.width, "%s: without the bounds the covered rectangle is "
		"filtered", name);
	for (at = 0; at < WIDTH * HEIGHT; at++)
		differing += memcmp(whole + at * 4, bounded + at * 4, 3) != 0;
	CHECK(!differing, "%s: %ld pixels differ between the bounds' rectangle (%u,%u %ux%u) and the covered one", name,
		differing, bounded_rect[0], bounded_rect[1], bounded_rect[2], bounded_rect[3]);
	if (expect_smaller)
		CHECK(bounded_rect[2] * bounded_rect[3] < covered.width * covered.height, "%s: the bounds' rectangle is smaller "
			"than the covered one", name);
	CHECK(after[0] == 0xffffffffu && after[1] == 0xffffffffu && !after[2] && !after[3], "%s: the bounds are left empty "
		"for the next window", name);
	free(whole);
	free(bounded);
}

int main(void)
{
	@autoreleasepool
	{
		NSError *error = nil;
		id<MTLLibrary> library;
		uint8_t *texels = malloc(WIDTH * HEIGHT * 4);
		const MTLScissorRect window = { 0, 0, WIDTH, HEIGHT }, inset = { 9, 5, 70, 50 };

		device = MTLCreateSystemDefaultDevice();
		if (!device)
		{
			printf("traced resolve: no Metal device, skipped\n");
			return 0;
		}
		queue = [device newCommandQueue];
		library = [device newLibraryWithSource:traced_resolve_source options:nil error:&error];
		if (!library)
		{
			fprintf(stderr, "traced resolve: the programs do not compile: %s\n", error.localizedDescription.UTF8String);
			return 1;
		}
		bounds_kernel = pipeline(library, @"traced_bounds");
		down = pipeline(library, @"traced_radius_down");
		reach = pipeline(library, @"traced_radius_reach");
		blur = pipeline(library, @"traced_blur");

		/* a shadow in the middle, its corner an odd step from the window's */
		memset(texels, 255, WIDTH * HEIGHT * 4);
		disc(texels, 47, 31, 6, 128);
		check_case("middle", texels, window, 8.0f, 1);
		/* penumbrae that differ texel to texel: the bounds' corner keeps the half resolution blocks where they were */
		memset(texels, 255, WIDTH * HEIGHT * 4);
		disc(texels, 48, 32, 9, 1);
		check_case("varied", texels, window, 10.0f, 1);
		/* a shadow at the window's edge: the rectangle is clamped to it */
		memset(texels, 255, WIDTH * HEIGHT * 4);
		disc(texels, 2, 60, 7, 255);
		check_case("edge", texels, window, 8.0f, 1);
		/* the game's covered rectangle inside the window, a shadow across its corner */
		memset(texels, 255, WIDTH * HEIGHT * 4);
		disc(texels, 12, 8, 6, 200);
		disc(texels, 60, 40, 3, 0);
		check_case("inset", texels, inset, 6.0f, 1);
		/* two shadows far apart: one rectangle holds both */
		memset(texels, 255, WIDTH * HEIGHT * 4);
		disc(texels, 10, 10, 3, 90);
		disc(texels, 85, 55, 4, 255);
		check_case("apart", texels, window, 5.0f, 0);
		/* no darkened pixel: nothing is filtered or drawn, and the term stays white */
		{
			id<MTLTexture> term = texture(MTLPixelFormatRGBA8Unorm, WIDTH, HEIGHT);
			id<MTLTexture> depth = texture(MTLPixelFormatDepth32Float, WIDTH, HEIGHT);
			uint8_t *result = malloc(WIDTH * HEIGHT * 4);
			uint32_t rect[6], after[4];

			memset(texels, 255, WIDTH * HEIGHT * 4);
			resolve(term, depth, texels, window, 8.0f, 1, result, rect, after);
			CHECK(!rect[2] || !rect[3], "with no darkened pixel the rectangle is empty (%ux%u)", rect[2], rect[3]);
			free(result);
		}
		free(texels);
	}
	if (failures)
		fprintf(stderr, "traced resolve: %d failures\n", failures);
	return failures != 0;
}
