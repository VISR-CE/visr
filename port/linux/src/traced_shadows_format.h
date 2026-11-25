/*
TRACED_SHADOWS_FORMAT.H

The records the game sends the Metal backend for traced object shadows
(display.traced_shadows): what gpu_backend.h's traced_shadows_data carries, by
kind. Written by port/linux/game/traced_shadow_casters.c, read by
port/ios/host/traced_shadows.h and gpu_metal.m. Fixed-width and little-endian,
<stdint.h> alone, so the 32-bit guest, the 64-bit host and the probes compile
it alike.

A map's parts and structures belong to an epoch: the game starts a new one
when a map unloads (TRACED_SHADOWS_RESET), and a record of an older epoch is
refused, so a vertex buffer the game reuses at the same address in the next
map can never name the last map's part.
*/

#ifndef __HALO_TRACED_SHADOWS_FORMAT_H
#define __HALO_TRACED_SHADOWS_FORMAT_H

#include <stdint.h>

enum
{
	/* id: the new epoch. No bytes. Every part and structure is dropped */
	TRACED_SHADOWS_RESET = 1,
	/* id: the part's. Bytes: traced_shadows_part, its vertices, its triangles */
	TRACED_SHADOWS_PART,
	/* id: the shadow's, this frame. Bytes: traced_shadows_caster, its shape, its pose */
	TRACED_SHADOWS_CASTER,
	/* id: unused. Bytes: traced_shadows_world, then 9 floats a triangle: the level's opaque surfaces */
	TRACED_SHADOWS_WORLD,
	/* id: unused. No bytes: every caster sent so far this frame is built before the next draw */
	TRACED_SHADOWS_COMMIT,
	/* id: the shadow the next traced draws apply. Bytes: TRACED_SHADOWS_BASIS_FLOATS floats */
	TRACED_SHADOWS_APPLY,
	/* id: unused. Bytes: struct traced_shadows_light, the light the next traced light draws come from */
	TRACED_SHADOWS_LIGHT,
};

enum
{
	TRACED_SHADOWS_PARTS = 4096,
	/* shadows a frame (the game's deferred list holds 256 a window) */
	TRACED_SHADOWS_SHADOWS = 2048,
	TRACED_SHADOWS_PART_VERTICES = 65535,
	TRACED_SHADOWS_NODE_FLOATS = 13,
	TRACED_SHADOWS_BASIS_FLOATS = 24,
	TRACED_SHADOWS_LIGHT_FLOATS = 48,
};

/* a model part at rest: vertex_count traced_shadows_vertex, then triangle_count * 3 16-bit indices (padded to a
multiple of 4 bytes) */
struct traced_shadows_part
{
	uint32_t epoch;
	uint32_t vertex_count;
	uint32_t triangle_count;
	uint32_t reserved;
};

/* a vertex follows two nodes of its model: weight of nodes[0], the rest of nodes[1] (the game's linear blend) */
struct traced_shadows_vertex
{
	float position[3];
	float weight;
	uint8_t nodes[2];
	uint8_t reserved[2];
};

/* a shadow's casters this frame. The shape says which parts under which nodes: model_count times
{ uint32_t node_count, part_count; uint32_t parts[part_count]; }. The pose is every model's node matrices in the
same order, TRACED_SHADOWS_NODE_FLOATS floats a node (the game's real_matrix4x3: scale, forward, left, up,
position). Shape and pose are kept apart so that a structure is found by its shape and updated by its pose */
struct traced_shadows_caster
{
	uint32_t epoch;
	uint32_t object;
	uint32_t model_count;
	uint32_t shape_size; /* bytes */
	uint32_t pose_size;  /* bytes */
	/* the light family the caster belongs to (port/linux/game/light_visibility.h): its instance takes mask bit
	family (2-31) in the traced lights' structure instead of bit 0, so the family's own lights pass through it;
	0 none */
	uint32_t family;
};

struct traced_shadows_world
{
	uint32_t epoch;
	uint32_t triangle_count;
};

/* TRACED_SHADOWS_APPLY's floats, as the traced pixel shader reads them (nv2a_psh.c's traced block), four a row:
0 forward and the shadow's radius; 1 left and the ray's start past the receiver; 2 up and the ray's end;
3 the shadow's position and whether the level's structure stops the ray;
4 x the tangent of the light's half-angle, y pixels a world unit at distance 1 (the projection's y scale times half
the view's height), z one over the largest penumbra radius in pixels, w unused; 5 unused */

