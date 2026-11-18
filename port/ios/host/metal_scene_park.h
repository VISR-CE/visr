#ifndef METAL_SCENE_PARK_H
#define METAL_SCENE_PARK_H

/* Whether the scene's pass is big enough to be worth setting aside
(gpu_metal.m's native_park) when the game leaves it for another target,
instead of ending it there and loading it again afterward. The size that
counts is the part the pass draws: dynamic resolution's pooled target is
allocated at the ceiling and drawn at the top left (content_width x
content_height, 0 when the pass draws all of it), and a pass that draws
1280x720 of a 1920x1080 target loads and stores what a 1280x720 target does. */
#define METAL_SCENE_PARK_PIXELS 1000000ul

static inline int metal_scene_park_size(unsigned long width, unsigned long height, unsigned long content_width,
	unsigned long content_height)
{
	if (content_width && content_height)
	{
		width = content_width;
		height = content_height;
	}
	return width * height >= METAL_SCENE_PARK_PIXELS;
}

#endif
