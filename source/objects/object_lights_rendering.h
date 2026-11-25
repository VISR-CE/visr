/*
OBJECT_LIGHTS_RENDERING.H

Narrow cross-translation-unit rendering interface owned by OBJECT_LIGHTS.C.
*/

#ifndef __OBJECT_LIGHTS_RENDERING_H
#define __OBJECT_LIGHTS_RENDERING_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"

/* ---------- structures */

union real_point3d;
struct render_lighting;

/* ---------- prototypes/OBJECT_LIGHTS.C */

void lights_preprocess_scene(
	void);
void lights_render_diffuse(
	void);
/* port: what blocks each of the window's dynamic lights (light_visibility.h), once a window after
lights_preprocess_scene and before its scene pass */
void lights_choose_visibility(
	void);
/* port: the next draws are a dynamic light's over the level, blocked by its source (light_visibility_next); FALSE
when they are drawn as the game draws them. light_visibility_end after them. The pass's distance attenuation
coordinate is texture stage attenuation_stage, made as 0.5 - (light - point) / (2 attenuation_radius); a stage
of NONE is a pass without one (the specular spot light's), which is drawn as the game draws it for now */
boolean light_visibility_begin(
	union real_point3d const *position,
	real radius,
	short attenuation_stage,
	real attenuation_radius);
void light_visibility_end(
	void);
/* port: whether an object stands in reach of one of the window's traced lights and is of a kind that blocks them
(light_visibility.h's caster rule), so the caster walker names its parts (render_objects.c) */
boolean lights_object_blocks_light(
	long object_index);
void lights_render_specular(
	void);
boolean lights_distant_lighting_at_point(
	long flags,
	union real_point3d const *position,
	struct render_lighting *lighting);
void lights_prepare_for_object_static(
	long object_index,
	struct render_lighting *lighting);

/* ---------- globals */

extern boolean object_light_interpolate;
extern real object_light_ambient_base;
extern real object_light_ambient_scale;
extern real object_light_secondary_scale;

#endif /* __OBJECT_LIGHTS_RENDERING_H */
