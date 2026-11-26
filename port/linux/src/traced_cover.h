/*
TRACED_COVER.H

What a traced shadow's volume covers on screen (display.traced_shadows): the
box's eight corners in view space, clipped by the near plane and projected,
as a rectangle in normalized device coordinates with y down. Plain C with no
game types, so port/linux/game/traced_shadow_casters.c and
port/ios/tests/traced_shadows_probe.c compile the same code.
*/

#ifndef __HALO_TRACED_COVER_H
#define __HALO_TRACED_COVER_H

/* the projection's terms that place a view-space point (x, y, z with z < 0 in front): x' = (p00 x + p20 z) / -z,
y' = -(p11 y + p21 z) / -z */
struct traced_cover_projection
{
	float p00, p20, p11, p21;
	float z_near;
};

static inline void traced_cover_point(const struct traced_cover_projection *projection, const float view[3],
	float bounds[4])
{
	float inverse_depth = -1.0f / view[2];
	float x = (projection->p00 * view[0] + projection->p20 * view[2]) * inverse_depth;
	float y = -(projection->p11 * view[1] + projection->p21 * view[2]) * inverse_depth;

	bounds[0] = x < bounds[0] ? x : bounds[0];
	bounds[1] = y < bounds[1] ? y : bounds[1];
	bounds[2] = x > bounds[2] ? x : bounds[2];
	bounds[3] = y > bounds[3] ? y : bounds[3];
}

/* A box's screen bounds: corner i is the one whose bits 0, 1 and 2 pick the far side of its three axes, so the
twelve edges join corners one bit apart. Corners behind the near plane are replaced by the points where the box's
edges cross it, so a volume the camera stands in covers only what its front part projects to (the whole window was
once the answer for any volume reaching the near plane, and a player's own shadow reaches it most frames). Returns 0
when the box is wholly behind the near plane (bounds untouched), else 1 with bounds = left, top, right, bottom, not
clamped to the window. */
static inline int traced_cover_box(const struct traced_cover_projection *projection, const float views[8][3],
	float bounds[4])
{
	float plane = -projection->z_near;
	int corner, axis, any = 0;

	bounds[0] = bounds[1] = 1e30f;
	bounds[2] = bounds[3] = -1e30f;
	for (corner = 0; corner < 8; corner++)
	{
		if (views[corner][2] <= plane)
		{
			traced_cover_point(projection, views[corner], bounds);
			any = 1;
		}
	}
	for (corner = 0; corner < 8; corner++)
	{
		for (axis = 1; axis < 8; axis <<= 1)
		{
			const float *from = views[corner], *to = views[corner | axis];
			float crossing[3], t;

			if ((corner & axis) || (from[2] <= plane) == (to[2] <= plane))
				continue;
			t = (plane - from[2]) / (to[2] - from[2]);
			crossing[0] = from[0] + (to[0] - from[0]) * t;
			crossing[1] = from[1] + (to[1] - from[1]) * t;
			crossing[2] = plane;
			traced_cover_point(projection, crossing, bounds);
			any = 1;
		}
	}
	return any;
}

#endif
