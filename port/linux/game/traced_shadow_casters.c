/*
TRACED_SHADOW_CASTERS.C

display.traced_shadows (Metal on a GPU with ray tracing hardware): object shadows traced against their casters
instead of looked up in a shadow map. The game still decides which objects cast shadows, along which axis, how dark
and over which surfaces (render_object_shadows, structure_render_shadow); only the shadow map changes: nothing is
drawn into it, and the apply draws trace.

- Parts. As the game would draw a shadow's models into its map (_rasterizer_environment_shadow_model_draw), each
  model part is named to the backend instead. A part's vertices at rest and its triangles are sent once a map
  (TRACED_SHADOWS_PART, port/linux/src/traced_shadows_format.h) and known by a number from then on.
- Casters. A shadow's casters are its parts and the node matrices they are drawn under (TRACED_SHADOWS_CASTER). The
  backend skins them and keeps an acceleration structure for each object's shape (port/ios/host/traced_shadows.h).
- The level. With debug.traced_shadows_world, the level's opaque surfaces are sent once (TRACED_SHADOWS_WORLD), so
  that a ray stops at a floor between a receiver and its caster.
- Applying. Each apply draw of the shadow (_rasterizer_environment_shadow_draw) takes the traced pixel shader key
  (halo_traced_shadows_key, d3d8_device.c; nv2a_psh.c) after TRACED_SHADOWS_APPLY names the shadow and its basis.

Whether a window traces is the front end's answer (halo_traced_shadows_window_begin, d3d8_device.c). Apple builds
only: the records go to gpu_metal.m.
*/

#include "cseries.h"
#include "math/real_math.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_geometry.h"
#include "rasterizer/rasterizer_model_types.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "shaders/shader_definitions.h"
#include "shaders/shaders.h"
#include "structures/structure_bsp_definitions.h"
#include "tag_files/tag_groups.h"

int config_boolean(const char *name); /* port/linux/src/port_config.c */
long config_integer(const char *name);
double config_real(const char *name);
void platform_log(const char *format, ...);
unsigned long system_milliseconds(void); /* cseries_windows.h */
#include "traced_shadow_casters.h"
#include "light_visibility.h"
#include "../src/halo_stereo.h"

#include <xtl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HALO_IOS

#include "../src/traced_shadows_format.h"

/* port/runtime/guest/runtime/guest_host.h */
void host_traced_shadows_data(uint32_t kind, uint32_t id, const void *bytes, uint32_t size);
/* d3d8_device.c, d3d8_resources.c */
int halo_traced_shadows_window_begin(float p11);
void halo_traced_shadows_window_end(void);
void halo_traced_shadows_key(int on);
void halo_light_visibility_key(int path);
int halo_light_visibility_view(float rect[4]);
void halo_traced_shadows_basis(float values[4]);
void halo_traced_shadows_cover(float x0, float y0, float x1, float y1);
const void *halo_vertex_buffer_bytes(const void *buffer);
/* rasterizer_xbox.c (port): a part's own nodes, for a model of more nodes than the constants hold */
short custom_edition_part_palette(struct vertex_buffer const *vertex_buffer, byte const **nodes);

enum
{
	PART_TABLE_SIZE = 8192, /* a power of two, twice the parts a map may define */
	PART_PROBES = 64,
	SHAPE_WORDS = 2048,
	POSE_NODES = 2048,
};

_Static_assert(sizeof(real_matrix4x3) == TRACED_SHADOWS_NODE_FLOATS * sizeof(float),
	"a node matrix is not traced_shadows_format.h's pose entry");

/* a model part the backend knows, by the buffers the game draws it from */
struct traced_part
{
	void const *vertex_buffer;
	void const *triangle_buffer;
	void const *vertex_hardware;
	void const *triangle_hardware;
	boolean palette;
	boolean valid;
	byte left_out; /* why it is not valid (TRACED_LEFT_OUT_*) */
	unsigned short id;
};

/* why a draw, a part or a shadow was left out (traced_left_out) */
enum
{
	TRACED_LEFT_OUT_NO_NODES,
	TRACED_LEFT_OUT_VERTEX_TYPE,
	TRACED_LEFT_OUT_TRIANGLE_TYPE,
	TRACED_LEFT_OUT_PART_SIZE,
	TRACED_LEFT_OUT_PART_BYTES,
	TRACED_LEFT_OUT_PART_EMPTY,
	TRACED_LEFT_OUT_PART_TABLE,
	TRACED_LEFT_OUT_SHAPE,
	TRACED_LEFT_OUT_SHADOW_EMPTY,
	TRACED_LEFT_OUT_SHADOW_LIMIT,
	TRACED_LEFT_OUT_REASONS
};

void platform_log(const char *format, ...);

