/*
TRACED_SHADOW_CASTERS.H

display.traced_shadows: what object shadows are traced against (port/linux/game/traced_shadow_casters.c), called from
rasterizer_xbox_shadows.c and render_objects.c.
*/

#ifndef __TRACED_SHADOW_CASTERS_H
#define __TRACED_SHADOW_CASTERS_H
#pragma once

struct triangle_buffer;
struct vertex_buffer;
struct render_skinning;

enum
{
	TRACED_SHADOW_NONE = 0xfffffffful, /* a shadow drawn into its map */
};

/* a map has loaded: the last map's parts are forgotten */
void traced_shadow_casters_new_map(void);
/* a window's shadows begin: whether they are traced (p11: the projection's y scale). When they are, the calls
below follow, and traced_shadow_casters_window_end after the last apply draw */
boolean traced_shadow_casters_window_begin(real p11);
void traced_shadow_casters_window_end(void);
/* a new shadow's casters follow, part by part, until traced_shadow_casters_end; its id this frame, or
TRACED_SHADOW_NONE in a window that does not trace */
unsigned long traced_shadow_casters_begin(long object_index);
void traced_shadow_casters_part(struct triangle_buffer const *triangle_buffer, struct vertex_buffer const *vertex_buffer,
	struct render_skinning const *skinning);
void traced_shadow_casters_end(void);
/* the window's shadows are all generated: their structures are built before the apply draws */
void traced_shadow_casters_commit(void);
/* the next draws apply shadow id, of this matrix and radius, until traced_shadow_apply_end */
void traced_shadow_apply(unsigned long id, real_matrix4x3 const *shadow_matrix, real radius);
void traced_shadow_apply_end(void);
/* display.traced_lights: the traced strength of each of the window's dynamic lights (count of them, by
light_visibility.h's rule): 0 drawn as the game draws it, up to 1 lit only where a ray from the point reaches it.
versus: a player-versus-player game, which never traces (light through a wall is information there). Once a window
a frame: a second call in the same window and frame (a second eye) gets the first's strengths by light */
struct light_visibility_candidate;
void traced_lights_choose(struct light_visibility_candidate const *lights, short count, boolean versus, float *strength);
/* the next draws are a dynamic light's over the level, lit only where a ray from the point reaches the light, its
blocking scaled by strength; family: its owner family's mask bit (light_visibility.h), whose casters the rays pass
through, 0 none; owner_sphere: the owner family's root (center x, y, z, radius) when it has no bit, which no caster
blocks the light within, NULL none. FALSE when they are drawn as the game draws them. traced_light_end after them */
boolean traced_light_begin(real_point3d const *position, real radius, real strength, long family,
	real const *owner_sphere, short attenuation_stage, real attenuation_radius);
void traced_light_end(void);
/* whether objects with no shadow of their own (too faint, or a shadowless kind such as a door) block the traced
lights too, through the caster walker (render_objects.c, object_lights.c's lights_object_blocks_light) */
boolean traced_light_walks_casters(void);

#endif
