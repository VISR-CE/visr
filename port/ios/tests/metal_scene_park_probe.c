/* metal_scene_park.h: the scene's pass is set aside by the size it draws,
so a dynamic resolution pass drawn at 720 lines of a 1080-line target is
treated as a 720-line target is. */
#include "metal_scene_park.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	/* whole targets: 1080 lines and up are set aside, 720 lines are not */
	assert(metal_scene_park_size(1920, 1080, 0, 0));
	assert(metal_scene_park_size(3840, 2160, 0, 0));
	assert(!metal_scene_park_size(1280, 720, 0, 0));
	assert(!metal_scene_park_size(960, 540, 0, 0));
	/* dynamic resolution's pooled target follows the part drawn */
	assert(metal_scene_park_size(1920, 1080, 1920, 1080));
	assert(metal_scene_park_size(1920, 1080, 1708, 960));
	assert(!metal_scene_park_size(1920, 1080, 1280, 720));
	assert(!metal_scene_park_size(3840, 2160, 1280, 720));
	assert(metal_scene_park_size(3840, 2160, 2560, 1440));
	/* half a content size is no content size */
	assert(metal_scene_park_size(1920, 1080, 1280, 0));
	assert(metal_scene_park_size(1920, 1080, 0, 720));
	/* the threshold itself */
	assert(metal_scene_park_size(1000, 1000, 0, 0));
	assert(!metal_scene_park_size(1000, 999, 0, 0));
	printf("PASS: Metal scene pass parking follows the size drawn\n");
	return 0;
}