static struct
{
	struct traced_part *parts;
	unsigned long part_count; /* numbers given out this map */
	unsigned long epoch;
	boolean reset_sent;
	boolean window;
	boolean world; /* debug.traced_shadows_world, and the level's surfaces sent */
	unsigned __int64 frame;
	unsigned long next_id;
	/* the shadow being generated */
	boolean generating;
	unsigned long id;
	long object_index;
	unsigned long model_count;
	unsigned long *shape; /* SHAPE_WORDS */
	unsigned long shape_words, model_at;
	float *pose;          /* POSE_NODES node matrices */
	unsigned long pose_nodes;
	real_matrix4x3 const *model_matrices;
	short model_matrix_count;
	/* the level's surfaces, as last sent */
	struct structure_bsp *world_bsp;
	void const *world_first_vertices;
	/* what was left out since the last log line, by reason (traced_left_out) */
	unsigned long left_out[TRACED_LEFT_OUT_REASONS], frames;
	/* shadows whose volume projects to part of the window, and to all of it (traced_shadow_cover) */
	unsigned long cover_cases[2];
} traced;

/* Nothing a shadow leaves out is silent: a shadow without a caster has its apply draws dropped by the backend, so it
is missing from the picture. Each reason is counted and logged every 600 traced frames in which any occurred. */
static void traced_left_out(
	long reason)
{
	traced.left_out[reason]++;
}

static void traced_left_out_log(
	void)
{
	static char const *const names[TRACED_LEFT_OUT_REASONS] = { "draws without node matrices", "parts of another vertex type",
		"parts of another triangle type", "parts too large or past the part limit", "parts whose buffers cannot be read",
		"parts with no triangles", "part table full", "models past a shadow's shape or pose limit", "shadows with no parts",
		"shadows past the frame's limit" };
	char text[512];
	long reason, at = 0;

	if (++traced.frames < 600)
		return;
	traced.frames = 0;
	for (reason = 0; reason < TRACED_LEFT_OUT_REASONS; reason++)
	{
		if (traced.left_out[reason] && at < (long)sizeof(text) - 80)
			at += snprintf(text + at, sizeof(text) - at, "%s%lu %s", at ? ", " : "", traced.left_out[reason], names[reason]);
		traced.left_out[reason] = 0;
	}
	if (at)
		platform_log("traced shadows: left out in 600 frames: %s", text);
	if (traced.cover_cases[1])
		platform_log("traced shadows: in 600 frames %lu shadows covered part of the window, %lu all of it",
			traced.cover_cases[0], traced.cover_cases[1]);
	traced.cover_cases[0] = traced.cover_cases[1] = 0;
}

void traced_shadow_casters_new_map(
	void)
{
	if (traced.parts)
		memset(traced.parts, 0, sizeof(*traced.parts) * PART_TABLE_SIZE);
	traced.part_count = 0;
	traced.epoch++;
	traced.reset_sent = FALSE;
	traced.world_bsp = NULL;
	traced.world_first_vertices = NULL;
}

/* the level's opaque render triangles, once for each structure the map switches to */
static void world_send(
	void)
{
	struct structure_bsp *bsp = global_structure_bsp_get();
	void const *first_vertices = NULL;
	long lightmap_index, material_index, total = 0, written = 0;
	struct traced_shadows_world *record;
	float *positions;

	if (!bsp || !bsp->lightmaps.address || bsp->lightmaps.count <= 0)
		return;
	{
		struct structure_lightmap *lightmap = TAG_BLOCK_GET_ELEMENT(&bsp->lightmaps, 0, struct structure_lightmap);

		if (lightmap->materials.count > 0)
			first_vertices = TAG_BLOCK_GET_ELEMENT(&lightmap->materials, 0, struct structure_material)->vertices.hardware_format;
	}
	if (bsp == traced.world_bsp && first_vertices == traced.world_first_vertices)
		return;
	traced.world_bsp = bsp;
	traced.world_first_vertices = first_vertices;
	for (lightmap_index = 0; lightmap_index < bsp->lightmaps.count; lightmap_index++)
	{
		struct structure_lightmap *lightmap = TAG_BLOCK_GET_ELEMENT(&bsp->lightmaps, lightmap_index, struct structure_lightmap);

		for (material_index = 0; material_index < lightmap->materials.count; material_index++)
			total += TAG_BLOCK_GET_ELEMENT(&lightmap->materials, material_index, struct structure_material)->surface_count;
	}
	if (total <= 0 || !(record = malloc(sizeof(*record) + sizeof(float) * 9 * total)))
		return;
	positions = (float *)(record + 1);
	for (lightmap_index = 0; lightmap_index < bsp->lightmaps.count; lightmap_index++)
	{
		struct structure_lightmap *lightmap = TAG_BLOCK_GET_ELEMENT(&bsp->lightmaps, lightmap_index, struct structure_lightmap);

		for (material_index = 0; material_index < lightmap->materials.count; material_index++)
		{
			struct structure_material *material = TAG_BLOCK_GET_ELEMENT(&lightmap->materials, material_index,
				struct structure_material);
			struct shader *shader = material->shader.index != NONE ? shader_definition_get(material->shader.index) : NULL;
			byte const *vertices = halo_vertex_buffer_bytes(material->vertices.hardware_format);
			long stride = rasterizer_geometry_get_vertex_size(material->vertices.type);
			long surface;

			/* (glass, water and other see-through surfaces let the light through) */
			if (!shader || shader_type_is_transparent(shader->base.type) || !vertices || stride < 12)
				continue;
			for (surface = 0; surface < material->surface_count; surface++)
			{
				struct structure_surface *triangle = TAG_BLOCK_GET_ELEMENT(&bsp->surfaces,
					material->first_surface_index + surface, struct structure_surface);
				short corner;

				if (!triangle)
					continue;
				for (corner = 0; corner < 3; corner++)
				{
					long vertex = triangle->vertex_indices[corner];
					float const *point = (float const *)(vertices + (vertex < material->vertices.count ? vertex : 0) * stride);

					positions[written * 9 + corner * 3 + 0] = point[0];
					positions[written * 9 + corner * 3 + 1] = point[1];
					positions[written * 9 + corner * 3 + 2] = point[2];
				}
				written++;
			}
		}
	}
	if (written > 0)
	{
		record->epoch = (uint32_t)traced.epoch;
		record->triangle_count = (uint32_t)written;
		host_traced_shadows_data(TRACED_SHADOWS_WORLD, 0, record,
			(uint32_t)(sizeof(*record) + sizeof(float) * 9 * written));
	}
	free(record);
}

