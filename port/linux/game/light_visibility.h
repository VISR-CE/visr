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
Each family with a traced light this frame takes one bit of the traced lights' instance mask, from
LIGHT_VISIBILITY_FAMILY_FIRST; past the last, its lights fall back to the owner distance (debug.traced_light_owner).
The family bit of the object's family this frame, 0 none (object_lights.c) */
int light_visibility_family(long object_index);

enum
{
	LIGHT_VISIBILITY_MAXIMUM = 128, /* MAXIMUM_RENDERED_LIGHTS, the game's lights in a view */
	LIGHT_VISIBILITY_EASE_MILLISECONDS = 100,
	/* the instance mask: bit 0 casters of no family, bit 1 the level, then one bit a family */
	LIGHT_VISIBILITY_FAMILY_FIRST = 2,
	LIGHT_VISIBILITY_FAMILY_LAST = 31,
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

/* the family bit for a family root, assigning the next free bit (roots[] of count, at most the bits there are); 0
when none is left. root -1 (NONE) is no family; any other value is a handle, and the guest's 32-bit handles with the
salt's high bit set are negative */
static inline int light_visibility_family_bit(
	long *roots,
	int *count,
	long root,
	int assign)
{
	int i;

	if (root == -1)
		return 0;
	for (i = 0; i < *count; i++)
		if (roots[i] == root)
			return LIGHT_VISIBILITY_FAMILY_FIRST + i;
	if (!assign || *count > LIGHT_VISIBILITY_FAMILY_LAST - LIGHT_VISIBILITY_FAMILY_FIRST)
		return 0;
	roots[*count] = root;
	return LIGHT_VISIBILITY_FAMILY_FIRST + (*count)++;
}

/* TRUE when two spheres (center, radius) overlap */
static inline int light_visibility_spheres_touch(
	float ax, float ay, float az, float ar,
	float bx, float by, float bz, float br)
{
	float dx = ax - bx, dy = ay - by, dz = az - bz, r = ar + br;

	return r > 0.f && dx * dx + dy * dy + dz * dz < r * r;
}

/* chosen[i] 1 for the candidates chosen this frame: those whose owner allows it (every one when fixtures is set),
own flashlight first, then by score, then by id, up to cap */
static inline void light_visibility_rank(
	const struct light_visibility_candidate *lights,
	int count,
	int cap,
	int fixtures,
	unsigned char *chosen)
{
	int order[LIGHT_VISIBILITY_MAXIMUM], ordered = 0, i, j;

	if (count > LIGHT_VISIBILITY_MAXIMUM)
		count = LIGHT_VISIBILITY_MAXIMUM;
	for (i = 0; i < count; i++)
	{
		chosen[i] = 0;
		if (!fixtures && !light_visibility_owner_blocks(lights[i].owner_type))
			continue;
		for (j = ordered; j > 0; j--)
		{
			const struct light_visibility_candidate *a = &lights[i], *b = &lights[order[j - 1]];

			if (!(a->own > b->own || (a->own == b->own &&
				(a->score > b->score || (a->score == b->score && a->id < b->id)))))
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
	}
	ramp->count = count;
	ramp->milliseconds = milliseconds;
	ramp->valid = 1;
}

#endif
