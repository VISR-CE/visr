/*
SHADOW_PROPS.H

port: which objects the game never gave a shadow may cast one under display.shadow_props and
debug.shadow_scenery (render_objects.c). Plain C on plain numbers so a test can build it alone
(tools/test_shadow_props.py).
*/

#ifndef __SHADOW_PROPS_H
#define __SHADOW_PROPS_H

#include <string.h>

#define SHADOW_PROP_MAXIMUM_RADIUS 5.f

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

/* an object the game marked shadowless: whether it casts anyway. is_scenery, is_device (machine, control, light
fixture) and is_item (equipment, garbage) say what it is; whitelisted is the name test above for a scenery object */
static int shadow_prop_allowed(
	int is_scenery,
	int is_device,
	int is_item,
	float bounding_radius,
	int props_on,
	long debug_mask,
	int whitelisted)
{
	if ((debug_mask & SHADOW_PROP_DEBUG_SCENERY) && is_scenery)
		return 1;
	if ((debug_mask & SHADOW_PROP_DEBUG_DEVICES) && is_device)
		return 1;
	if ((debug_mask & SHADOW_PROP_DEBUG_ITEMS) && is_item)
		return 1;
	if (!props_on || bounding_radius > SHADOW_PROP_MAXIMUM_RADIUS)
		return 0;

	return is_device || (is_scenery && whitelisted);
}

#endif