boolean traced_shadow_casters_window_begin(
	real p11)
{
	traced.window = halo_traced_shadows_window_begin(p11) != 0;
	if (!traced.window)
		return FALSE;
	if (traced.frame != rasterizer_globals.fps_accumulation_frame_index)
	{
		traced.frame = rasterizer_globals.fps_accumulation_frame_index;
		traced.next_id = 0;
		traced_left_out_log();
	}
	if (!traced.reset_sent)
	{
		host_traced_shadows_data(TRACED_SHADOWS_RESET, (uint32_t)traced.epoch, NULL, 0);
		traced.reset_sent = TRUE;
	}
	traced.world = FALSE;
	if (config_boolean("debug.traced_shadows_world"))
	{
		world_send();
		traced.world = traced.world_bsp != NULL;
	}
	return TRUE;
}

void traced_shadow_casters_window_end(
	void)
{
	traced.window = FALSE;
	traced.generating = FALSE;
	halo_traced_shadows_window_end();
}

/* a part's record: its vertices at rest, each with the two nodes of its model it follows, and its triangles (a
strip's unrolled, its odd ones turned, its degenerate ones left out) */
static boolean part_send(
	struct traced_part *part,
	struct triangle_buffer const *triangle_buffer,
	struct vertex_buffer const *vertex_buffer)
{
	byte const *vertices;
	BYTE *indices = NULL;
	byte const *palette = NULL;
	short palette_count = 0;
	long stride, index_count, index;
	unsigned long size, triangle_count = 0;
	struct traced_shadows_part *record;
	boolean sent = FALSE;

	if (vertex_buffer->type == _rasterizer_vertex_type_model_compressed)
		stride = sizeof(struct model_vertex_compressed);
	else if (vertex_buffer->type == _rasterizer_vertex_type_model_uncompressed)
		stride = sizeof(struct model_vertex_uncompressed);
	else
	{
		part->left_out = TRACED_LEFT_OUT_VERTEX_TYPE;
		return FALSE;
	}
	if (triangle_buffer->type == _triangle_buffer_type_precompiled_strip)
		index_count = triangle_buffer->count + 2;
	else if (triangle_buffer->type == _triangle_buffer_type_triangles)
		index_count = triangle_buffer->count * 3;
	else
	{
		part->left_out = TRACED_LEFT_OUT_TRIANGLE_TYPE;
		return FALSE;
	}
	if (vertex_buffer->count <= 0 || triangle_buffer->count <= 0 || vertex_buffer->count > TRACED_SHADOWS_PART_VERTICES ||
		traced.part_count >= TRACED_SHADOWS_PARTS)
	{
		part->left_out = TRACED_LEFT_OUT_PART_SIZE;
		return FALSE;
	}
	/* (a model of more nodes than the constants hold: the part's vertices name its own nodes,
	rasterizer_model_part_skinning) */
	if (part->palette)
		palette_count = custom_edition_part_palette(vertex_buffer, &palette);
	size = sizeof(*record) + sizeof(struct traced_shadows_vertex) * vertex_buffer->count +
		((sizeof(unsigned short) * 3 * index_count + 3) & ~3ul);
	record = malloc(size);
	if (!record)
	{
		part->left_out = TRACED_LEFT_OUT_PART_BYTES;
		return FALSE;
	}
	memset(record, 0, size);
	/* (until it is sent: its buffers could not be read, or it has no triangle) */
	part->left_out = TRACED_LEFT_OUT_PART_BYTES;
	vertices = (byte const *)halo_vertex_buffer_bytes(vertex_buffer->hardware_format);
	IDirect3DIndexBuffer8_Lock((IDirect3DIndexBuffer8 *)triangle_buffer->hardware_format, 0, 0, &indices, D3DLOCK_READONLY);
	if (vertices && indices)
	{
		struct traced_shadows_vertex *out = (struct traced_shadows_vertex *)(record + 1);
		unsigned short *triangles = (unsigned short *)(out + vertex_buffer->count);
		unsigned short const *list = (unsigned short const *)indices;
		boolean strip = triangle_buffer->type == _triangle_buffer_type_precompiled_strip;

		for (index = 0; index < vertex_buffer->count; index++, out++)
		{
			long nodes[2];
			short which;

			if (vertex_buffer->type == _rasterizer_vertex_type_model_compressed)
			{
				struct model_vertex_compressed const *in = (struct model_vertex_compressed const *)(vertices + index * stride);

				out->position[0] = in->position.x;
				out->position[1] = in->position.y;
				out->position[2] = in->position.z;
				nodes[0] = in->nodes[0] / 3;
				nodes[1] = in->nodes[1] / 3;
				out->weight = (float)in->node_weight * (1.f / 32767.f);
			}
			else
			{
				struct model_vertex_uncompressed const *in = (struct model_vertex_uncompressed const *)(vertices + index * stride);

				out->position[0] = in->position.x;
				out->position[1] = in->position.y;
				out->position[2] = in->position.z;
				nodes[0] = in->nodes[0];
				nodes[1] = in->nodes[1];
				out->weight = in->node_weights[0];
			}
			for (which = 0; which < 2; which++)
			{
				long node = nodes[which];

				if (palette)
					node = node >= 0 && node < palette_count ? palette[node] : 0;
				out->nodes[which] = (uint8_t)(node >= 0 && node < 255 ? node : 0);
			}
		}
		for (index = 0; index + 2 < index_count; index += strip ? 1 : 3)
		{
			unsigned short a = list[index], b = list[index + 1], c = list[index + 2];

			if (a == b || b == c || a == c || a >= vertex_buffer->count || b >= vertex_buffer->count ||
				c >= vertex_buffer->count)
			{
				continue;
			}
			if (strip && (index & 1))
			{
				unsigned short swap = b;

				b = c;
				c = swap;
			}
			triangles[triangle_count * 3 + 0] = a;
			triangles[triangle_count * 3 + 1] = b;
			triangles[triangle_count * 3 + 2] = c;
			triangle_count++;
		}
		part->left_out = TRACED_LEFT_OUT_PART_EMPTY;
		if (triangle_count)
		{
			record->epoch = (uint32_t)traced.epoch;
			record->vertex_count = (uint32_t)vertex_buffer->count;
			record->triangle_count = (uint32_t)triangle_count;
			part->id = (unsigned short)traced.part_count++;
			host_traced_shadows_data(TRACED_SHADOWS_PART, part->id, record,
				(uint32_t)(sizeof(*record) + sizeof(struct traced_shadows_vertex) * vertex_buffer->count +
					((sizeof(unsigned short) * 3 * triangle_count + 3) & ~3ul)));
			sent = TRUE;
		}
	}
	if (indices)
		IDirect3DIndexBuffer8_Unlock((IDirect3DIndexBuffer8 *)triangle_buffer->hardware_format);
	free(record);
	return sent;
}

