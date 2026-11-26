/*
TRACED_SHADOWS.H

The parts of traced object shadows (display.traced_shadows; gpu_metal.m) that need no Metal: checking the game's
records (port/linux/src/traced_shadows_format.h), skinning a part's vertices, and the caster cache, which decides for
each shadow's caster whether its acceleration structure is reused as it is, updated in place, or built.

Plain C in a header so that port/ios/tests/traced_shadows_probe.c runs the same code on any host
(tools/test_traced_shadows.py).

The cache. A structure belongs to an object and a shape (which parts under which nodes): two windows of a split
screen may draw one object's shadow at different levels of detail, and those are two structures. Each frame a caster's
record is compared with what its structure was last made from:
- the same pose: reuse (a parked vehicle, a crate, a body; the other eye's or window's shadow of the same frame);
- another pose: update in place (a refit), or build again when refits are off;
- no structure of that shape: build;
- TRACED_REFIT_LIMIT refits since its build: build again, at most TRACED_REBUILDS_A_FRAME such a frame, so an animation
  that has moved far from the pose a structure was built in does not trace slowly forever.
A slot not used for TRACED_IDLE_FRAMES frames is free for another caster, and its structure is released.
Refits are spread over frames when there are too many: when the last frame's moved casters (those with a structure)
came to more than TRACED_FRAME_UPDATE_TRIANGLES, each is refitted every period-th frame (period = that total over the
budget, rounded up, at most TRACED_STALE_FRAMES + 1), in a phase set by its slot, and keeps its last structure (a
shadow a frame or two behind its pose) in between; new casters are always built. A 64-player match moved about
140,000 triangles of casters a frame, all refitted every frame before this.
Past TRACED_FRAME_TRIANGLES of work in a frame, casters are taken in the order the game sends them (not by distance):
a later one with a structure of an older pose keeps it, for up to TRACED_STALE_FRAMES frames, as a deferred refit
does; one without a structure has no shadow that frame.
*/

#ifndef __HALO_TRACED_SHADOWS_H
#define __HALO_TRACED_SHADOWS_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "traced_shadows_format.h"

enum
{
	TRACED_CASTER_SLOTS = 1024, /* a power of two */
	TRACED_CASTER_PROBES = 32,
	/* refits of one structure before it is built again: each refit keeps the build's boxes and only widens them, so
	a structure refitted every frame gets slower to trace; 120 is two seconds of refits at 60 Hz */
	TRACED_REFIT_LIMIT = 120,
	/* such rebuilds a frame, so casters that reach the limit together (a squad placed at once) do not all build in
	the same frame */
	TRACED_REBUILDS_A_FRAME = 4,
	TRACED_IDLE_FRAMES = 60,
	/* slots a frame's sweep looks at for idle casters to release (the whole table in about half a second) */
	TRACED_SWEEP_SLOTS = 32,
	/* triangles of every caster built or updated in a frame; a caster past it keeps a structure of an older pose
	(TRACED_STALE_FRAMES at most) or has no shadow that frame */
	TRACED_FRAME_TRIANGLES = 200000,
	/* the refit budget a frame: moved casters past it are refitted every few frames instead of every frame. A
	64-player match moved about 140,000 triangles of casters a frame; at 48,000 that is a period of 3, inside
	TRACED_STALE_FRAMES + 1, and that match then updated about 45,000 a frame */
	TRACED_FRAME_UPDATE_TRIANGLES = 48000,
	/* frames in a row a moved caster may keep a structure of an older pose: a shadow at most 50 ms behind its
	caster at 60 Hz */
	TRACED_STALE_FRAMES = 3,
};

enum traced_decision
{
	TRACED_REFUSED, /* a malformed record, another epoch's, or no free slot: no shadow */
	TRACED_REUSE,
	TRACED_REFIT,
	TRACED_BUILD,
};

/* ---------- records */

