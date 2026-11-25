/*
TRACED_LIGHT_RULE.H

Which of a window's dynamic lights are traced (display.traced_lights), and how strongly: plain C with no game state,
so tools/test_traced_shadows.py compiles and runs it. traced_shadow_casters.c's traced_lights_choose is its one
caller in the game.

- By owner. A light owned by a unit, weapon, item or projectile, or by no object (an explosion, an impact, a
  scripted effect at a point), is traced. One owned by scenery or a device (a light fixture, a machine, a control),
  directly or through an effect made on it, is not: the level's designers placed those against a world where light
  passes through walls, and some sit inside their own fixture. tools/traced_light_stock.tsv lists every dynamic
  light on the stock maps by owner (tools/traced_light_census.py); the test checks this rule against it.
- By cap. Of the traced kind, the window's own flashlight comes first, then the rest by screen contribution
  (light_render_selection.h's score: radius squared over distance squared), up to display.traced_lights.
- In time. A light new this frame is traced at full strength from its first frame, however short-lived: a two-frame
  muzzle flash is blocked in both. Only a light that enters or leaves the traced set because of the cap eases, over
  TRACED_LIGHT_EASE_MILLISECONDS of wall time, so its shadows never switch in one frame because another light went
  out. A light easing out is still traced until its strength reaches zero.
*/

#ifndef __TRACED_LIGHT_RULE_H
#define __TRACED_LIGHT_RULE_H

enum
{
	TRACED_LIGHT_MAXIMUM = 128, /* MAXIMUM_RENDERED_LIGHTS, the game's lights in a view */
	TRACED_LIGHT_EASE_MILLISECONDS = 100,
	/* object_types.h: biped, vehicle, weapon, equipment, garbage, projectile come before scenery */
	TRACED_LIGHT_LAST_TRACED_OWNER_TYPE = 5,
};

struct traced_light_candidate
{
	unsigned long id;   /* the light's datum index: the same light keeps it from frame to frame */
	double score;       /* screen contribution */
	int owner_type;     /* the owner object's type, or -1 for none */
	int own;            /* the window's own flashlight (on its first-person weapon) */
};

struct traced_light_ramp
{
	unsigned long id[TRACED_LIGHT_MAXIMUM];
	float strength[TRACED_LIGHT_MAXIMUM];
	int count;
	unsigned long milliseconds; /* wall time of the last step */
	int valid;
};

/* TRUE for an owner whose lights may be traced (-1: the light has no owner object) */
static inline int traced_light_owner_traced(
	int owner_type)
{
	return owner_type < 0 || owner_type <= TRACED_LIGHT_LAST_TRACED_OWNER_TYPE;
}

/* chosen[i] 1 for the candidates traced this frame: those whose owner allows it (every one when fixtures is set),
own flashlight first, then by score, then by id, up to cap */
static inline void traced_light_rank(
	const struct traced_light_candidate *lights,
	int count,
	int cap,
	int fixtures,
	unsigned char *chosen)
{
	int order[TRACED_LIGHT_MAXIMUM], ordered = 0, i, j;

	if (count > TRACED_LIGHT_MAXIMUM)
		count = TRACED_LIGHT_MAXIMUM;
	for (i = 0; i < count; i++)
	{
		chosen[i] = 0;
		if (!fixtures && !traced_light_owner_traced(lights[i].owner_type))
			continue;
		for (j = ordered; j > 0; j--)
		{
			const struct traced_light_candidate *a = &lights[i], *b = &lights[order[j - 1]];

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
static inline void traced_light_ramp_step(
	struct traced_light_ramp *ramp,
	const struct traced_light_candidate *lights,
	const unsigned char *chosen,
	int count,
	unsigned long milliseconds,
	float *strength)
{
	float step = 1.f;
	int i, j;

	if (count > TRACED_LIGHT_MAXIMUM)
		count = TRACED_LIGHT_MAXIMUM;
	if (ramp->valid)
	{
		unsigned long elapsed = milliseconds - ramp->milliseconds;

		step = elapsed >= TRACED_LIGHT_EASE_MILLISECONDS ? 1.f : (float)elapsed / (float)TRACED_LIGHT_EASE_MILLISECONDS;
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