static struct traced_part *part_get(
	struct triangle_buffer const *triangle_buffer,
	struct vertex_buffer const *vertex_buffer,
	boolean palette)
{
	unsigned long hash = ((unsigned long)(uintptr_t)vertex_buffer * 2654435761ul) ^ (unsigned long)(uintptr_t)triangle_buffer;
	long probe;

	if (!traced.parts)
	{
		traced.parts = calloc(PART_TABLE_SIZE, sizeof(*traced.parts));
		if (!traced.parts)
			return NULL;
	}
	for (probe = 0; probe < PART_PROBES; probe++)
	{
		struct traced_part *part = &traced.parts[(hash + probe) & (PART_TABLE_SIZE - 1)];

		if (part->vertex_buffer == vertex_buffer && part->triangle_buffer == triangle_buffer &&
			part->vertex_hardware == vertex_buffer->hardware_format &&
			part->triangle_hardware == triangle_buffer->hardware_format && part->palette == palette)
		{
			if (!part->valid)
				traced_left_out(part->left_out);
			return part->valid ? part : NULL;
		}
		if (!part->vertex_buffer)
		{
			part->vertex_buffer = vertex_buffer;
			part->triangle_buffer = triangle_buffer;
			part->vertex_hardware = vertex_buffer->hardware_format;
			part->triangle_hardware = triangle_buffer->hardware_format;
			part->palette = palette;
			part->valid = part_send(part, triangle_buffer, vertex_buffer);
			if (!part->valid)
				traced_left_out(part->left_out);
			return part->valid ? part : NULL;
		}
	}
	traced_left_out(TRACED_LEFT_OUT_PART_TABLE);
	return NULL;
}

unsigned long traced_shadow_casters_begin(
	long object_index)
{
	if (!traced.window)
		return TRACED_SHADOW_NONE;
	if (!traced.shape)
		traced.shape = malloc(sizeof(*traced.shape) * SHAPE_WORDS);
	if (!traced.pose)
		traced.pose = malloc(sizeof(real_matrix4x3) * POSE_NODES);
	if (!traced.shape || !traced.pose || traced.next_id >= TRACED_SHADOWS_SHADOWS)
	{
		traced_left_out(TRACED_LEFT_OUT_SHADOW_LIMIT);
		return TRACED_SHADOW_NONE;
	}
	traced.generating = TRUE;
	traced.id = traced.next_id++;
	traced.object_index = object_index;
	traced.model_count = 0;
	traced.shape_words = 0;
	traced.model_at = 0;
	traced.pose_nodes = 0;
	traced.model_matrices = NULL;
	traced.model_matrix_count = 0;
	return traced.id;
}

