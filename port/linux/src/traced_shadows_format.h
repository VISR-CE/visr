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
	/* id: unused. Bytes: TRACED_SHADOWS_LIGHT_FLOATS floats, the light the next traced light draws come from */
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
	TRACED_SHADOWS_LIGHT_FLOATS = 16,
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
	uint32_t reserved;
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

/* TRACED_SHADOWS_LIGHT's floats (debug.traced_light_blocking; nv2a_psh.c's traced light block), four a row:
0 the light's position and radius; 1 x the ray's start past the lit point and its end short of the light,
y whether the frame's shadow casters block it too, z how near the light a caster may be and still not block it
(the unit that carries a flashlight stands at it), w unused; 2 x 1 to skip the rays this draw (the cost runs'
in-run comparison), y how many times the rays are traced (cost runs), z w unused; 3 where the ray is aimed: the
light itself, or the eye for a light the viewer carries (a flashlight sits a little ahead of the eye, past the corner
of a wall the player stands against, and would lose its whole beam on that wall) */

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
