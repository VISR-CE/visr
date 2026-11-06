/* Stills of display.crt_effect for look checks: a Hardcore frame (the game's 640x480 picture, a debug.screenshot_every
PNG) drawn at a display's size through the same shader the present pass uses (port/ios/host/crt_shader.h), or
through the plain bilinear stretch with "off", as Hardcore draws without the effect. The output is the 4:3 area
only (the pillarbox bars are black either way).

	xcrun clang -fobjc-arc -Iport/ios/host tools/crt_stills.m -framework Foundation -framework Metal \
		-framework ImageIO -framework CoreGraphics -framework UniformTypeIdentifiers -o build/crt-stills
	build/crt-stills in.png out.png HEIGHT [on|off] [CROP]

HEIGHT is the display's lines (1179 for an iPhone 17 Pro held sideways, 1640 for an iPad Air, 2160 for a 4K
TV); CROP, when given, also writes out.png's centered CROPxCROP pixels scaled 3x without smoothing beside it
(out-crop.png), for judging HUD text. */
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <Metal/Metal.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "crt_shader.h"

static NSString *const source = @"#include <metal_stdlib>\n"
	"using namespace metal;\n"
	"struct PresentOut { float4 position [[position]]; float2 coordinates; };\n"
	"vertex PresentOut present_vertex(uint id [[vertex_id]])\n"
	"{\n"
	"\tPresentOut out;\n"
	"\tfloat2 corner = float2(id & 1, id >> 1);\n"
	"\tout.position = float4(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0, 0.0, 1.0);\n"
	"\tout.coordinates = corner;\n"
	"\treturn out;\n"
	"}\n"
	"fragment float4 present_fragment(PresentOut in [[stage_in]], texture2d<float> picture [[texture(0)]],\n"
	"\tsampler linear [[sampler(0)]])\n"
	"{\n"
	"\treturn float4(picture.sample(linear, in.coordinates).rgb, 1.0);\n"
	"}\n" CRT_SHADER_SOURCE;

static CGImageRef load(const char *path)
{
	CGImageSourceRef image_source = CGImageSourceCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:@(path)], NULL);
	CGImageRef image = image_source ? CGImageSourceCreateImageAtIndex(image_source, 0, NULL) : NULL;

	if (image_source)
		CFRelease(image_source);
	return image;
}

static void save(const char *path, const void *pixels, size_t width, size_t height)
{
	CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	CGContextRef context = CGBitmapContextCreate((void *)pixels, width, height, 8, width * 4, space,
		kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
	CGImageRef image = CGBitmapContextCreateImage(context);
	CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
		(__bridge CFURLRef)[NSURL fileURLWithPath:@(path)], (__bridge CFStringRef)UTTypePNG.identifier, 1, NULL);

	CGImageDestinationAddImage(destination, image, NULL);
	if (!CGImageDestinationFinalize(destination))
		fprintf(stderr, "crt-stills: cannot write %s\n", path);
	CFRelease(destination);
	CGImageRelease(image);
	CGContextRelease(context);
	CGColorSpaceRelease(space);
}

int main(int argc, char **argv)
{
	@autoreleasepool
	{
		if (argc < 4)
		{
			fprintf(stderr, "usage: crt-stills in.png out.png HEIGHT [on|off] [CROP]\n");
			return 2;
		}
		int crt = argc < 5 || strcmp(argv[4], "off") != 0;
		size_t height = (size_t)atol(argv[3]), width = height * 4 / 3, crop = argc > 5 ? (size_t)atol(argv[5]) : 0;
		CGImageRef image = load(argv[1]);
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		NSError *error = nil;
		id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];

		if (!image || !library || !height)
		{
			fprintf(stderr, "crt-stills: %s\n", !image ? "cannot read the picture" : !height ? "no height" :
				error.description.UTF8String);
			return 1;
		}
		size_t source_width = CGImageGetWidth(image), source_height = CGImageGetHeight(image);
		NSMutableData *texels = [NSMutableData dataWithLength:source_width * source_height * 4];
		CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		CGContextRef context = CGBitmapContextCreate(texels.mutableBytes, source_width, source_height, 8,
			source_width * 4, space, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
		CGContextDrawImage(context, CGRectMake(0, 0, source_width, source_height), image);
		CGContextRelease(context);
		CGColorSpaceRelease(space);

		MTLTextureDescriptor *description = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
			width:source_width height:source_height mipmapped:NO];
		id<MTLTexture> picture = [device newTextureWithDescriptor:description];
		[picture replaceRegion:MTLRegionMake2D(0, 0, source_width, source_height) mipmapLevel:0
			withBytes:texels.bytes bytesPerRow:source_width * 4];
		description = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width
			height:height mipmapped:NO];
		description.usage = MTLTextureUsageRenderTarget;
		description.storageMode = MTLStorageModeShared;
		id<MTLTexture> target = [device newTextureWithDescriptor:description];

		MTLRenderPipelineDescriptor *pipeline_description = [MTLRenderPipelineDescriptor new];
		pipeline_description.vertexFunction = [library newFunctionWithName:@"present_vertex"];
		pipeline_description.fragmentFunction = [library newFunctionWithName:crt ? @"crt_fragment" : @"present_fragment"];
		pipeline_description.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pipeline_description error:&error];
		if (!pipeline)
		{
			fprintf(stderr, "crt-stills: %s\n", error.description.UTF8String);
			return 1;
		}
		MTLSamplerDescriptor *sampler_description = [MTLSamplerDescriptor new];
		sampler_description.minFilter = sampler_description.magFilter = MTLSamplerMinMagFilterLinear;
		sampler_description.sAddressMode = sampler_description.tAddressMode = MTLSamplerAddressModeClampToEdge;
		id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sampler_description];

		MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture = target;
		pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		id<MTLCommandQueue> queue = [device newCommandQueue];
		id<MTLCommandBuffer> commands = [queue commandBuffer];
		id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
		float sizes[4] = { (float)source_width, (float)source_height, (float)width, (float)height };
		[encoder setRenderPipelineState:pipeline];
		[encoder setFragmentTexture:picture atIndex:0];
		[encoder setFragmentSamplerState:sampler atIndex:0];
		[encoder setFragmentBytes:sizes length:sizeof(sizes) atIndex:0];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
		[encoder endEncoding];
		[commands commit];
		[commands waitUntilCompleted];

		NSMutableData *out = [NSMutableData dataWithLength:width * height * 4];
		[target getBytes:out.mutableBytes bytesPerRow:width * 4 fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
		save(argv[2], out.bytes, width, height);
		if (crop && crop <= width && crop <= height)
		{
			size_t x0 = (width - crop) / 2, y0 = (height - crop) / 2, scale = 3, side = crop * scale;
			NSMutableData *zoomed = [NSMutableData dataWithLength:side * side * 4];
			const uint32_t *from = out.bytes;
			uint32_t *to = zoomed.mutableBytes;
			for (size_t y = 0; y < side; y++)
				for (size_t x = 0; x < side; x++)
					to[y * side + x] = from[(y0 + y / scale) * width + x0 + x / scale];
			NSString *crop_path = [[@(argv[2]) stringByDeletingPathExtension] stringByAppendingString:@"-crop.png"];
			save(crop_path.fileSystemRepresentation, zoomed.bytes, side, side);
		}
		CGImageRelease(image);
	}
	return 0;
}
