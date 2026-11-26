/* Traced object shadows without Metal (port/ios/host/traced_shadows.h): the records' checks, the skinning against a
reference blend, and the caster cache's decisions. Run by tools/test_traced_shadows.py. */
#include <math.h>
#include <stdio.h>

#include "traced_cover.h"
#include "traced_shadows.h"

static int failures;

#define CHECK(condition, text) do { if (!(condition)) { fprintf(stderr, "traced shadows: %s (line %d)\n", text, __LINE__); failures++; } } while (0)

/* a node matrix: scale, a rotation about z by angle, a position */
static void node(float *m, float scale, float angle, float x, float y, float z)
{
	m[0] = scale;
	m[1] = cosf(angle); m[2] = sinf(angle); m[3] = 0.0f;
	m[4] = -sinf(angle); m[5] = cosf(angle); m[6] = 0.0f;
	m[7] = 0.0f; m[8] = 0.0f; m[9] = 1.0f;
	m[10] = x; m[11] = y; m[12] = z;
}

static void reference(const float *m, const float *p, float *out)
{
	int axis;

	for (axis = 0; axis < 3; axis++)
		out[axis] = m[0] * (m[1 + axis] * p[0] + m[4 + axis] * p[1] + m[7 + axis] * p[2]) + m[10 + axis];
}

