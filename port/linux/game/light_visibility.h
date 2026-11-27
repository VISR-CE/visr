/*
LIGHT_VISIBILITY.H

How much of each dynamic light reaches a lit pixel: one visibility factor per lit pixel per light, multiplied into the
light's contribution, from one of two sources chosen per light once a window, before its scene pass
(object_lights.c's lights_choose_visibility):

- LIGHT_VISIBILITY_TRACED: a ray from the pixel to the light, against the level and the casters
  (display.traced_lights; traced_shadow_casters.c, nv2a_psh.c's traced light block);
- LIGHT_VISIBILITY_MAP: the flashlight's shadow map (display.flashlight_shadows);
- LIGHT_VISIBILITY_NONE: the light as the game draws it.

A light has one source, and the pixel shader key holds it in one field (nv2a_pixel_shader_key light_visibility), so
no light is darkened by both. The diffuse draws learn their light's source through light_visibility_next.

The rest is plain C with no game state, so tools/test_traced_shadows.py compiles and runs it:

- By owner. A light owned by a unit, weapon, item or projectile, or by no object (an explosion, an impact, a
  scripted effect at a point), may be blocked. One owned by scenery or a device (a light fixture, a machine, a
  control), directly or through an effect made on it, is not: the level's designers placed those against a world
  where light passes through walls, and some sit inside their own fixture. tools/traced_light_stock.tsv lists every
  dynamic light on the stock maps by owner (tools/traced_light_census.py); the test checks this rule against it.
- By cap. Of those, the window's own flashlight comes first, then the rest by screen contribution
  (light_render_selection.h's score: radius squared over distance squared), up to the source's cap.
- In time. A light new this frame is blocked at full strength from its first frame, however short-lived: a two-frame
  muzzle flash is blocked in both. Only a light that enters or leaves the set because of the cap eases, over
  LIGHT_VISIBILITY_EASE_MILLISECONDS of wall time, so its shadows never switch in one frame because another light
  went out. A light easing out keeps its source until its strength reaches zero.
*/

#ifndef __LIGHT_VISIBILITY_H
#define __LIGHT_VISIBILITY_H

#include "../src/traced_shadows_format.h" /* the instance mask's family bits */

enum
{
	LIGHT_VISIBILITY_NONE = 0,
	LIGHT_VISIBILITY_TRACED = 1,
	LIGHT_VISIBILITY_MAP = 2,
};

/* the diffuse draws of the next light take this source and strength (0 none, up to 1 fully blocked where the source
says the light does not reach); object_lights.c */
void light_visibility_next(int path, float strength);
/* An owner family is an object, its parents and its children (a unit, the weapon it holds, the vehicle it rides and
that vehicle's other riders): its casters never block its own lights (a partner's flashlight is inside his body).
Each frame the families of the most important lights take one bit each of the traced lights' instance mask, from
LIGHT_VISIBILITY_FAMILY_FIRST (light_visibility_families_offer: the players' own lights first, then by each light's best
render score from any local player's camera). A family past the last bit has no bit, and its lights are kept clear by
its root's bounding sphere instead: no caster blocks them inside it (traced_light_begin's owner_sphere), so a shooter
never blocks his own muzzle flash however many families a fight has.
The family bit of the object's family, 0 none (object_lights.c) */
int light_visibility_family(long object_index);

enum
{
	LIGHT_VISIBILITY_MAXIMUM = 128, /* MAXIMUM_RENDERED_LIGHTS, the game's lights in a view */
	/* light_visibility_begin's attenuation stage: the texture stage holding the pass's distance attenuation
	coordinate (the specular point pass's xT1, the diffuse pass's xT2), or none (the specular spot pass), whose lit
	point is rebuilt from depth */
	LIGHT_VISIBILITY_STAGE_NONE = 0,
	LIGHT_VISIBILITY_STAGE_SPECULAR = 1,
	LIGHT_VISIBILITY_STAGE_DIFFUSE = 2,
	LIGHT_VISIBILITY_EASE_MILLISECONDS = 100,
	/* the families' bits of the traced lights' instance mask (traced_shadows_format.h) */
	LIGHT_VISIBILITY_FAMILY_FIRST = TRACED_SHADOWS_FAMILY_FIRST,
	LIGHT_VISIBILITY_FAMILY_LAST = TRACED_SHADOWS_FAMILY_LAST,
	/* object_types.h: biped, vehicle, weapon, equipment, garbage, projectile come before scenery */
	LIGHT_VISIBILITY_LAST_BLOCKED_OWNER_TYPE = 5,
};