void traced_shadow_casters_part(
	struct triangle_buffer const *triangle_buffer,
	struct vertex_buffer const *vertex_buffer,
	struct render_skinning const *skinning)
{
	struct traced_part *part;

	if (!traced.generating)
		return;
	if (!skinning || !skinning->node_matrices || skinning->node_matrix_count <= 0 ||
		skinning->node_matrix_count > 255 || !triangle_buffer || !vertex_buffer || !triangle_buffer->hardware_format ||
		!vertex_buffer->hardware_format)
	{
		traced_left_out(TRACED_LEFT_OUT_NO_NODES);
		return;
	}
	part = part_get(triangle_buffer, vertex_buffer, skinning->node_matrix_count >= RASTERIZER_MAXIMUM_NODES_PER_MODEL);
	if (!part)
		return;
	/* another model of the shadow's (an object's parts are drawn model by model): its nodes join the pose */
	if (skinning->node_matrices != traced.model_matrices || skinning->node_matrix_count != traced.model_matrix_count)
	{
		if (traced.shape_words + 3 > SHAPE_WORDS || traced.pose_nodes + skinning->node_matrix_count > POSE_NODES)
		{
			traced_left_out(TRACED_LEFT_OUT_SHAPE);
			return;
		}
		traced.model_matrices = skinning->node_matrices;
		traced.model_matrix_count = skinning->node_matrix_count;
		traced.model_at = traced.shape_words;
		traced.shape[traced.shape_words++] = (unsigned long)skinning->node_matrix_count;
		traced.shape[traced.shape_words++] = 0;
		memcpy(traced.pose + traced.pose_nodes * TRACED_SHADOWS_NODE_FLOATS, skinning->node_matrices,
			sizeof(real_matrix4x3) * skinning->node_matrix_count);
		traced.pose_nodes += skinning->node_matrix_count;
		traced.model_count++;
	}
	if (traced.shape_words >= SHAPE_WORDS)
	{
		traced_left_out(TRACED_LEFT_OUT_SHAPE);
		return;
	}
	traced.shape[traced.shape_words++] = part->id;
	traced.shape[traced.model_at + 1]++;
}

void traced_shadow_casters_end(
	void)
{
	struct traced_shadows_caster *record;
	unsigned long shape_size, pose_size;

	if (!traced.generating)
		return;
	traced.generating = FALSE;
	shape_size = sizeof(uint32_t) * traced.shape_words;
	pose_size = sizeof(real_matrix4x3) * traced.pose_nodes;
	if (!traced.model_count || !(record = malloc(sizeof(*record) + shape_size + pose_size)))
	{
		traced_left_out(TRACED_LEFT_OUT_SHADOW_EMPTY);
		return;
	}
	record->epoch = (uint32_t)traced.epoch;
	record->object = (uint32_t)traced.object_index;
	record->model_count = (uint32_t)traced.model_count;
	record->shape_size = (uint32_t)shape_size;
	record->pose_size = (uint32_t)pose_size;
	record->family = (uint32_t)light_visibility_family(traced.object_index);
	memcpy(record + 1, traced.shape, shape_size);
	memcpy((byte *)(record + 1) + shape_size, traced.pose, pose_size);
	host_traced_shadows_data(TRACED_SHADOWS_CASTER, (uint32_t)traced.id, record,
		(uint32_t)(sizeof(*record) + shape_size + pose_size));
	free(record);
}

void traced_shadow_casters_commit(
	void)
{
	if (traced.window)
		host_traced_shadows_data(TRACED_SHADOWS_COMMIT, 0, NULL, 0);
}

/* What the shadow can darken on screen, for the resolve's rectangle: its volume (r along forward and left, 4 r along
up, as the apply draw's texture coordinates span it) clipped by the near plane and projected by the window's camera, in
normalized device coordinates with y down. Corners behind the near plane are replaced by the points where the box's
edges cross it, so a volume the camera stands in still covers only what its front part projects to (the whole window
once was the answer for any volume reaching the near plane, and a player's own shadow reaches it most frames). A
mirrored camera or no projection covers the window; a volume wholly behind the camera covers nothing. */
static void traced_cover_point(
	struct render_frustum const *frustum,
	real_point3d const *view,
	real bounds[4])
{
	real inverse_depth = -1.f / view->z;
	/* (render_camera_view_to_screen's projection, without its clip) */
	real x = (frustum->projection_matrix[0][0] * view->x + frustum->projection_matrix[2][0] * view->z) * inverse_depth;
	real y = -(frustum->projection_matrix[1][1] * view->y + frustum->projection_matrix[2][1] * view->z) * inverse_depth;

	bounds[0] = x < bounds[0] ? x : bounds[0];
	bounds[1] = y < bounds[1] ? y : bounds[1];
	bounds[2] = x > bounds[2] ? x : bounds[2];
	bounds[3] = y > bounds[3] ? y : bounds[3];
}