static void check_skinning(void)
{
	float matrices[3 * TRACED_SHADOWS_NODE_FLOATS], out[4 * 3], a[3], b[3];
	struct traced_shadows_vertex vertices[4] = {
		{ { 1.0f, 2.0f, 3.0f }, 1.0f, { 0, 1 }, { 0, 0 } },
		{ { 1.0f, 2.0f, 3.0f }, 0.0f, { 0, 1 }, { 0, 0 } },
		{ { -0.5f, 0.25f, 2.0f }, 0.3f, { 2, 1 }, { 0, 0 } },
		/* a node the model does not have is its first */
		{ { 1.0f, 0.0f, 0.0f }, 1.0f, { 9, 0 }, { 0, 0 } },
	};
	int axis;

	node(matrices, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	node(matrices + TRACED_SHADOWS_NODE_FLOATS, 2.0f, 1.0f, 5.0f, -1.0f, 0.5f);
	node(matrices + 2 * TRACED_SHADOWS_NODE_FLOATS, 0.5f, -0.7f, 0.0f, 3.0f, 1.0f);
	traced_skin(vertices, 4, matrices, 3, out);
	reference(matrices, vertices[0].position, a);
	for (axis = 0; axis < 3; axis++)
		CHECK(fabsf(out[axis] - a[axis]) < 1e-5f, "a vertex wholly on its first node is that node's transform");
	reference(matrices + TRACED_SHADOWS_NODE_FLOATS, vertices[1].position, b);
	for (axis = 0; axis < 3; axis++)
		CHECK(fabsf(out[3 + axis] - b[axis]) < 1e-5f, "a vertex wholly on its second node is that node's transform");
	reference(matrices + 2 * TRACED_SHADOWS_NODE_FLOATS, vertices[2].position, a);
	reference(matrices + TRACED_SHADOWS_NODE_FLOATS, vertices[2].position, b);
	for (axis = 0; axis < 3; axis++)
		CHECK(fabsf(out[6 + axis] - (0.3f * a[axis] + 0.7f * b[axis])) < 1e-5f, "a blended vertex is the weighted sum");
	reference(matrices, vertices[3].position, a);
	for (axis = 0; axis < 3; axis++)
		CHECK(fabsf(out[9 + axis] - a[axis]) < 1e-5f, "a node past the model's is its first");
}

static void check_records(void)
{
	struct
	{
		struct traced_shadows_part header;
		struct traced_shadows_vertex vertices[3];
		uint16_t triangles[4];
	} part;
	struct
	{
		struct traced_shadows_caster header;
		uint32_t shape[4];
		float pose[2 * TRACED_SHADOWS_NODE_FLOATS];
	} caster = { { 1, 7, 1, 16, sizeof(float) * 2 * TRACED_SHADOWS_NODE_FLOATS, 0 }, { 2, 2, 10, 11 }, { 0 } };
	const struct traced_shadows_part *part_header;
	const struct traced_shadows_vertex *vertices;
	const uint16_t *triangles;
	const struct traced_shadows_caster *caster_header;
	const uint32_t *shape;
	const float *pose;

	memset(&part, 0, sizeof(part));
	part.header.epoch = 1;
	part.header.vertex_count = 3;
	part.header.triangle_count = 1;
	part.triangles[1] = 1;
	part.triangles[2] = 2;
	CHECK(traced_part_read(&part, sizeof(part), &part_header, &vertices, &triangles), "a well-formed part is read");
	CHECK(!traced_part_read(&part, sizeof(part) - 4, &part_header, &vertices, &triangles), "a short part is refused");
	part.triangles[2] = 3;
	CHECK(!traced_part_read(&part, sizeof(part), &part_header, &vertices, &triangles),
		"a triangle that names a vertex the part does not have is refused");
	CHECK(traced_caster_read(&caster, sizeof(caster), &caster_header, &shape, &pose) && shape[2] == 10 &&
		pose == caster.pose, "a well-formed caster is read");
	caster.shape[1] = 3;
	CHECK(!traced_caster_read(&caster, sizeof(caster), &caster_header, &shape, &pose),
		"a model with more parts than the shape holds is refused");
	caster.shape[1] = 2;
	caster.shape[0] = 3;
	CHECK(!traced_caster_read(&caster, sizeof(caster), &caster_header, &shape, &pose),
		"a pose that does not hold every node is refused");
}

static void check_cache(void)
{
	static struct traced_caster_cache cache;
	uint32_t shape_a[4] = { 2, 2, 10, 11 }, shape_b[3] = { 2, 1, 12 };
	float pose[2 * TRACED_SHADOWS_NODE_FLOATS] = { 0 }, moved[2 * TRACED_SHADOWS_NODE_FLOATS] = { 1.0f };
	uint32_t slot, first, other, frame;
	int released;

	traced_cache_frame(&cache, 1);
	CHECK(traced_cache_decide(&cache, 7, shape_a, sizeof(shape_a), pose, sizeof(pose), 100, 1, &first, &released) ==
		TRACED_BUILD && !released, "a caster seen first is built");
	CHECK(traced_cache_decide(&cache, 7, shape_a, sizeof(shape_a), pose, sizeof(pose), 100, 1, &slot, &released) ==
		TRACED_REUSE && slot == first, "the same caster again in a frame (the other eye's) is reused");
	/* the class: one object at two levels of detail in one frame is two structures, neither rebuilt for the other */
	CHECK(traced_cache_decide(&cache, 7, shape_b, sizeof(shape_b), pose, sizeof(pose), 50, 1, &other, &released) ==
		TRACED_BUILD && other != first, "another shape of the same object has its own structure");
	CHECK(traced_cache_decide(&cache, 7, shape_a, sizeof(shape_a), pose, sizeof(pose), 100, 1, &slot, &released) ==
		TRACED_REUSE && slot == first, "and the first shape's is still there");
	/* the class: a structure is never updated twice before its draws */
	CHECK(traced_cache_decide(&cache, 7, shape_a, sizeof(shape_a), moved, sizeof(moved), 100, 1, &slot, &released) ==
		TRACED_BUILD && slot != first, "a second pose of one shape in one frame takes another slot");
	traced_cache_frame(&cache, 2);
	CHECK(traced_cache_decide(&cache, 7, shape_a, sizeof(shape_a), pose, sizeof(pose), 100, 1, &slot, &released) ==
		TRACED_REUSE, "a caster that has not moved since last frame is reused");
	CHECK(traced_cache_decide(&cache, 7, shape_b, sizeof(shape_b), moved, sizeof(moved), 50, 1, &slot, &released) ==
		TRACED_REFIT && slot == other, "a caster that moved is updated in place");
	traced_cache_frame(&cache, 3);
	moved[1] = 2.0f;
	CHECK(traced_cache_decide(&cache, 7, shape_b, sizeof(shape_b), moved, sizeof(moved), 50, 0, &slot, &released) ==
		TRACED_BUILD && slot == other, "with refits off a caster that moved is built again");
	/* TRACED_REFIT_LIMIT refits, then one build */
	for (frame = 4; frame < 4 + TRACED_REFIT_LIMIT; frame++)
	{
		traced_cache_frame(&cache, frame);
		moved[2] = (float)frame;
		CHECK(traced_cache_decide(&cache, 7, shape_b, sizeof(shape_b), moved, sizeof(moved), 50, 1, &slot, &released) ==
			TRACED_REFIT, "a moving caster is refitted up to the limit");
	}
	traced_cache_frame(&cache, frame);
	moved[2] = -1.0f;
	CHECK(traced_cache_decide(&cache, 7, shape_b, sizeof(shape_b), moved, sizeof(moved), 50, 1, &slot, &released) ==
		TRACED_BUILD && slot == other, "past the refit limit a caster is built again");
	/* the triangle budget */
	traced_cache_frame(&cache, frame + 1);
	CHECK(traced_cache_decide(&cache, 8, shape_a, sizeof(shape_a), pose, sizeof(pose), TRACED_FRAME_TRIANGLES + 1, 1, &slot,
		&released) == TRACED_REFUSED, "a caster past the frame's triangles is refused");
	CHECK(traced_cache_decide(&cache, 7, shape_a, sizeof(shape_a), pose, sizeof(pose), TRACED_FRAME_TRIANGLES + 1, 1, &slot,
		&released) == TRACED_REUSE, "but one that needs no work is still reused");
	/* a new map: nothing of the last one is found */
	traced_cache_reset(&cache);
	traced_cache_frame(&cache, frame + 2);
	CHECK(traced_cache_decide(&cache, 7, shape_a, sizeof(shape_a), pose, sizeof(pose), 100, 1, &slot, &released) ==
		TRACED_BUILD && !released, "after a reset every caster is built");
	traced_cache_reset(&cache);
}

/* the class: a decision that was not carried out is forgotten (a build dropped before its commit once left the slot
looking built, and the next frame reused a structure that had never been built) */
static void check_forget(void)
{
	static struct traced_caster_cache cache;
	uint32_t shape[3] = { 1, 1, 0 }, slot, again;
	float pose[TRACED_SHADOWS_NODE_FLOATS] = { 0 }, moved[TRACED_SHADOWS_NODE_FLOATS] = { 1.0f };
	int released;

	traced_cache_frame(&cache, 1);
	CHECK(traced_cache_decide(&cache, 3, shape, sizeof(shape), pose, sizeof(pose), 10, 1, &slot, &released) ==
		TRACED_BUILD, "a new caster is built");
	traced_cache_forget(&cache, slot);
	traced_cache_frame(&cache, 2);
	CHECK(traced_cache_decide(&cache, 3, shape, sizeof(shape), pose, sizeof(pose), 10, 1, &again, &released) ==
		TRACED_BUILD, "a forgotten build is built again, not reused");
	traced_cache_frame(&cache, 3);
	CHECK(traced_cache_decide(&cache, 3, shape, sizeof(shape), moved, sizeof(moved), 10, 1, &slot, &released) ==
		TRACED_REFIT, "a built caster that moved is refitted");
	traced_cache_forget(&cache, slot);
	traced_cache_frame(&cache, 4);
	CHECK(traced_cache_decide(&cache, 3, shape, sizeof(shape), moved, sizeof(moved), 10, 1, &slot, &released) ==
		TRACED_BUILD, "a forgotten refit is built from nothing, not refitted or reused");
	traced_cache_reset(&cache);
}

/* an idle slot goes to a new caster, which is told to release what was there */
static void check_idle(void)
{
	static struct traced_caster_cache cache;
	uint32_t shape[3] = { 1, 1, 0 }, slot, object;
	float pose[TRACED_SHADOWS_NODE_FLOATS] = { 0 };
	int released, any_released = 0, refused = 0;

	traced_cache_frame(&cache, 1);
	for (object = 0; object < TRACED_CASTER_SLOTS * 2; object++)
		if (traced_cache_decide(&cache, object, shape, sizeof(shape), pose, sizeof(pose), 1, 1, &slot, &released) ==
			TRACED_REFUSED)
			refused++;
	CHECK(refused > 0, "a full cache refuses a caster rather than taking a slot in use");
	traced_cache_frame(&cache, 2 + TRACED_IDLE_FRAMES);
	for (object = 100000; object < 100000 + 64; object++)
	{
		CHECK(traced_cache_decide(&cache, object, shape, sizeof(shape), pose, sizeof(pose), 1, 1, &slot, &released) ==
			TRACED_BUILD, "idle slots are taken by new casters");
		any_released |= released;
	}
	CHECK(any_released, "and the caller is told to release the old structure");
	traced_cache_reset(&cache);
}

/* the class: a cache that only gives a slot back when another caster hashes to it keeps every structure ever made.
The sweep releases every idle caster within a bounded number of frames, a bounded number of slots a frame, and
never one asked for lately */
static void check_sweep(void)
{
	static struct traced_caster_cache cache;
	uint32_t shape[3] = { 1, 1, 0 }, slot, object, looked, frame, kept_slot, released_count = 0, used = 0, index;
	float pose[TRACED_SHADOWS_NODE_FLOATS] = { 0 };
	int released;

	traced_cache_frame(&cache, 1);
	for (object = 0; object < 300; object++)
		traced_cache_decide(&cache, object, shape, sizeof(shape), pose, sizeof(pose), 1, 1, &slot, &released);
	looked = 0;
	CHECK(!traced_cache_sweep(&cache, &looked, &slot) && looked == TRACED_SWEEP_SLOTS,
		"the sweep leaves casters asked for this frame, and looks at its share of the slots only");
	/* one caster stays in view; the rest are never asked for again */
	for (frame = 2; frame <= 2 + TRACED_IDLE_FRAMES + TRACED_CASTER_SLOTS / TRACED_SWEEP_SLOTS; frame++)
	{
		traced_cache_frame(&cache, frame);
		CHECK(traced_cache_decide(&cache, 7, shape, sizeof(shape), pose, sizeof(pose), 1, 1, &kept_slot, &released) ==
			TRACED_REUSE, "a caster in view is reused through the sweeps");
		looked = 0;
		while (traced_cache_sweep(&cache, &looked, &slot))
		{
			CHECK(slot != kept_slot, "the sweep never releases a caster in view");
			CHECK(!cache.slots[slot].used, "a swept slot is clear");
			released_count++;
		}
		CHECK(looked == TRACED_SWEEP_SLOTS, "each frame's sweep is bounded");
	}
	for (index = 0; index < TRACED_CASTER_SLOTS; index++)
		used += cache.slots[index].used != 0;
	CHECK(released_count == 299 && used == 1, "every idle caster is released within the idle time and one pass of the table");
	traced_cache_reset(&cache);
}

/* the class: refit work a frame grew with the casters in view (about 140,000 triangles a frame at 64 players). Past
the budget each moved caster is refitted every few frames in its own phase, so a frame's refits stay near the budget,
no caster keeps an older pose's structure longer than the stale limit, and a new caster is always built */
static void check_update_budget(void)
{
	static struct traced_caster_cache cache;
	enum { CASTERS = 20, TRIANGLES = 7000, FRAMES = 60 }; /* 140,000 triangles: under TRACED_FRAME_TRIANGLES */
	uint32_t shape[3] = { 1, 1, 0 }, slot, object, frame, kept[CASTERS] = { 0 }, worst_kept = 0, worst_frame = 0;
	float pose[TRACED_SHADOWS_NODE_FLOATS] = { 0 };
	int released;

	for (frame = 1; frame <= FRAMES; frame++)
	{
		uint32_t updated = 0;

		traced_cache_frame(&cache, frame);
		pose[0] = (float)frame; /* every caster moves every frame */
		for (object = 0; object < CASTERS; object++)
		{
			enum traced_decision decision = traced_cache_decide(&cache, object, shape, sizeof(shape), pose, sizeof(pose),
				TRIANGLES, 1, &slot, &released);

			CHECK(decision != TRACED_REFUSED, "every caster keeps a shadow");
			if (frame == 1)
				CHECK(decision == TRACED_BUILD, "a new caster is always built");
			if (decision == TRACED_REUSE)
				worst_kept = ++kept[object] > worst_kept ? kept[object] : worst_kept;
			else
			{
				kept[object] = 0;
				updated += TRIANGLES;
			}
		}
		if (frame > 2 && updated > worst_frame)
			worst_frame = updated;
	}
	CHECK(worst_kept <= TRACED_STALE_FRAMES, "no caster keeps an older pose's structure past the stale limit");
	CHECK(worst_frame <= CASTERS * TRIANGLES / 2, "past the budget a frame refits a share of the moved casters, not all");
	traced_cache_reset(&cache);
}

/* a box's eight view-space corners, x in [x0, x1], y in [y0, y1], z in [z0, z1] (corner bits 0, 1, 2 pick the far
side) */
static void cover_box(float views[8][3], float x0, float x1, float y0, float y1, float z0, float z1)
{
	int corner;

	for (corner = 0; corner < 8; corner++)
	{
		views[corner][0] = corner & 1 ? x1 : x0;
		views[corner][1] = corner & 2 ? y1 : y0;
		views[corner][2] = corner & 4 ? z1 : z0;
	}
}

/* what a shadow's volume covers on screen (traced_cover.h). The class: a volume reaching behind the near plane,
whose behind corners either project mirrored or stand in for the whole window. */
static void check_cover(void)
{
	const struct traced_cover_projection projection = { 1.0f, 0.0f, 1.0f, 0.0f, 0.1f };
	float views[8][3], bounds[4];

	cover_box(views, -1.0f, 1.0f, -1.0f, 1.0f, -12.0f, -8.0f);
	CHECK(traced_cover_box(&projection, (const float (*)[3])views, bounds), "a volume in front covers something");
	CHECK(fabsf(bounds[0] + 0.125f) < 1e-6f && fabsf(bounds[1] + 0.125f) < 1e-6f &&
		fabsf(bounds[2] - 0.125f) < 1e-6f && fabsf(bounds[3] - 0.125f) < 1e-6f,
		"a volume in front covers its corners' projection");
	cover_box(views, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 3.0f);
	CHECK(!traced_cover_box(&projection, (const float (*)[3])views, bounds), "a volume wholly behind covers nothing");
	/* beside the camera and reaching behind it: only the right of the window, out past its edge */
	cover_box(views, 2.0f, 4.0f, -1.0f, 1.0f, -5.0f, 5.0f);
	CHECK(traced_cover_box(&projection, (const float (*)[3])views, bounds), "a volume reaching behind covers something");
	CHECK(bounds[0] > 0.0f, "a volume beside the camera covers only its side, not the whole window");
	CHECK(bounds[2] >= 1.0f, "a volume reaching behind the camera covers out to the window's edge");
}

int main(void)
{
	check_skinning();
	check_records();
	check_cache();
	check_forget();
	check_cover();
	check_idle();
	check_sweep();
	check_update_budget();
	if (failures)
		fprintf(stderr, "traced shadows: %d failures\n", failures);
	return failures != 0;
}