struct light_visibility_candidate
{
	unsigned long id;   /* the light's datum index: the same light keeps it from frame to frame */
	double score;       /* screen contribution */
	int owner_type;     /* the owner object's type, or -1 for none */
	int own;            /* the window's own flashlight (on its first-person weapon) */
};

struct light_visibility_ramp
{
	unsigned long id[LIGHT_VISIBILITY_MAXIMUM];
	float strength[LIGHT_VISIBILITY_MAXIMUM];
	unsigned char chosen[LIGHT_VISIBILITY_MAXIMUM]; /* the step's choice by light: who holds a slot */
	int count;
	unsigned long milliseconds; /* wall time of the last step */
	int valid;
};

/* TRUE for an owner whose lights may be blocked (-1: the light has no owner object) */
static inline int light_visibility_owner_blocks(
	int owner_type)
{
	return owner_type < 0 || owner_type <= LIGHT_VISIBILITY_LAST_BLOCKED_OWNER_TYPE;
}

/* TRUE for an object type that blocks lights when it has a collision model: characters, vehicles, scenery and devices
(a closed door stops plasma light). Not items (the first-person weapon among them) or projectiles */
static inline int light_visibility_caster_type(
	int object_type)
{
	return object_type == 0 || object_type == 1 || (object_type >= 6 && object_type <= 9);
}

/* the family bit for a family root in the frame's table (roots[] of count, light_visibility_families_offer), 0 when it
has none. root -1 (NONE) is no family; any other value is a handle, and the guest's 32-bit handles with the salt's
high bit set are negative */
static inline int light_visibility_family_bit(
	long const *roots,
	int count,
	long root)
{
	int i;

	if (root == -1)
		return 0;
	for (i = 0; i < count; i++)
		if (roots[i] == root)
			return LIGHT_VISIBILITY_FAMILY_FIRST + i;
	return 0;
}

/* TRUE when family a (importance, root) goes ahead of family b: higher importance first, then the lower root */
static inline int light_visibility_family_ahead(
	double a_importance,
	long a_root,
	double b_importance,
	long b_root)
{
	return a_importance > b_importance || (a_importance == b_importance && a_root < b_root);
}

/* the frame's family table, made before anything asks for a bit (a caster record can be sent before its owner's light
is chosen: split screen reuses window 0's records in window 1), by offering every owned light's root with that
light's importance, in any order and with no limit on how many. The table keeps the bits most important roots
(roots[] and importance[] of count, most important first, ties to the lower root), each at its best light's
importance, and root i takes bit LIGHT_VISIBILITY_FAMILY_FIRST + i; the rest have none. (A root dropped for a better
one never comes back with less than it had: its best so far was already below the table's least.) root -1 (NONE)
takes nothing. A record lives one frame on the host (gpu_metal.m clears its casters each frame), so a bit may move
between frames */
static inline void light_visibility_families_offer(
	long *roots,
	double *importance,
	int *count,
	int bits,
	long root,
	double value)
{
	int i;

	if (root == -1 || bits <= 0)
		return;
	for (i = 0; i < *count && roots[i] != root; i++)
		;
	if (i < *count)
	{
		if (value <= importance[i])
			return;
	}
	else if (*count < bits)
		i = (*count)++;
	else if (light_visibility_family_ahead(value, root, importance[*count - 1], roots[*count - 1]))
		i = *count - 1;
	else
		return;
	/* (moved up to its place) */
	for (; i > 0 && light_visibility_family_ahead(value, root, importance[i - 1], roots[i - 1]); i--)
	{
		roots[i] = roots[i - 1];
		importance[i] = importance[i - 1];
	}
	roots[i] = root;
	importance[i] = value;
}

/* TRUE when two spheres (center, radius) overlap */
static inline int light_visibility_spheres_touch(
	float ax, float ay, float az, float ar,
	float bx, float by, float bz, float br)
{
	float dx = ax - bx, dy = ay - by, dz = az - bz, r = ar + br;

	return r > 0.f && dx * dx + dy * dy + dz * dz < r * r;
}