static void traced_shadow_cover(
	real_matrix4x3 const *shadow_matrix,
	real radius)
{
	struct render_frustum const *frustum = &global_window_parameters.frustum;
	real bounds[4] = { 1e30f, 1e30f, -1e30f, -1e30f };
	real_point3d views[8];
	real plane;
	long corner, axis;

	if (!frustum->projection_valid || global_window_parameters.camera.mirrored)
	{
		halo_traced_shadows_cover(-1.f, -1.f, 1.f, 1.f);
		return;
	}
	plane = -frustum->z_near;
	for (corner = 0; corner < 8; corner++)
	{
		real a = corner & 1 ? radius : -radius, b = corner & 2 ? radius : -radius, c = corner & 4 ? 4.f * radius : -4.f * radius;
		real_point3d world;

		world.x = shadow_matrix->position.x + shadow_matrix->forward.i * a + shadow_matrix->left.i * b + shadow_matrix->up.i * c;
		world.y = shadow_matrix->position.y + shadow_matrix->forward.j * a + shadow_matrix->left.j * b + shadow_matrix->up.j * c;
		world.z = shadow_matrix->position.z + shadow_matrix->forward.k * a + shadow_matrix->left.k * b + shadow_matrix->up.k * c;
		matrix4x3_transform_point(&frustum->world_to_view, &world, &views[corner]);
		if (views[corner].z <= plane)
			traced_cover_point(frustum, &views[corner], bounds);
	}
	/* the box's twelve edges (corners one bit apart): where one crosses the near plane */
	for (corner = 0; corner < 8; corner++)
	{
		for (axis = 1; axis < 8; axis <<= 1)
		{
			real_point3d const *from = &views[corner], *to = &views[corner | axis];
			real_point3d crossing;
			real t;

			if ((corner & axis) || (from->z <= plane) == (to->z <= plane))
				continue;
			t = (plane - from->z) / (to->z - from->z);
			crossing.x = from->x + (to->x - from->x) * t;
			crossing.y = from->y + (to->y - from->y) * t;
			crossing.z = plane;
			traced_cover_point(frustum, &crossing, bounds);
		}
	}
	if (bounds[2] < bounds[0] || bounds[3] < bounds[1])
		return;
	traced.cover_cases[bounds[0] <= -1.f && bounds[1] <= -1.f && bounds[2] >= 1.f && bounds[3] >= 1.f]++;
	halo_traced_shadows_cover(bounds[0], bounds[1], bounds[2], bounds[3]);
}

void traced_shadow_apply(
	unsigned long id,
	real_matrix4x3 const *shadow_matrix,
	real radius)
{
	float basis[TRACED_SHADOWS_BASIS_FLOATS];

	/* (a shadow without a slot is generated and applied in one go: its casters end at its first apply draw) */
	if (traced.generating)
		traced_shadow_casters_end();
	memset(basis, 0, sizeof(basis));
	basis[0] = shadow_matrix->forward.i;
	basis[1] = shadow_matrix->forward.j;
	basis[2] = shadow_matrix->forward.k;
	basis[3] = radius;
	basis[4] = shadow_matrix->left.i;
	basis[5] = shadow_matrix->left.j;
	basis[6] = shadow_matrix->left.k;
	basis[7] = 0.005f; /* the ray's start past the receiver, world units (1.5 cm) */
	basis[8] = shadow_matrix->up.i;
	basis[9] = shadow_matrix->up.j;
	basis[10] = shadow_matrix->up.k;
	basis[11] = 8.f * radius; /* its end: the shadow volume's depth (xT1.x spans 4 r each way) */
	basis[12] = shadow_matrix->position.x;
	basis[13] = shadow_matrix->position.y;
	basis[14] = shadow_matrix->position.z;
	basis[15] = traced.world ? 1.f : 0.f;
	halo_traced_shadows_basis(&basis[16]);
	host_traced_shadows_data(TRACED_SHADOWS_APPLY, (uint32_t)id, basis, sizeof(basis));
	traced_shadow_cover(shadow_matrix, radius);
	halo_traced_shadows_key(1);
}

void traced_shadow_apply_end(
	void)
{
	halo_traced_shadows_key(0);
}

/* debug.traced_light_blocking: 0 off; 1 the level's surfaces block dynamic lights; 2 the frame's shadow casters
too; 3 as 2, and objects with no shadow of their own block too, through the caster walker (render_objects.c) */
static long traced_light_mode(
	void)
{
	static unsigned __int64 frame = (unsigned __int64)-1;
	static long mode;

	if (frame != rasterizer_globals.fps_accumulation_frame_index)
	{
		frame = rasterizer_globals.fps_accumulation_frame_index;
		/* (the profile's choice, display.traced_lights, with what blocks from the debug setting: -1 follows it) */
		mode = config_integer("debug.traced_light_blocking");
		if (mode < 0)
			mode = config_integer("display.traced_lights") > 0 ? 3 : 0;
	}
	return mode;
}

boolean traced_light_walks_casters(
	void)
{
	return traced_light_mode() > 2;
}

/* What traced_lights_choose decided: per window, the ramp and this frame's strengths by light */
static struct
{
	struct light_visibility_ramp ramp[MAXIMUM_WINDOWS];
	unsigned __int64 frame[MAXIMUM_WINDOWS];
} traced_lights;