/* a part record's vertices and triangles, or 0 when the record is not well formed */
static inline int traced_part_read(const void *bytes, uint32_t size, const struct traced_shadows_part **part,
	const struct traced_shadows_vertex **vertices, const uint16_t **triangles)
{
	const struct traced_shadows_part *header = (const struct traced_shadows_part *)bytes;
	uint64_t wanted;
	uint32_t index;

	if (!bytes || size < sizeof(*header) || !header->vertex_count || !header->triangle_count ||
		header->vertex_count > TRACED_SHADOWS_PART_VERTICES || header->triangle_count > 0x100000)
		return 0;
	wanted = sizeof(*header) + (uint64_t)header->vertex_count * sizeof(struct traced_shadows_vertex) +
		(((uint64_t)header->triangle_count * 3 * sizeof(uint16_t) + 3) & ~(uint64_t)3);
	if (size != wanted)
		return 0;
	*part = header;
	*vertices = (const struct traced_shadows_vertex *)(header + 1);
	*triangles = (const uint16_t *)(*vertices + header->vertex_count);
	for (index = 0; index < header->triangle_count * 3; index++)
		if ((*triangles)[index] >= header->vertex_count)
			return 0;
	return 1;
}

/* a caster record's shape and pose, or 0 when the record is not well formed: the sizes add up, the shape's
models fit it, and the pose holds every model's nodes */
static inline int traced_caster_read(const void *bytes, uint32_t size, const struct traced_shadows_caster **caster,
	const uint32_t **shape, const float **pose)
{
	const struct traced_shadows_caster *header = (const struct traced_shadows_caster *)bytes;
	const uint32_t *words;
	uint64_t at = 0, nodes = 0;
	uint32_t model, limit;

	if (!bytes || size < sizeof(*header) || (header->shape_size & 3) || (header->pose_size & 3) ||
		(uint64_t)sizeof(*header) + header->shape_size + header->pose_size != size || !header->model_count)
		return 0;
	words = (const uint32_t *)(header + 1);
	limit = header->shape_size / 4;
	for (model = 0; model < header->model_count; model++)
	{
		if (at + 2 > limit || !words[at] || words[at] > 255 || at + 2 + words[at + 1] > limit)
			return 0;
		nodes += words[at];
		at += 2 + (uint64_t)words[at + 1];
	}
	if (at != limit || nodes * TRACED_SHADOWS_NODE_FLOATS * sizeof(float) != header->pose_size)
		return 0;
	*caster = header;
	*shape = words;
	*pose = (const float *)(words + limit);
	return 1;
}

/* ---------- skinning */

/* count vertices of a part under node_count node matrices (TRACED_SHADOWS_NODE_FLOATS floats each), as the game's
shadow vertex shader places them: each a blend of its two nodes' transforms by its weight. A node past the model's
is the first. Written three floats a vertex */
static inline void traced_skin(const struct traced_shadows_vertex *vertices, uint32_t count, const float *matrices,
	uint32_t node_count, float *positions)
{
	uint32_t index;

	for (index = 0; index < count; index++)
	{
		const struct traced_shadows_vertex *vertex = &vertices[index];
		const float *a = matrices + (size_t)(vertex->nodes[0] < node_count ? vertex->nodes[0] : 0) * TRACED_SHADOWS_NODE_FLOATS;
		const float *b = matrices + (size_t)(vertex->nodes[1] < node_count ? vertex->nodes[1] : 0) * TRACED_SHADOWS_NODE_FLOATS;
		float x = vertex->position[0], y = vertex->position[1], z = vertex->position[2];
		float wa = vertex->weight * a[0], wb = (1.0f - vertex->weight) * b[0];
		float wpa = vertex->weight, wpb = 1.0f - vertex->weight;
		int axis;

		/* scale * (forward x + left y + up z) + position, forward at 1, left at 4, up at 7, position at 10 */
		for (axis = 0; axis < 3; axis++)
			positions[(size_t)index * 3 + axis] =
				wa * (a[1 + axis] * x + a[4 + axis] * y + a[7 + axis] * z) + wpa * a[10 + axis] +
				wb * (b[1 + axis] * x + b[4 + axis] * y + b[7 + axis] * z) + wpb * b[10 + axis];
	}
}

/* ---------- the caster cache */

struct traced_caster_slot
{
	uint32_t used;
	uint32_t object;
	/* copies of the shape and pose its structure was last made from */
	uint32_t *shape;
	float *pose;
	uint32_t shape_size, pose_size;
	uint32_t shape_capacity, pose_capacity;
	/* the frame it was last asked for, and last built or updated in */
	uint64_t frame, updated;
	uint32_t refits;
	uint32_t triangles;
};

