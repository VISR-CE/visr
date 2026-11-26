/* Traced object shadows' resolve on the GPU (display.traced_shadows): the programs and the compute half that
gpu_metal.m's gpu_metal_traced_shadows_resolve encodes for each window, kept here so
port/ios/tests/traced_resolve_probe.m runs the same kernels in the same order on the Mac's GPU.

A window's apply draws record the bounds of the pixels they darken (nv2a_psh.c, into the bounds buffer). The resolve
then runs traced_bounds, one thread, which turns them into the rectangle the filter and the composite cover (the rect
buffer: TRACED_RECT_* below), and the filter's kernels are dispatched over it indirectly, so a window whose shadows
darken a corner filters that corner, whatever the game's covers on the CPU said. */
#ifndef TRACED_RESOLVE_H
#define TRACED_RESOLVE_H

#import <Metal/Metal.h>
#include <math.h>
#include <string.h>

#include "traced_shadows_format.h"

static NSString *const traced_resolve_source =
	@"#include <metal_stdlib>\n"
	"using namespace metal;\n"
	"\n"
	"struct traced_vertex_out\n"
	"{\n"
	"\tfloat4 position [[position]];\n"
	"};\n"
	"\n"
	"/* the rectangle the filter and the composite cover, in the target's texels (traced_bounds writes it); h is its half\n"
	"resolution */\n"
	"struct traced_filter_rect\n"
	"{\n"
	"\tuint cx, cy, cw, ch;\n"
	"\tuint hw, hh;\n"
	"\tuint pad0, pad1;\n"
	"};\n"
	"\n"
	"/* the composite's quad, a triangle strip over rc; size is the target's, which the viewport covers */\n"
	"vertex traced_vertex_out traced_vertex(uint id [[vertex_id]], device const traced_filter_rect &rc [[buffer(0)]],\n"
	"\tconstant float2 &size [[buffer(1)]])\n"
	"{\n"
	"\ttraced_vertex_out out;\n"
	"\tfloat2 corner = float2(rc.cx + ((id & 1) ? rc.cw : 0u), rc.cy + ((id & 2) ? rc.ch : 0u));\n"
	"\tout.position = float4(corner.x / size.x * 2.0 - 1.0, 1.0 - corner.y / size.y * 2.0, 0.0, 1.0);\n"
	"\treturn out;\n"
	"}\n"
	"\n"
	"/* the window's rectangle, and what to show: 0 the term's color (the product of the shadows' darkening), 1 the\n"
	"term's color alone, 2 the penumbra radius alone (both for looking at the term) */\n"
	"struct traced_composite_parameters\n"
	"{\n"
	"\tuint ox, oy, w, h;\n"
	"\tuint view;\n"
	"\tuint shifted; /* the term texture holds rc alone (the filter's output), its first texel at rc's corner */\n"
	"\tuint pad0, pad1;\n"
	"};\n"
	"\n"
	"fragment float4 traced_composite(traced_vertex_out in [[stage_in]], texture2d<float, access::read> term [[texture(0)]],\n"
	"\tconstant traced_composite_parameters &c [[buffer(0)]], device const traced_filter_rect &rc [[buffer(1)]])\n"
	"{\n"
	"\tuint2 q = clamp(uint2(in.position.xy), uint2(c.ox, c.oy), uint2(c.ox + c.w - 1, c.oy + c.h - 1));\n"
	"\tfloat4 t = term.read(q - (c.shifted ? uint2(rc.cx, rc.cy) : uint2(0)));\n"
	"\tif (c.view == 2)\n"
	"\t\treturn float4(t.a, t.a, t.a, 1.0);\n"
	"\treturn float4(t.rgb, 1.0);\n"
	"}\n"
	"\n"
	"/* The filter (debug.traced_shadows_filter): the term's hard edges become penumbrae as wide as the radius in its\n"
	"alpha, over rc (traced_filter_rect), which the work textures hold alone. radius is the largest penumbra in\n"
	"pixels (alpha 1); a the direction of a separable step; shifted: the blur's source holds rc alone */\n"
	"struct traced_filter_parameters\n"
	"{\n"
	"\tint ax, ay;\n"
	"\tuint shifted;\n"
	"\tfloat radius;\n"
	"};\n"
	"\n"
	"/* a shadow's apply draw darkened this pixel (the term is cleared to white) */\n"
	"static bool traced_hit(float4 t)\n"
	"{\n"
	"\treturn min(t.r, min(t.g, t.b)) < " TRACED_SHADOWS_DARKENED_BELOW_MSL ";\n"
	"}\n"
	"\n"
	"/* half resolution: the smallest radius, in pixels, among each 2x2 block's shadowed pixels; 0 with none */\n"
	"kernel void traced_radius_down(texture2d<float, access::read> term [[texture(0)]],\n"
	"\ttexture2d<float, access::write> half_out [[texture(1)]],\n"
	"\tconstant traced_filter_parameters &p [[buffer(0)]], device const traced_filter_rect &rc [[buffer(1)]],\n"
	"\tuint2 gid [[thread_position_in_grid]])\n"
	"{\n"
	"\tif (gid.x >= rc.hw || gid.y >= rc.hh)\n"
	"\t\treturn;\n"
	"\tfloat r = 0.0;\n"
	"\tfor (uint i = 0; i < 4; ++i)\n"
	"\t{\n"
	"\t\tuint2 q = min(gid * 2 + uint2(i & 1, i >> 1), uint2(rc.cw - 1, rc.ch - 1)) + uint2(rc.cx, rc.cy);\n"
	"\t\tfloat4 t = term.read(q);\n"
	"\t\tif (traced_hit(t))\n"
	"\t\t{\n"
	"\t\t\tfloat e = max(t.a * p.radius, 1.0);\n"
	"\t\t\tr = r > 0.0 ? min(r, e) : e;\n"
	"\t\t}\n"
	"\t}\n"
	"\thalf_out.write(float4(r), gid);\n"
	"}\n"
	"\n"
	"/* one direction of the blocker search: the widest radius among the texels along the axis whose penumbra reaches\n"
	"this one (a shadowed texel reaches as far as its own radius), so a pixel next to a shadow knows how wide to blur */\n"
	"kernel void traced_radius_reach(texture2d<float, access::read> half_in [[texture(0)]],\n"
	"\ttexture2d<float, access::write> half_out [[texture(1)]],\n"
	"\tconstant traced_filter_parameters &p [[buffer(0)]], device const traced_filter_rect &rc [[buffer(1)]],\n"
	"\tuint2 gid [[thread_position_in_grid]])\n"
	"{\n"
	"\tif (gid.x >= rc.hw || gid.y >= rc.hh)\n"
	"\t\treturn;\n"
	"\tint n = int(ceil(p.radius * 0.5));\n"
	"\tfloat best = 0.0;\n"
	"\tfor (int k = -n; k <= n; ++k)\n"
	"\t{\n"
	"\t\tint2 c = int2(gid) + int2(p.ax, p.ay) * k;\n"
	"\t\tif (c.x < 0 || c.y < 0 || c.x >= int(rc.hw) || c.y >= int(rc.hh))\n"
	"\t\t\tcontinue;\n"
	"\t\tfloat r = half_in.read(uint2(c)).r;\n"
	"\t\tif (r > 0.0 && float(abs(k)) * 2.0 <= r)\n"
	"\t\t\tbest = max(best, r);\n"
	"\t}\n"
	"\thalf_out.write(float4(best), gid);\n"
	"}\n"
	"\n"
	"/* one direction of the blur: a tent as wide as the pixel's radius (its own where it is shadowed, the searched one\n"
	"beside a shadow), every texel up to 16 a side and every second one past that. A tap counts only where the depth is\n"
	"what the center's depth and its slope along the axis predict (stored depth is linear across the screen on a flat\n"
	"surface), so an object in front of the ground neither lightens the shadow behind it nor takes its penumbra. The\n"
	"alpha written is the radius used, as the term's is */\n"
	"kernel void traced_blur(texture2d<float, access::read> term [[texture(0)]],\n"
	"\ttexture2d<float, access::read> source [[texture(1)]],\n"
	"\ttexture2d<float, access::read> reach [[texture(2)]],\n"
	"\tdepth2d<float, access::read> depth [[texture(3)]],\n"
	"\ttexture2d<float, access::write> out [[texture(4)]],\n"
	"\tconstant traced_filter_parameters &p [[buffer(0)]], device const traced_filter_rect &rc [[buffer(1)]],\n"
	"\tuint2 gid [[thread_position_in_grid]])\n"
	"{\n"
	"\tif (gid.x >= rc.cw || gid.y >= rc.ch)\n"
	"\t\treturn;\n"
	"\tint2 g = int2(gid) + int2(rc.cx, rc.cy);\n"
	"\tint2 low = int2(rc.cx, rc.cy), high = int2(rc.cx + rc.cw - 1, rc.cy + rc.ch - 1);\n"
	"\tint2 s = p.shifted ? int2(rc.cx, rc.cy) : int2(0), a = int2(p.ax, p.ay);\n"
	"\tfloat4 t = term.read(uint2(g));\n"
	"\tfloat r = traced_hit(t) ? max(t.a * p.radius, 1.0) : reach.read(min(gid / 2, uint2(rc.hw - 1, rc.hh - 1))).r;\n"
	"\tfloat3 sum = source.read(uint2(g - s)).rgb;\n"
	"\tif (r < 0.5)\n"
	"\t{\n"
	"\t\tout.write(float4(sum, 0.0), gid);\n"
	"\t\treturn;\n"
	"\t}\n"
	"\tfloat d0 = depth.read(uint2(g));\n"
	"\tfloat forward = depth.read(uint2(clamp(g + a, low, high))) - d0;\n"
	"\tfloat backward = d0 - depth.read(uint2(clamp(g - a, low, high)));\n"
	"\tfloat slope = abs(forward) < abs(backward) ? forward : backward;\n"
	/* (past 16 pixels of radius every other texel: the taps stay at 33 or fewer up to a 32-pixel radius and at
	63 for the largest, 61 pixels at 2048 lines, where every texel would take 125) */
	"\tint stride = r > 16.0 ? 2 : 1;\n"
	"\tint n = int(ceil(r / float(stride)));\n"
	"\tfloat total = 1.0;\n"
	"\tfor (int k = -n; k <= n; ++k)\n"
	"\t{\n"
	"\t\tint offset = k * stride;\n"
	"\t\tint2 q = g + a * offset;\n"
	"\t\tfloat w = 1.0 - float(abs(offset)) / (r + 1.0);\n"
	"\t\tif (k == 0 || w <= 0.0 || q.x < low.x || q.y < low.y || q.x > high.x || q.y > high.y)\n"
	"\t\t\tcontinue;\n"
	"\t\tfloat expected = d0 + slope * float(offset);\n"
	/* (a tap is on the same surface when its depth is within half of the change the center's slope predicts for
	that offset, so a step to another surface at any distance is left out; 1e-6 lets a surface facing the
	camera, with no slope, keep its taps) */
	"\t\tif (abs(depth.read(uint2(q)) - expected) > abs(slope) * float(abs(offset)) * 0.5 + 1e-6)\n"
	"\t\t\tcontinue;\n"
	"\t\tsum += source.read(uint2(q - s)).rgb * w;\n"
	"\t\ttotal += w;\n"
	"\t}\n"
	"\tout.write(float4(sum / total, r / p.radius), gid);\n"
	"}\n"
	"\n"
	"/* What the window's filter and composite cover (rc, and the threadgroups of the filter's two grids): the bounds\n"
	"the apply draws recorded of the pixels they darkened (nv2a_psh.c), widened by pad and kept inside the rectangle the\n"
	"game's covers give (in; its whole when use is 0), with the corner an even step from in's so the half resolution\n"
	"blocks fall where the whole rectangle's do. The bounds go back to empty for the next window. One thread, dispatched\n"
	"as one threadgroup of one (traced_filter_encode). */\n"
	"struct traced_bounds_input\n"
	"{\n"
	"\tuint cx, cy, cw, ch;\n"
	"\tuint pad, use, group, unused;\n"
	"};\n"
	"\n"
	"kernel void traced_bounds(device atomic_uint *bounds [[buffer(0)]], device traced_filter_rect &rc [[buffer(1)]],\n"
	"\tdevice uint *groups [[buffer(2)]], constant traced_bounds_input &in [[buffer(3)]])\n"
	"{\n"
	"\tuint left = atomic_load_explicit(&bounds[0], memory_order_relaxed);\n"
	"\tuint top = atomic_load_explicit(&bounds[1], memory_order_relaxed);\n"
	"\tuint right = atomic_load_explicit(&bounds[2], memory_order_relaxed);\n"
	"\tuint bottom = atomic_load_explicit(&bounds[3], memory_order_relaxed);\n"
	"\tatomic_store_explicit(&bounds[0], 0xffffffffu, memory_order_relaxed);\n"
	"\tatomic_store_explicit(&bounds[1], 0xffffffffu, memory_order_relaxed);\n"
	"\tatomic_store_explicit(&bounds[2], 0u, memory_order_relaxed);\n"
	"\tatomic_store_explicit(&bounds[3], 0u, memory_order_relaxed);\n"
	"\tuint l = in.cx, t = in.cy, r = in.cx + in.cw, b = in.cy + in.ch;\n"
	"\tif (in.use)\n"
	"\t{\n"
	"\t\tif (right <= left || bottom <= top)\n"
	"\t\t\tr = l, b = t;\n"
	"\t\telse\n"
	"\t\t{\n"
	"\t\t\tl = max(l, left > in.pad ? left - in.pad : 0u);\n"
	"\t\t\tt = max(t, top > in.pad ? top - in.pad : 0u);\n"
	"\t\t\tl = min(in.cx + ((l - in.cx) & ~1u), r);\n"
	"\t\t\tt = min(in.cy + ((t - in.cy) & ~1u), b);\n"
	"\t\t\tr = max(min(r, right + in.pad), l);\n"
	"\t\t\tb = max(min(b, bottom + in.pad), t);\n"
	"\t\t}\n"
	"\t}\n"
	"\trc.cx = l;\n"
	"\trc.cy = t;\n"
	"\trc.cw = r - l;\n"
	"\trc.ch = b - t;\n"
	"\trc.hw = (rc.cw + 1) / 2;\n"
	"\trc.hh = (rc.ch + 1) / 2;\n"
	"\tgroups[0] = (rc.hw + in.group - 1) / in.group;\n"
	"\tgroups[1] = (rc.hh + in.group - 1) / in.group;\n"
	"\tgroups[2] = 1;\n"
	"\tgroups[3] = (rc.cw + in.group - 1) / in.group;\n"
	"\tgroups[4] = (rc.ch + in.group - 1) / in.group;\n"
	"\tgroups[5] = 1;\n"
	"}\n";