void traced_lights_choose(
	struct light_visibility_candidate const *lights,
	short count,
	boolean versus,
	float *strength)
{
	short window = render.window_index;
	long cap = config_integer("display.traced_lights");
	unsigned char chosen[LIGHT_VISIBILITY_MAXIMUM];
	struct light_visibility_ramp *ramp;
	short i, j;

	if (count > LIGHT_VISIBILITY_MAXIMUM)
		count = LIGHT_VISIBILITY_MAXIMUM;
	for (i = 0; i < count; i++)
		strength[i] = 0.f;
	/* (debug.traced_light_blocking above 0 traces without the profile key, every light up to the game's limit) */
	if (cap <= 0 && config_integer("debug.traced_light_blocking") > 0)
		cap = LIGHT_VISIBILITY_MAXIMUM;
	if (versus || traced_light_mode() <= 0 || cap <= 0 || window < 0 || window >= MAXIMUM_WINDOWS)
	{
		if (window >= 0 && window < MAXIMUM_WINDOWS)
			traced_lights.ramp[window].valid = 0;
		return;
	}
	ramp = &traced_lights.ramp[window];
	/* (a second eye of the same window and frame: the first's strengths, so the eyes never disagree) */
	if (ramp->valid && traced_lights.frame[window] == rasterizer_globals.fps_accumulation_frame_index)
	{
		for (i = 0; i < count; i++)
			for (j = 0; j < ramp->count; j++)
				if (ramp->id[j] == lights[i].id)
					strength[i] = ramp->strength[j];
		return;
	}
	traced_lights.frame[window] = rasterizer_globals.fps_accumulation_frame_index;
	light_visibility_rank(lights, count, (int)(cap < LIGHT_VISIBILITY_MAXIMUM ? cap : LIGHT_VISIBILITY_MAXIMUM),
		config_boolean("debug.traced_lights_fixtures"), chosen);
	light_visibility_ramp_step(ramp, lights, chosen, count, system_milliseconds(), strength);
	/* (how many lights a view has and traces, for the cost runs: over 600 window frames) */
	{
		static unsigned long views, total, owned, traced, most;
		unsigned long view_traced = 0;

		for (i = 0; i < count; i++)
		{
			owned += !light_visibility_owner_blocks(lights[i].owner_type);
			view_traced += strength[i] > 0.f;
		}
		total += (unsigned long)count;
		traced += view_traced;
		most = view_traced > most ? view_traced : most;
		if (++views == 600)
		{
			platform_log("traced lights: %lu views, %.2f lights a view, %.2f traced (most %lu), %.2f placed by the level",
				views, (double)total / views, (double)traced / views, most, (double)owned / views);
			views = total = owned = traced = most = 0;
		}
	}
}

/* the window's camera in a traced light's floats (traced_shadows_format.h's TRACED_SHADOWS_LIGHT): its position
(12-14), which turns each pixel's geometric normal toward the side it is seen from, and what rebuilds a pixel's
world point from its depth (nv2a_psh.c): the view's rectangle in the target's pixels (16-19), the depth's near and
far (20, 21; from the projection's depth row when it has one, as ambient occlusion does), the projection's p00, p11,
p20 and p21 (22-25) and view_to_world's forward, left and up with the scale in them, each beside one coordinate of
its position (28-39). Float 26 is the host's (a foveated pass). FALSE when the view has no rectangle or projection */
static boolean traced_light_view(
	float light[TRACED_SHADOWS_LIGHT_FLOATS])
{
	struct render_frustum const *frustum = &global_window_parameters.frustum;
	real_matrix4x3 const *view = &frustum->view_to_world;
	real const (*projection)[4] = frustum->projection_matrix;
	real scale = view->scale;

	light[12] = global_window_parameters.camera.position.x;
	light[13] = global_window_parameters.camera.position.y;
	light[14] = global_window_parameters.camera.position.z;
	if (!frustum->projection_valid || !halo_light_visibility_view(&light[16]))
		return FALSE;
	light[20] = frustum->z_near;
	light[21] = frustum->z_far;
	if (projection[2][2] != 1.f && projection[2][2] != -1.f && projection[3][2] != 0.f)
	{
		light[20] = projection[3][2] / (projection[2][2] - 1.f);
		light[21] = projection[3][2] / (projection[2][2] + 1.f);
	}
	light[22] = projection[0][0];
	light[23] = projection[1][1];
	light[24] = projection[2][0];
	light[25] = projection[2][1];
	light[28] = view->forward.i * scale;
	light[29] = view->forward.j * scale;
	light[30] = view->forward.k * scale;
	light[31] = view->position.x;
	light[32] = view->left.i * scale;
	light[33] = view->left.j * scale;
	light[34] = view->left.k * scale;
	light[35] = view->position.y;
	light[36] = view->up.i * scale;
	light[37] = view->up.j * scale;
	light[38] = view->up.k * scale;
	light[39] = view->position.z;
	return TRUE;
}

