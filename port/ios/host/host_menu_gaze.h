/* Look and pinch on the Vision Pro's menus, as plain C so a probe builds it alone
   (port/ios/tests/menu_gaze_probe.c). In the immersive space the menus are not in a view: they are drawn on a
   surface in the room, the theater's screen (host_theater.m) or the UI's quad in head-tracked stereo
   (host_stereo.m), and the presenter notes where it drew that surface each frame. A pinch arrives as a spatial
   event (Theater.swift) with the ray the eyes selected along and the pinching hand's position. Where that ray
   meets the surface is the finger's position on it; the hand's travel while pinched is the finger's travel. Both
   feed the menus' pointer (host_menu_pointer.h), so the rules are the touch screen's: the pinch focuses what it
   looks at, a release without travel activates it, a drag past the slop steps the list.

   Positions and sizes on the surface are in visionOS points, about a millimeter at a meter (a milliradian), so
   the reach and the slop mean the same angle wherever the surface stands. The point crosses to the guest as a
   fraction of the picture (host_ui_pointer.picture): the surface shows the game's picture whole, stretched to
   the surface's shape, so no letterbox is undone. */
#ifndef HALO_HOST_MENU_GAZE_H
#define HALO_HOST_MENU_GAZE_H
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "host_menu_pointer.h"

/* points along the surface for a meter of the hand's travel while pinched: 2 cm passes the slop, a list steps
   for every 2.8 cm (starting values, for the headset to confirm) */
#define HOST_MENU_GAZE_HAND_POINTS_PER_METER 1000.0f
/* how far past the surface's edge a pinch still counts, as a share of its half size (the reach finds an item
   at the edge); beyond it the pinch is not the menu's */
#define HOST_MENU_GAZE_EDGE_MARGIN 0.1f
/* a surface noted longer ago than this is no longer drawn (the space closed, the menus went away) */
#define HOST_MENU_GAZE_SURFACE_SECONDS 0.5

/* where the menus' surface was last drawn, in the space's frame (ARKit's origin: meters, x right, y up, z back):
   its center and its half extents along the picture's width (left to right) and height (bottom to top), and
   where the eyes were */
struct host_menu_gaze_surface {
    int valid;
    double drawn_at;
    float center[3], x_axis[3], y_axis[3];
    float eye[3];
};

/* phases of a spatial event (SwiftUI's SpatialEventCollection.Event.Phase) */
enum { HOST_MENU_GAZE_ACTIVE, HOST_MENU_GAZE_ENDED, HOST_MENU_GAZE_CANCELLED };

/* pinches that began off the menus, remembered until they end: two hands and a pointer, with room to spare */
#define HOST_MENU_GAZE_IGNORED 8

/* the pinch being followed */
struct host_menu_gaze {
    int tracking;
    intptr_t id;
    float down_x, down_y;
    int has_hand;
    float hand[3];
    /* each pinch that began off the menus (in play, looking away) stays off them until it ends; the oldest is
       forgotten first if more are held than there is room for */
    int ignored_count;
    intptr_t ignored[HOST_MENU_GAZE_IGNORED];
};

static inline int host_menu_gaze_ignored(const struct host_menu_gaze *g, intptr_t id) {
    for (int i = 0; i < g->ignored_count; i++)
        if (g->ignored[i] == id) return 1;
    return 0;
}
static inline void host_menu_gaze_ignore(struct host_menu_gaze *g, intptr_t id) {
    if (host_menu_gaze_ignored(g, id)) return;
    if (g->ignored_count == HOST_MENU_GAZE_IGNORED) {
        memmove(g->ignored, g->ignored + 1, sizeof(g->ignored[0]) * (HOST_MENU_GAZE_IGNORED - 1));
        g->ignored_count--;
    }
    g->ignored[g->ignored_count++] = id;
}
static inline void host_menu_gaze_forget(struct host_menu_gaze *g, intptr_t id) {
    for (int i = 0; i < g->ignored_count; i++)
        if (g->ignored[i] == id) {
            memmove(g->ignored + i, g->ignored + i + 1, sizeof(g->ignored[0]) * (size_t)(g->ignored_count - i - 1));
            g->ignored_count--;
            return;
        }
}