/* the rect buffer: traced_filter_rect, then the threadgroups of the half resolution grid and of the whole one
(MTLDispatchThreadgroupsIndirectArguments each) */
enum
{
	TRACED_RECT_GROUPS_HALF = 32,
	TRACED_RECT_GROUPS_WHOLE = 44,
	TRACED_RECT_BYTES = 56,
	TRACED_BOUNDS_BYTES = 16,
	TRACED_FILTER_GROUP = 8,
};

/* the bounds of nothing, which the apply draws widen (traced_bounds leaves them so after each window) */
static inline void traced_bounds_clear(id<MTLBuffer> bounds)
{
	uint32_t *values = (uint32_t *)bounds.contents;

	values[0] = values[1] = 0xffffffffu;
	values[2] = values[3] = 0;
}

/* One window's compute half: traced_bounds, then with filter the radius search and the blur over the rectangle it
chose. covered is the rectangle the game's covers give, inside the window: the work textures hold it at least (work
at its size, half at half), and the bounds are kept inside it. use_bounds 0 covers all of it (HALO_RT_CPU_BOUNDS, for
measuring what the bounds save). The pad is twice the largest radius: a pixel within one radius of a shadow blurs taps
up to another radius away, and every pixel past that keeps its white term. The blurred term ends in work[1]. */
static void traced_filter_encode(id<MTLComputeCommandEncoder> compute, id<MTLComputePipelineState> bounds_kernel,
	id<MTLComputePipelineState> down, id<MTLComputePipelineState> reach, id<MTLComputePipelineState> blur,
	id<MTLBuffer> bounds, id<MTLBuffer> rect, MTLScissorRect covered, float radius, int use_bounds, int filter,
	id<MTLTexture> term, id<MTLTexture> depth, id<MTLTexture> work0, id<MTLTexture> work1, id<MTLTexture> half0,
	id<MTLTexture> half1)
{
	struct { uint32_t cx, cy, cw, ch, pad, use, group, unused; } in;
	struct { int32_t ax, ay; uint32_t shifted; float radius; } p;
	const MTLSize group = MTLSizeMake(TRACED_FILTER_GROUP, TRACED_FILTER_GROUP, 1);
	int i;

	in.cx = (uint32_t)covered.x;
	in.cy = (uint32_t)covered.y;
	in.cw = (uint32_t)covered.width;
	in.ch = (uint32_t)covered.height;
	in.pad = (uint32_t)ceilf(2.0f * radius);
	in.use = use_bounds ? 1 : 0;
	in.group = TRACED_FILTER_GROUP;
	in.unused = 0;
	[compute setComputePipelineState:bounds_kernel];
	[compute setBuffer:bounds offset:0 atIndex:0];
	[compute setBuffer:rect offset:0 atIndex:1];
	[compute setBuffer:rect offset:TRACED_RECT_GROUPS_HALF atIndex:2];
	[compute setBytes:&in length:sizeof(in) atIndex:3];
	[compute dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
	if (!filter)
		return;
	memset(&p, 0, sizeof(p));
	p.radius = radius;
	[compute setBuffer:rect offset:0 atIndex:1];
	/* the radius beside each shadow: half0, reached along x into half1, along y back into half0 */
	[compute setComputePipelineState:down];
	[compute setBytes:&p length:sizeof(p) atIndex:0];
	[compute setTexture:term atIndex:0];
	[compute setTexture:half0 atIndex:1];
	[compute dispatchThreadgroupsWithIndirectBuffer:rect indirectBufferOffset:TRACED_RECT_GROUPS_HALF
		threadsPerThreadgroup:group];
	[compute setComputePipelineState:reach];
	for (i = 0; i < 2; ++i)
	{
		p.ax = i == 0;
		p.ay = i == 1;
		[compute setBytes:&p length:sizeof(p) atIndex:0];
		[compute setTexture:i ? half1 : half0 atIndex:0];
		[compute setTexture:i ? half0 : half1 atIndex:1];
		[compute dispatchThreadgroupsWithIndirectBuffer:rect indirectBufferOffset:TRACED_RECT_GROUPS_HALF
			threadsPerThreadgroup:group];
	}
	/* the blur: the term along x into work0, that along y into work1 */
	[compute setComputePipelineState:blur];
	[compute setTexture:term atIndex:0];
	[compute setTexture:half0 atIndex:2];
	[compute setTexture:depth atIndex:3];
	for (i = 0; i < 2; ++i)
	{
		p.ax = i == 0;
		p.ay = i == 1;
		p.shifted = (uint32_t)i;
		[compute setBytes:&p length:sizeof(p) atIndex:0];
		[compute setTexture:i ? work0 : term atIndex:1];
		[compute setTexture:i ? work1 : work0 atIndex:4];
		[compute dispatchThreadgroupsWithIndirectBuffer:rect indirectBufferOffset:TRACED_RECT_GROUPS_WHOLE
			threadsPerThreadgroup:group];
	}
}

#endif