boolean traced_light_begin(
	real_point3d const *position,
	real radius,
	real strength,
	long family,
	short attenuation_stage,
	real attenuation_radius)
{
	static unsigned long calls;
	long mode = traced_light_mode();
	float light[TRACED_SHADOWS_LIGHT_FLOATS];

	/* (the level's structure goes with a window's traced shadows, this frame's) */
	if (mode <= 0 || strength <= 0.f || !traced.world || traced.frame != rasterizer_globals.fps_accumulation_frame_index)
		return FALSE;
	memset(light, 0, sizeof(light));
	light[0] = position->x;
	light[1] = position->y;
	light[2] = position->z;
	/* (the lit point is rebuilt from the pass's attenuation coordinate: stage light[15], over light[3]) */
	light[3] = attenuation_radius;
	light[15] = (float)attenuation_stage;
	light[4] = 0.01f; /* the ray's start past the lit point and its end short of the light, world units (3 cm) */
	light[5] = mode > 1 ? 1.f : 0.f;
	/* (what stands within this of the light does not block it: the owner's family, masked out by its bit when it has
	one, else by this distance) */
	light[6] = family ? light[4] : (float)config_real("debug.traced_light_owner");
	if (light[6] < light[4])
		light[6] = light[4];
	light[9] = (float)config_integer("debug.traced_light_repeat");
	light[10] = (float)strength;
	light[11] = (float)family;
	light[27] = config_boolean("debug.traced_light_depth_point") ? 1.f : 0.f;
	/* every ray ends this short of the light, so that a light resting on a surface (a stuck plasma grenade) or inside
	a projectile's model is not blocked by what it touches */
	light[7] = (float)(0.02f * radius > 0.05f ? 0.02f * radius : 0.05f);
	/* A light the viewer carries (within debug.traced_light_viewer of the eye): the flashlight sits a little ahead
	of the eye, past the corner of a wall the player stands against, and would lose its whole beam on that wall. Its
	rays stop short of it by its distance from the eye plus a margin, so what stands within that of the light does
	not block it. (Aiming the rays at the eye instead is not an option: from a visible point that ray is never
	blocked, so the flashlight would lose every shadow.) debug.traced_light_carried false turns the rule off */
	{
		real from_eye = distance3d(position, &render.camera.position);

		if (from_eye < (real)config_real("debug.traced_light_viewer") && config_boolean("debug.traced_light_carried"))
		{
			float carried = (float)(from_eye + config_real("debug.traced_light_stop_margin"));

			light[7] = carried > light[7] ? carried : light[7];
		}
	}
	/* (where the light sits against the eye: a light inside a wall the player hugs blocks itself) */
	if ((calls++ % 600) == 0)
		platform_log("traced light: %.2f %.2f %.2f radius %.1f, %.3f from the eye", position->x, position->y,
			position->z, radius, (float)distance3d(position, &render.camera.position));
	/* (a pass without an attenuation coordinate rebuilds its point from depth, which needs the view's rectangle) */
	if (!traced_light_view(light) && (attenuation_stage == 0 || light[27] > 0.f))
		return FALSE;
	/* (the second eye's replay draws these draws again for eye 1: its record carries eye 1's camera) */
	if (rasterizer_other_eye_draws())
	{
		halo_stereo_replay_eye(HALO_STEREO_REPLAY_EYE_0);
		host_traced_shadows_data(TRACED_SHADOWS_LIGHT, 0, light, sizeof(light));
		if (rasterizer_other_eye_begin())
		{
			halo_stereo_replay_eye(HALO_STEREO_REPLAY_EYE_1);
			traced_light_view(light);
			host_traced_shadows_data(TRACED_SHADOWS_LIGHT, 0, light, sizeof(light));
			rasterizer_other_eye_end();
		}
		halo_stereo_replay_eye(HALO_STEREO_REPLAY_BOTH);
	}
	else
		host_traced_shadows_data(TRACED_SHADOWS_LIGHT, 0, light, sizeof(light));
	halo_light_visibility_key(LIGHT_VISIBILITY_TRACED);
	return TRUE;
}

void traced_light_end(
	void)
{
	halo_light_visibility_key(LIGHT_VISIBILITY_NONE);
}

#else

void traced_shadow_casters_new_map(void) { }
boolean traced_shadow_casters_window_begin(real p11) { (void)p11; return FALSE; }
void traced_shadow_casters_window_end(void) { }
unsigned long traced_shadow_casters_begin(long object_index) { (void)object_index; return TRACED_SHADOW_NONE; }
void traced_shadow_casters_part(struct triangle_buffer const *triangle_buffer, struct vertex_buffer const *vertex_buffer,
	struct render_skinning const *skinning) { (void)triangle_buffer; (void)vertex_buffer; (void)skinning; }
void traced_shadow_casters_end(void) { }
void traced_shadow_casters_commit(void) { }
void traced_shadow_apply(unsigned long id, real_matrix4x3 const *shadow_matrix, real radius)
	{ (void)id; (void)shadow_matrix; (void)radius; }
void traced_shadow_apply_end(void) { }
boolean traced_light_walks_casters(void) { return FALSE; }
void traced_lights_choose(struct light_visibility_candidate const *lights, short count, boolean versus, float *strength)
	{ short i; (void)lights; (void)versus; for (i = 0; i < count; i++) strength[i] = 0.f; }
boolean traced_light_begin(real_point3d const *position, real radius, real strength, long family,
	short attenuation_stage, real attenuation_radius)
	{ (void)position; (void)radius; (void)strength; (void)family; (void)attenuation_stage; (void)attenuation_radius;
	return FALSE; }
void traced_light_end(void) { }

#endif
