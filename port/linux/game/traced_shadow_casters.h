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

#endif
