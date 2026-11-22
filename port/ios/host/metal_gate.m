/* The Metal GPU-family gate's device side (metal_gate.h): gather the system Metal device's facts once and resolve
them. Everything that asks what the GPU can do reads the result. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <TargetConditionals.h>
#if __has_include(<MetalFX/MetalFX.h>) && !TARGET_OS_SIMULATOR && !defined(METAL_GATE_NO_METALFX)
#import <MetalFX/MetalFX.h>
#define METAL_GATE_HAVE_METALFX 1
#endif
#include "metal_gate.h"

static struct metal_gate gate;
static dispatch_once_t gate_once;
static int gate_adopted;

static void gather(id<MTLDevice> device, struct metal_gate_facts *facts)
{
	int family;

	*facts = (struct metal_gate_facts){ 0 };
	if (!device)
		return;
	/* MTLGPUFamilyApple1 is 1001 and each later family the next number */
	for (family = 1; family < 32; family++)
		if ([device supportsFamily:(MTLGPUFamily)(1000 + family)])
			facts->apple_families |= UINT32_C(1) << family;
	/* MTLGPUFamilyMac2 by number: the SDK deprecates the name (Apple silicon Macs answer for Apple7 too), and an
	Intel or AMD Mac answers for it alone */
	facts->mac = [device supportsFamily:(MTLGPUFamily)2002] ? 1 : 0;
#define METAL_GATE_ANSWER(query, yes) do { if (yes) facts->queries |= UINT32_C(1) << METAL_QUERY_##query; } while (0)
	if (@available(iOS 16.4, tvOS 16.4, visionOS 1.0, macOS 11.0, *))
		METAL_GATE_ANSWER(BC, device.supportsBCTextureCompression);
	if (@available(iOS 14.0, tvOS 16.0, visionOS 1.0, macOS 11.0, *))
		METAL_GATE_ANSWER(FLOAT32_FILTER, device.supports32BitFloatFiltering);
	METAL_GATE_ANSWER(RATE_MAP, [device supportsRasterizationRateMapWithLayerCount:1]);
#ifdef METAL_GATE_HAVE_METALFX
	METAL_GATE_ANSWER(METALFX_SPATIAL, [MTLFXSpatialScalerDescriptor supportsDevice:device]);
#endif
	METAL_GATE_ANSWER(COUNTERS_STAGE, [device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]);
	METAL_GATE_ANSWER(COUNTERS_DRAW, [device supportsCounterSampling:MTLCounterSamplingPointAtDrawBoundary]);
#undef METAL_GATE_ANSWER
}

const struct metal_gate *metal_gate(void)
{
	dispatch_once(&gate_once, ^{
		if (!gate_adopted)
		{
			struct metal_gate_facts facts;

			@autoreleasepool
			{
				gather(MTLCreateSystemDefaultDevice(), &facts);
			}
			metal_gate_resolve(&facts, &gate);
		}
	});
	return &gate;
}

void metal_gate_adopt_facts(const struct metal_gate_facts *facts)
{
	metal_gate_resolve(facts, &gate);
	gate_adopted = 1;
}