static inline float host_menu_gaze_dot(const float a[3], const float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline void host_menu_gaze_transform(const float m[16], const float p[3], float w, float out[3]) {
    for (int i = 0; i < 3; i++) out[i] = m[i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i] * w;
}

/* a surface given in a frame (frame_to_origin: a column-major 4x4, simd's layout) and the eye's position in the
   space's frame */
static inline void host_menu_gaze_surface_set(struct host_menu_gaze_surface *s, const float frame_to_origin[16],
    const float center[3], const float x_axis[3], const float y_axis[3], const float eye[3], double now) {
    host_menu_gaze_transform(frame_to_origin, center, 1.0f, s->center);
    host_menu_gaze_transform(frame_to_origin, x_axis, 0.0f, s->x_axis);
    host_menu_gaze_transform(frame_to_origin, y_axis, 0.0f, s->y_axis);
    memcpy(s->eye, eye, sizeof(s->eye));
    s->drawn_at = now;
    s->valid = host_menu_gaze_dot(s->x_axis, s->x_axis) > 0.0f && host_menu_gaze_dot(s->y_axis, s->y_axis) > 0.0f;
}

static inline int host_menu_gaze_surface_fresh(const struct host_menu_gaze_surface *s, double now) {
    return s && s->valid && now - s->drawn_at >= 0.0 && now - s->drawn_at <= HOST_MENU_GAZE_SURFACE_SECONDS;
}

/* Where a ray (origin, direction: any length) meets the surface, as fractions of the picture: u from its left,
   v from its top. 1 when it meets the surface's plane ahead of the origin within margin (a share of the half
   size) past its edges, the fractions then outside 0 to 1 by up to margin / 2; 0 otherwise (behind, alongside,
   or off it). */
static inline int host_menu_gaze_hit(const struct host_menu_gaze_surface *s, const float origin[3],
    const float direction[3], float margin, float *u, float *v) {
    float normal[3] = {
        s->x_axis[1] * s->y_axis[2] - s->x_axis[2] * s->y_axis[1],
        s->x_axis[2] * s->y_axis[0] - s->x_axis[0] * s->y_axis[2],
        s->x_axis[0] * s->y_axis[1] - s->x_axis[1] * s->y_axis[0],
    };
    float to_center[3] = { s->center[0] - origin[0], s->center[1] - origin[1], s->center[2] - origin[2] };
    float denominator = host_menu_gaze_dot(direction, normal), t, d[3], across, up;

    if (!s->valid || !(fabsf(denominator) > 1e-9f * sqrtf(host_menu_gaze_dot(normal, normal))))
        return 0;
    t = host_menu_gaze_dot(to_center, normal) / denominator;
    if (!(t > 0.0f))
        return 0;
    for (int i = 0; i < 3; i++) d[i] = origin[i] + t * direction[i] - s->center[i];
    across = host_menu_gaze_dot(d, s->x_axis) / host_menu_gaze_dot(s->x_axis, s->x_axis);
    up = host_menu_gaze_dot(d, s->y_axis) / host_menu_gaze_dot(s->y_axis, s->y_axis);
    if (!(fabsf(across) <= 1.0f + margin && fabsf(up) <= 1.0f + margin))
        return 0;
    *u = (across + 1.0f) * 0.5f;
    *v = (1.0f - up) * 0.5f;
    return 1;
}

/* the surface's width and height in points as seen from a point (its angle across and up, in milliradians) */
static inline void host_menu_gaze_points(const struct host_menu_gaze_surface *s, const float from[3], float *width,
    float *height) {
    float d[3] = { s->center[0] - from[0], s->center[1] - from[1], s->center[2] - from[2] };
    float distance = sqrtf(host_menu_gaze_dot(d, d));
    float half_width = sqrtf(host_menu_gaze_dot(s->x_axis, s->x_axis));
    float half_height = sqrtf(host_menu_gaze_dot(s->y_axis, s->y_axis));

    if (!(distance > 0.01f))
        distance = 0.01f;
    *width = 2000.0f * atanf(half_width / distance);
    *height = 2000.0f * atanf(half_height / distance);
}

/* the hand's travel (meters, the space's frame) as points along the surface: right and down */
static inline void host_menu_gaze_travel(const struct host_menu_gaze_surface *s, const float travel[3], float *dx,
    float *dy) {
    float width = sqrtf(host_menu_gaze_dot(s->x_axis, s->x_axis));
    float height = sqrtf(host_menu_gaze_dot(s->y_axis, s->y_axis));

    *dx = *dy = 0.0f;
    if (width > 0.0f) *dx = host_menu_gaze_dot(travel, s->x_axis) / width * HOST_MENU_GAZE_HAND_POINTS_PER_METER;
    if (height > 0.0f) *dy = -host_menu_gaze_dot(travel, s->y_axis) / height * HOST_MENU_GAZE_HAND_POINTS_PER_METER;
}

/* A spatial event: id names the pinch for its life, phase is HOST_MENU_GAZE_*; ray (origin then direction, the
   space's frame) is the selection ray, NULL without one; hand the pinch's position, NULL without one. surface
   is where the menus were last drawn, NULL when they are not drawn in the space (then no pinch begins). The
   first event of a pinch begins it where the ray meets the surface; later ones move it by the hand's travel; the
   last ends or cancels it. A second pinch while one is held is ignored, as a second finger is. Nothing is taken
   while the menus are not up (host_menu_pointer_listening): a pinch in play stays the game's. Returns 1 when the
   event was the menus'. */
static inline int host_menu_gaze_event(struct host_menu_gaze *g, struct host_menu_pointer *m,
    const struct host_menu_gaze_surface *s, intptr_t id, int phase, const float *ray, const float *hand) {
    if (g->tracking && !(m->tracking && m->finger == g->id)) {
        /* the menus dropped the pinch being followed (they went away, a replay took the controls): the rest of it
           is not theirs, and if its end never comes (the space closed mid-pinch), the next pinch is still free */
        g->tracking = 0;
        host_menu_gaze_ignore(g, g->id);
    }
    if (host_menu_gaze_ignored(g, id)) {
        if (phase != HOST_MENU_GAZE_ACTIVE) host_menu_gaze_forget(g, id);
        return 0;
    }
    if (g->tracking && g->id == id) {
        if (phase == HOST_MENU_GAZE_ENDED) {
            host_menu_pointer_touch_end(m, id);
            g->tracking = 0;
        } else if (phase == HOST_MENU_GAZE_CANCELLED) {
            host_menu_pointer_touch_cancel(m, id);
            g->tracking = 0;
        } else if (hand && g->has_hand && s && s->valid) {
            float travel[3] = { hand[0] - g->hand[0], hand[1] - g->hand[1], hand[2] - g->hand[2] }, dx, dy;

            host_menu_gaze_travel(s, travel, &dx, &dy);
            host_menu_pointer_touch_move(m, id, g->down_x + dx, g->down_y + dy);
        }
        return 1;
    }
    if (phase != HOST_MENU_GAZE_ACTIVE)
        return 0;
    {
        float u, v, width, height;

        if (g->tracking || m->tracking || !host_menu_pointer_listening(m) || !s || !ray ||
            !host_menu_gaze_hit(s, ray, ray + 3, HOST_MENU_GAZE_EDGE_MARGIN, &u, &v)) {
            host_menu_gaze_ignore(g, id);
            return 0;
        }
        host_menu_gaze_points(s, ray, &width, &height);
        host_menu_pointer_set_surface(m, width, height);
        g->down_x = u * width;
        g->down_y = v * height;
        g->has_hand = hand != NULL;
        if (hand) memcpy(g->hand, hand, sizeof(g->hand));
        host_menu_pointer_gaze_begin(m, id, g->down_x, g->down_y);
        g->tracking = m->tracking && m->finger == id;
        g->id = id;
        return g->tracking;
    }
}
#endif