struct traced_caster_cache
{
	struct traced_caster_slot slots[TRACED_CASTER_SLOTS];
	uint64_t frame;
	uint32_t rebuilds; /* periodic ones this frame */
	uint32_t sweep; /* where the next frame's sweep starts */
	uint32_t frame_triangles;
	/* since the last log line */
	uint32_t reuses, refits, builds, refused, deferred;
	/* triangles of the moved casters with a structure, this frame and the last, and the refit period they set */
	uint32_t moved_triangles, last_moved_triangles, period;
};

static inline uint32_t traced_shape_hash(uint32_t object, const uint32_t *shape, uint32_t shape_size)
{
	uint32_t hash = object * 2654435761u, index;

	for (index = 0; index < shape_size / 4; index++)
		hash = (hash ^ shape[index]) * 16777619u;
	return hash ^ (hash >> 15);
}

/* a new frame: the periodic rebuilds' and the triangles' budgets start again */
static inline void traced_cache_frame(struct traced_caster_cache *cache, uint64_t frame)
{
	if (cache->frame == frame)
		return;
	cache->frame = frame;
	cache->rebuilds = 0;
	cache->frame_triangles = 0;
	cache->last_moved_triangles = cache->moved_triangles;
	cache->moved_triangles = 0;
	cache->period = (cache->last_moved_triangles + TRACED_FRAME_UPDATE_TRIANGLES - 1) / TRACED_FRAME_UPDATE_TRIANGLES;
	cache->period = cache->period < 1 ? 1 : cache->period > TRACED_STALE_FRAMES + 1 ? TRACED_STALE_FRAMES + 1 : cache->period;
}

/* a slot's copies and counts are dropped; the caller releases its structure */
static inline void traced_slot_clear(struct traced_caster_slot *slot)
{
	free(slot->shape);
	free(slot->pose);
	memset(slot, 0, sizeof(*slot));
}

static inline void traced_cache_reset(struct traced_caster_cache *cache)
{
	uint32_t index;

	for (index = 0; index < TRACED_CASTER_SLOTS; index++)
		traced_slot_clear(&cache->slots[index]);
	cache->rebuilds = cache->frame_triangles = cache->sweep = 0;
}

/* A decision that was not carried out (a build or refit dropped before its commit, an allocation that failed): the
slot is forgotten as the sweep forgets one, so the caster's next decision builds its structure from nothing instead of
reusing or refitting one that was never built. The caller releases the slot's structure. */
static inline void traced_cache_forget(struct traced_caster_cache *cache, uint32_t slot_index)
{
	traced_slot_clear(&cache->slots[slot_index & (TRACED_CASTER_SLOTS - 1)]);
}

/* A frame's share of the sweep that releases casters nothing has asked for in TRACED_IDLE_FRAMES frames (a player who
left the view, a body that faded, a level of detail no longer drawn), so the structures' memory follows what is on
screen and not everything that ever was: call until it answers 0; each 1 names a slot (*slot_index) that is now
clear and whose structure the caller releases */
static inline int traced_cache_sweep(struct traced_caster_cache *cache, uint32_t *looked, uint32_t *slot_index)
{
	while (*looked < TRACED_SWEEP_SLOTS)
	{
		struct traced_caster_slot *slot = &cache->slots[cache->sweep];

		cache->sweep = (cache->sweep + 1) & (TRACED_CASTER_SLOTS - 1);
		(*looked)++;
		if (slot->used && slot->frame + TRACED_IDLE_FRAMES < cache->frame)
		{
			*slot_index = (uint32_t)(slot - cache->slots);
			traced_slot_clear(slot);
			return 1;
		}
	}
	return 0;
}

static inline int traced_slot_copy(struct traced_caster_slot *slot, const uint32_t *shape, uint32_t shape_size,
	const float *pose, uint32_t pose_size)
{
	if (slot->shape_capacity < shape_size)
	{
		uint32_t *grown = (uint32_t *)realloc(slot->shape, shape_size);

		if (!grown)
			return 0;
		slot->shape = grown;
		slot->shape_capacity = shape_size;
	}
	if (slot->pose_capacity < pose_size)
	{
		float *grown = (float *)realloc(slot->pose, pose_size);

		if (!grown)
			return 0;
		slot->pose = grown;
		slot->pose_capacity = pose_size;
	}
	memcpy(slot->shape, shape, shape_size);
	memcpy(slot->pose, pose, pose_size);
	slot->shape_size = shape_size;
	slot->pose_size = pose_size;
	return 1;
}

