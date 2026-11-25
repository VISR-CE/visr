#ifndef DEPTH_DECODE_H
#define DEPTH_DECODE_H

/* The stored depth's view distance: one decode for every pass that reads a depth target (ambient occlusion, soft
particles, the traced lights' depth point), checked numerically by tools/test_depth_decode.py.

The game's projection is GL-style (normalized depth -p22 + p32 / d: -1 at the near plane), the Xbox's screen
conversion takes that as the 0-1 range without remapping, and Metal stores its reverse. A distance d is therefore
stored as 1 - (-p22 + p32 / d) and comes back as p32 / (p22 + 1 - stored). Reading the reverse as a near-to-far range
of the frustum's near and far (z_near z_far / (z_far - (1 - stored) (z_far - z_near))) halves every distance past a
couple of world units.

DEPTH_VIEW_DISTANCE_MSL is the decode as Metal source text: depth_view_distance(stored, p22, p32), the projection's
depth row in, the linear view distance out. The pixel shader translator emits it ahead of a shader that rebuilds
a point from depth. */

#define DEPTH_VIEW_DISTANCE_MSL \
	"static float depth_view_distance(float stored, float p22, float p32)\n" \
	"{\n" \
	"\treturn p32 / (p22 + 1.0 - stored);\n" \
	"}\n"

/* the depth row (p22, p32) of the projection that made the depth: the projection's own when it has one (its p22 is not
+-1 and its p32 not 0), else the GL-style row of the frustum's near and far */
static inline void depth_row_from_view(float near_plane, float far_plane, float row22, float row32, float *p22,
	float *p32)
{
	if (row22 != 1.0f && row22 != -1.0f && row32 != 0.0f)
	{
		*p22 = row22;
		*p32 = row32;
		return;
	}
	*p22 = -(far_plane + near_plane) / (far_plane - near_plane);
	*p32 = -2.0f * near_plane * far_plane / (far_plane - near_plane);
}

#endif
