/*
SHADOW_PROPS.H

port: which objects the game never gave a shadow may cast one under display.shadow_props and
debug.shadow_scenery (render_objects.c). Plain C on plain numbers so a test can build it alone
(tools/test_shadow_props.py).
*/

#ifndef __SHADOW_PROPS_H
#define __SHADOW_PROPS_H

#include <string.h>

/* the largest prop that casts, and the most that cast in one view of a frame, the nearest to the camera first: each
shadow draws every level surface under it, so a large prop in a corridor costs far more than its map does */
#define SHADOW_PROP_MAXIMUM_RADIUS 3.f
#define SHADOW_PROP_MAXIMUM_PER_VIEW 4

/* debug.shadow_scenery's bits */
#define SHADOW_PROP_DEBUG_SCENERY 1
#define SHADOW_PROP_DEBUG_DEVICES 2
#define SHADOW_PROP_DEBUG_ITEMS 4

/* a scenery tag's path is whitelisted when it contains one of these */
static char const *const shadow_prop_scenery_names[] =
{
	"scenery\\c_", /* c_storage, c_storage_large, c_field_generator, c_uplink */
	"b40_metal",
	"crate"
};
#define SHADOW_PROP_SCENERY_NAME_COUNT ((int)(sizeof(shadow_prop_scenery_names) / sizeof(shadow_prop_scenery_names[0])))

static int shadow_prop_scenery_name_matches(
	char const *name)
{
	int index;

	for (index = 0; name && index < SHADOW_PROP_SCENERY_NAME_COUNT; index++)
	{
		if (strstr(name, shadow_prop_scenery_names[index]))
			return 1;
	}

	return 0;
}

/* whether a prop could cast at all: a device, or scenery whose name is whitelisted, small enough */
static int shadow_prop_eligible(
	int is_scenery,
	int is_device,
	float bounding_radius,
	int whitelisted)
{
	return bounding_radius <= SHADOW_PROP_MAXIMUM_RADIUS && (is_device || (is_scenery && whitelisted));
}

/* an object the game marked shadowless: whether it casts anyway. is_scenery, is_device (machine, control, light
fixture) and is_item (equipment, garbage) say what it is; eligible is the test above; chosen says it is among the
view's nearest (shadow_prop_select_nearest). The debug bits ignore the size, the names and the limit */
static int shadow_prop_allowed(
	int is_scenery,
	int is_device,
	int is_item,
	long debug_mask,
	int props_on,
	int eligible,
	int chosen)
{
	if ((debug_mask & SHADOW_PROP_DEBUG_SCENERY) && is_scenery)
		return 1;
	if ((debug_mask & SHADOW_PROP_DEBUG_DEVICES) && is_device)
		return 1;
	if ((debug_mask & SHADOW_PROP_DEBUG_ITEMS) && is_item)
		return 1;

	return props_on && eligible && chosen;
}

/* of count candidates at the given squared distances, the (at most) limit nearest: keep[i] is set for each, clear for the
rest. A tie keeps the earlier candidate. Returns how many are kept */
static int shadow_prop_select_nearest(
	float const *distances_squared,
	int count,
	int limit,
	unsigned char *keep)
{
	int kept = 0;
	int index;

	for (index = 0; index < count; index++)
		keep[index] = 0;
	while (kept < limit && kept < count)
	{
		int best = -1;

		for (index = 0; index < count; index++)
		{
			if (!keep[index] && (best < 0 || distances_squared[index] < distances_squared[best]))
				best = index;
		}
		keep[best] = 1;
		kept++;
	}

	return kept;
}

#endif