/* TRACED_SHADOWS_LIGHT's record: the light the next traced light draws come from (traced_shadow_casters.c writes
it, gpu_metal.m sets skip and foveated, nv2a_psh.c's traced light block reads it). One list makes the C struct and
the shader's, so neither side counts floats. Each field is 1, 3 or 4 floats (a float, packed_float3 or
packed_float4 in the shader, 4-byte aligned on both sides):
- position, attenuation_radius: the light, and the radius its pass's attenuation coordinate was made with
- ray_start: world units past the lit point a ray starts; casters: 1 when the frame's casters block it too;
  keep_clear: how near the light a caster may be and still not block it (the owner's, when it has no family);
  stop_short: how far short of the light every ray stops
- skip: 1 to skip the rays this draw (cost runs, host); repeats: how many times the rays are traced (cost runs);
  strength: how fully a blocked ray darkens (the cap's ease); family: the owner's mask bit, 0 none
- camera: the eye's position (the side a pixel is seen from); attenuation_stage: the texture stage of the pass's
  attenuation coordinate, LIGHT_VISIBILITY_STAGE_NONE for none (light_visibility.h)
- view_rect: the view in the target's pixels (x, y, width, height); depth_p22, depth_p32: the projection's
  depth row (normalized depth -p22 + p32 / d); p00, p11, p20, p21: its x and y; foveated: 1 on a foveated pass (host); depth_point_mode:
  debug.traced_light_depth_point
- view_forward, view_left, view_up (scaled) and view_x, view_y, view_z: view_to_world, to rebuild a point from depth
- depth_bias: the fraction of its view distance a point rebuilt from depth starts its ray off its surface */
#define TRACED_SHADOWS_LIGHT_FIELDS(F) \
	F(3, position) F(1, attenuation_radius) \
	F(1, ray_start) F(1, casters) F(1, keep_clear) F(1, stop_short) \
	F(1, skip) F(1, repeats) F(1, strength) F(1, family) \
	F(3, camera) F(1, attenuation_stage) \
	F(4, view_rect) \
	F(1, depth_p22) F(1, depth_p32) F(1, p00) F(1, p11) \
	F(1, p20) F(1, p21) F(1, foveated) F(1, depth_point_mode) \
	F(3, view_forward) F(1, view_x) F(3, view_left) F(1, view_y) \
	F(3, view_up) F(1, view_z) \
	F(1, depth_bias) F(3, owner_center) F(1, owner_radius) F(3, reserved)

#define TRACED_SHADOWS_LIGHT_C_1(name) float name;
#define TRACED_SHADOWS_LIGHT_C_3(name) float name[3];
#define TRACED_SHADOWS_LIGHT_C_4(name) float name[4];
#define TRACED_SHADOWS_LIGHT_C_FIELD(count, name) TRACED_SHADOWS_LIGHT_C_##count(name)
#define TRACED_SHADOWS_LIGHT_MSL_1 "float"
#define TRACED_SHADOWS_LIGHT_MSL_3 "packed_float3"
#define TRACED_SHADOWS_LIGHT_MSL_4 "packed_float4"
#define TRACED_SHADOWS_LIGHT_MSL_FIELD(count, name) "\t" TRACED_SHADOWS_LIGHT_MSL_##count " " #name ";\n"

struct traced_shadows_light
{
	TRACED_SHADOWS_LIGHT_FIELDS(TRACED_SHADOWS_LIGHT_C_FIELD)
};

/* the traced lights' instance mask: casters of no family, the level, then one bit a light family (the family field
of a caster or light record), from FIRST to LAST */
enum
{
	TRACED_SHADOWS_MASK_NO_FAMILY = 1u,
	TRACED_SHADOWS_MASK_LEVEL = 2u,
	TRACED_SHADOWS_FAMILY_FIRST = 2,
	TRACED_SHADOWS_FAMILY_LAST = 31,
};

/* the same record in the shader, bound at fragment buffer 8 as `constant traced_shadows_light &traced_light` */
#define TRACED_SHADOWS_LIGHT_MSL \
	"struct traced_shadows_light\n{\n" TRACED_SHADOWS_LIGHT_FIELDS(TRACED_SHADOWS_LIGHT_MSL_FIELD) "};\n"

typedef char traced_shadows_light_size_check[sizeof(struct traced_shadows_light) == TRACED_SHADOWS_LIGHT_FLOATS * 4
	? 1 : -1];

/* traced_shadows_resolve's values (gpu_backend.h): the window's rectangle in the target's texels (x, y, width,
height), the padded rectangle the shadows cover (x, y, width, height), the largest penumbra radius in pixels,
whether to filter (0 composites the traced term as it is), whether only the pass break is wanted, and what to show
(debug.traced_shadows_view: 0 the scene with its shadows, 1 the term's color alone, 2 its penumbra radius alone) */
enum
{
	TRACED_SHADOWS_RESOLVE_WINDOW = 0,
	TRACED_SHADOWS_RESOLVE_COVERED = 4,
	TRACED_SHADOWS_RESOLVE_RADIUS = 8,
	TRACED_SHADOWS_RESOLVE_FILTER,
	TRACED_SHADOWS_RESOLVE_BREAK_ONLY,
	TRACED_SHADOWS_RESOLVE_VIEW,
	TRACED_SHADOWS_RESOLVE_FLOATS,
};

#endif