/* held[i] 1 for the candidates the ramp's last step chose (none when it has no step) */
static inline void light_visibility_held(
	const struct light_visibility_ramp *ramp,
	const struct light_visibility_candidate *lights,
	int count,
	unsigned char *held)
{
	int i, j;

	if (count > LIGHT_VISIBILITY_MAXIMUM)
		count = LIGHT_VISIBILITY_MAXIMUM;
	for (i = 0; i < count; i++)
	{
		held[i] = 0;
		for (j = 0; ramp->valid && j < ramp->count; j++)
			if (ramp->id[j] == lights[i].id)
			{
				held[i] = ramp->chosen[j];
				break;
			}
	}
}

/* chosen[i] 1 for the candidates chosen this frame: those whose owner allows it (every one when fixtures is set),
own flashlight first, then by score, then by id, up to cap. A light that holds a slot (held[i], may be NULL) ranks
with its score raised by the fraction hold, so a challenger takes its slot only by beating it by that margin: two
lights of about the same score don't trade the slot, and its shadow, from frame to frame */
static inline void light_visibility_rank(
	const struct light_visibility_candidate *lights,
	int count,
	int cap,
	int fixtures,
	const unsigned char *held,
	double hold,
	unsigned char *chosen)
{
	int order[LIGHT_VISIBILITY_MAXIMUM], ordered = 0, i, j;
	double rank[LIGHT_VISIBILITY_MAXIMUM];

	if (count > LIGHT_VISIBILITY_MAXIMUM)
		count = LIGHT_VISIBILITY_MAXIMUM;
	for (i = 0; i < count; i++)
	{
		chosen[i] = 0;
		rank[i] = lights[i].score * (held && held[i] && hold > 0.0 ? 1.0 + hold : 1.0);
		if (!fixtures && !light_visibility_owner_blocks(lights[i].owner_type))
			continue;
		for (j = ordered; j > 0; j--)
		{
			const struct light_visibility_candidate *a = &lights[i], *b = &lights[order[j - 1]];
			double ra = rank[i], rb = rank[order[j - 1]];

			if (!(a->own > b->own || (a->own == b->own &&
				(ra > rb || (ra == rb && a->id < b->id)))))
				break;
			order[j] = order[j - 1];
		}
		order[j] = i;
		ordered++;
	}
	for (j = 0; j < ordered && j < cap; j++)
		chosen[order[j]] = 1;
}

/* strength[i] in 0-1 for each candidate from this frame's choice and the last step's strengths, and the ramp
remembers this frame's lights. A light the last step did not have starts where its choice puts it */
static inline void light_visibility_ramp_step(
	struct light_visibility_ramp *ramp,
	const struct light_visibility_candidate *lights,
	const unsigned char *chosen,
	int count,
	unsigned long milliseconds,
	float *strength)
{
	float step = 1.f;
	int i, j;

	if (count > LIGHT_VISIBILITY_MAXIMUM)
		count = LIGHT_VISIBILITY_MAXIMUM;
	if (ramp->valid)
	{
		unsigned long elapsed = milliseconds - ramp->milliseconds;

		step = elapsed >= LIGHT_VISIBILITY_EASE_MILLISECONDS ? 1.f : (float)elapsed / (float)LIGHT_VISIBILITY_EASE_MILLISECONDS;
	}
	for (i = 0; i < count; i++)
	{
		float target = chosen[i] ? 1.f : 0.f;

		strength[i] = target;
		for (j = 0; ramp->valid && j < ramp->count; j++)
		{
			if (ramp->id[j] == lights[i].id)
			{
				float from = ramp->strength[j];

				strength[i] = target > from ? (from + step < target ? from + step : target) :
					(from - step > target ? from - step : target);
				break;
			}
		}
	}
	for (i = 0; i < count; i++)
	{
		ramp->id[i] = lights[i].id;
		ramp->strength[i] = strength[i];
		ramp->chosen[i] = chosen[i];
	}
	ramp->count = count;
	ramp->milliseconds = milliseconds;
	ramp->valid = 1;
}

#endif