/* What to do with a caster this frame, and the slot whose structure it is (*slot_index; *released is 1 when the
slot held another caster's structure, which the caller releases before building). triangles is the caster's own
count, refit_allowed the debug switch. A caster already updated this frame in another pose (which the game does not
do, but a structure must never be updated twice before its draws) takes another slot */
static inline enum traced_decision traced_cache_decide(struct traced_caster_cache *cache, uint32_t object,
	const uint32_t *shape, uint32_t shape_size, const float *pose, uint32_t pose_size, uint32_t triangles,
	int refit_allowed, uint32_t *slot_index, int *released)
{
	uint32_t hash = traced_shape_hash(object, shape, shape_size), probe;
	struct traced_caster_slot *found = NULL, *spare = NULL;
	enum traced_decision decision;

	*released = 0;
	for (probe = 0; probe < TRACED_CASTER_PROBES; probe++)
	{
		struct traced_caster_slot *slot = &cache->slots[(hash + probe) & (TRACED_CASTER_SLOTS - 1)];

		if (slot->used && slot->object == object && slot->shape_size == shape_size &&
			!memcmp(slot->shape, shape, shape_size))
		{
			int same_pose = slot->pose_size == pose_size && !memcmp(slot->pose, pose, pose_size);

			if (same_pose || slot->updated != cache->frame)
			{
				found = slot;
				break;
			}
			continue;
		}
		if (!spare && (!slot->used || slot->frame + TRACED_IDLE_FRAMES < cache->frame))
			spare = slot;
	}
	if (found && found->pose_size == pose_size && !memcmp(found->pose, pose, pose_size))
	{
		found->frame = cache->frame;
		cache->reuses++;
		*slot_index = (uint32_t)(found - cache->slots);
		return TRACED_REUSE;
	}
	if (cache->frame_triangles + triangles > TRACED_FRAME_TRIANGLES)
	{
		/* (past the frame's work: a caster with a structure of an older pose keeps it, as a deferred refit does) */
		if (found && found->updated && found->updated != cache->frame &&
			cache->frame - found->updated <= TRACED_STALE_FRAMES)
		{
			found->frame = cache->frame;
			cache->deferred++;
			*slot_index = (uint32_t)(found - cache->slots);
			return TRACED_REUSE;
		}
		cache->refused++;
		return TRACED_REFUSED;
	}
	/* too many moved casters last frame: this one is refitted in its phase of the period, and keeps its structure
	otherwise (never one older than the period) */
	if (found && found->updated && found->updated != cache->frame)
	{
		uint32_t index = (uint32_t)(found - cache->slots);

		cache->moved_triangles += triangles;
		if (cache->period > 1 && (cache->frame + index) % cache->period != 0 &&
			cache->frame - found->updated < cache->period)
		{
			found->frame = cache->frame;
			cache->deferred++;
			*slot_index = index;
			return TRACED_REUSE;
		}
	}
	if (found)
	{
		decision = TRACED_REFIT;
		if (!refit_allowed)
			decision = TRACED_BUILD;
		else if (found->refits >= TRACED_REFIT_LIMIT && cache->rebuilds < TRACED_REBUILDS_A_FRAME)
		{
			decision = TRACED_BUILD;
			cache->rebuilds++;
		}
	}
	else
	{
		if (!spare)
		{
			cache->refused++;
			return TRACED_REFUSED;
		}
		*released = spare->used != 0;
		found = spare;
		found->used = 1;
		found->object = object;
		found->refits = 0;
		decision = TRACED_BUILD;
	}
	if (!traced_slot_copy(found, shape, shape_size, pose, pose_size))
	{
		/* (its structure, if any, no longer matches a pose the cache knows: drop the slot) */
		*released = 1;
		*slot_index = (uint32_t)(found - cache->slots);
		traced_slot_clear(found);
		cache->refused++;
		return TRACED_REFUSED;
	}
	found->frame = found->updated = cache->frame;
	found->triangles = triangles;
	found->refits = decision == TRACED_REFIT ? found->refits + 1 : 0;
	cache->frame_triangles += triangles;
	if (decision == TRACED_REFIT)
		cache->refits++;
	else
		cache->builds++;
	*slot_index = (uint32_t)(found - cache->slots);
	return decision;
}

#endif
