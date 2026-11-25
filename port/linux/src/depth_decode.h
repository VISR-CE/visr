#ifndef DEPTH_DECODE_H
#define DEPTH_DECODE_H

/* The stored depth's view distance: one decode for every pass that reads a depth target (ambient occlusion in
gpu_metal.m, the soft particle distances in gpu_metal.m and the particle's own distance in nv2a_psh.c, the stereo HUD
depth in host_stereo.m), checked numerically by tools/test_depth_decode.py.

The game's projection row is 0-to-1, near to far (render_cameras.c's frustum projection matrix): p22 = -f / (f - n)
and p32 = -f n / (f - n), so a view distance d has clip z / w = f / (f - n) - f n / ((f - n) d), 0 at the near plane
and 1 at the far one. The vertex translator (nv2a_vsh.c) stores (w - z) / w, the reverse: 1 at the near plane. The
distance therefore comes back as p32 / (p22 + 1 - stored), exact for any row of this shape (the weapon's own frustum
row too). Reading the stored value as a near-to-far range of the frustum's near and far, or deriving the near and far
from the row with GL formulas (which give n / 2), halves every distance from the near plane out.

DEPTH_VIEW_DISTANCE_MSL is the decode as Metal source text, depth_view_distance(stored, p22, p32): the projection's
depth row in, the linear view distance out. Every program that rebuilds a distance from depth emits it ahead of its
code. depth_view_distance_c is the same decode for C. */

#define DEPTH_VIEW_DISTANCE_MSL \
	"static float depth_view_distance(float stored, float p22, float p32)\n" \
	"{\n" \
	"\treturn p32 / (p22 + 1.0 - stored);\n" \
	"}\n"

static inline float depth_view_distance_c(float stored, float p22, float p32)
{
	return p32 / (p22 + 1.0f - stored);
}

/* the depth row (p22, p32) of the projection that made the depth: the projection's own when it has one (its p22 is not
+-1 and its p32 not 0), else the game's 0-to-1 row of the frustum's near and far */
static inline void depth_row_from_view(float near_plane, float far_plane, float row22, float row32, float *p22,
	float *p32)
{
	if (row22 != 1.0f && row22 != -1.0f && row32 != 0.0f)
	{
		*p22 = row22;
		*p32 = row32;
		return;
	}
	*p22 = -far_plane / (far_plane - near_plane);
	*p32 = -far_plane * near_plane / (far_plane - near_plane);
}

#endif
