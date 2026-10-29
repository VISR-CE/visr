/* What a finger or a pointer device does to the menus, as plain C so a probe
   builds it alone (host_menu_pointer.m feeds it from UIKit; the guest's
   halo_ui_pointer_update reads it through host_ui_pointer_read, d3d8_device.c).

   A touch: down moves the pointer there (the menus focus what is under it),
   a lift without travel is a click where it went down, travel past the slop
   cancels the click and turns the drag into list steps, one for each notch of
   travel. Only the first finger counts. A pointer device moves, clicks and
   wheels as a mouse does. Nothing is tracked while no menu is up or while a
   replay holds the controls. Events accumulate until a read, which clears
   them (platform_ui_pointer_read does the same for the desktop's mouse). */
#ifndef HALO_HOST_MENU_POINTER_H
#define HALO_HOST_MENU_POINTER_H
#include <math.h>
#include <string.h>
#include "halo_host_pointer.h"

/* in points */
#define HOST_MENU_POINTER_SLOP 10.0f
#define HOST_MENU_POINTER_NOTCH 28.0f
/* half of Apple's 44 pt minimum target */
#define HOST_MENU_POINTER_RADIUS 22.0f
#define HOST_MENU_POINTER_MAXIMUM_STEPS 8
/* a look and pinch (host_menu_gaze.h): half of visionOS's 60 pt minimum target, and a slop that a pinching
   hand's own wobble stays inside */
#define HOST_MENU_POINTER_GAZE_RADIUS 30.0f
#define HOST_MENU_POINTER_GAZE_SLOP 20.0f

struct host_menu_pointer {
    int menus_active, blocked;
    float surface_width, surface_height;
    /* the finger being followed, if any */
    int tracking;
    intptr_t finger;
    float down_x, down_y, anchor_y;
    int scrolling;
    /* the latest position, and what has happened since the last read */
    float x, y, click_x, click_y;
    int moved, left_clicks, right_clicks, wheel_steps;
    /* the latest input was a finger (1) or a look and pinch (2; it needs the reach), or a pointer device (0) */
    int touch;
};

static inline void host_menu_pointer_clear(struct host_menu_pointer *m) {
    m->tracking = 0;
    m->scrolling = 0;
    m->moved = m->left_clicks = m->right_clicks = m->wheel_steps = 0;
}
static inline void host_menu_pointer_set_surface(struct host_menu_pointer *m, float width, float height) {
    m->surface_width = width;
    m->surface_height = height;
}
/* a menu coming up or going away drops whatever was in flight */
static inline void host_menu_pointer_set_menus(struct host_menu_pointer *m, int active) {
    if ((active != 0) != (m->menus_active != 0)) host_menu_pointer_clear(m);
    m->menus_active = active != 0;
}
static inline void host_menu_pointer_set_blocked(struct host_menu_pointer *m, int blocked) {
    if (blocked) host_menu_pointer_clear(m);
    m->blocked = blocked != 0;
}
static inline int host_menu_pointer_listening(const struct host_menu_pointer *m) {
    return m->menus_active && !m->blocked;
}

static inline void host_menu_pointer_touch_begin(struct host_menu_pointer *m, intptr_t finger, float x, float y) {
    if (!host_menu_pointer_listening(m) || m->tracking) return;
    m->tracking = 1;
    m->finger = finger;
    m->scrolling = 0;
    m->down_x = m->x = x;
    m->down_y = m->y = y;
    m->touch = 1;
    m->moved = 1;
}
/* a pinch where the eyes meet the menus' surface (host_menu_gaze.h): a finger with a wider reach and slop, its
   position a fraction of the picture */
static inline void host_menu_pointer_gaze_begin(struct host_menu_pointer *m, intptr_t pinch, float x, float y) {
    host_menu_pointer_touch_begin(m, pinch, x, y);
    if (m->tracking && m->finger == pinch) m->touch = 2;
}
static inline void host_menu_pointer_touch_move(struct host_menu_pointer *m, intptr_t finger, float x, float y) {
    if (!m->tracking || m->finger != finger || !host_menu_pointer_listening(m)) return;
    if (!m->scrolling) {
        if (hypotf(x - m->down_x, y - m->down_y) <=
            (m->touch == 2 ? HOST_MENU_POINTER_GAZE_SLOP : HOST_MENU_POINTER_SLOP)) return;
        m->scrolling = 1;
        m->anchor_y = m->down_y;
    }
    float travel = y - m->anchor_y;
    int steps = (int)(travel / HOST_MENU_POINTER_NOTCH);
    if (steps) {
        /* the list follows the finger as a scrolled page does: a finger moving up brings the later items (the wheel's
           steps are positive away from the user, up the list) */
        m->wheel_steps -= steps;
        m->anchor_y += (float)steps * HOST_MENU_POINTER_NOTCH;
    }
    m->x = x;
    m->y = y;
}
static inline void host_menu_pointer_touch_end(struct host_menu_pointer *m, intptr_t finger) {
    if (!m->tracking || m->finger != finger) return;
    if (!m->scrolling && host_menu_pointer_listening(m)) {
        m->left_clicks++;
        m->click_x = m->down_x;
        m->click_y = m->down_y;
    }
    m->tracking = 0;
    m->scrolling = 0;
}
static inline void host_menu_pointer_touch_cancel(struct host_menu_pointer *m, intptr_t finger) {
    if (!m->tracking || m->finger != finger) return;
    m->tracking = 0;
    m->scrolling = 0;
}

/* a pointer device */
static inline void host_menu_pointer_device_move(struct host_menu_pointer *m, float x, float y) {
    if (!host_menu_pointer_listening(m)) return;
    m->x = x;
    m->y = y;
    m->moved = 1;
    m->touch = 0;
}
static inline void host_menu_pointer_device_click(struct host_menu_pointer *m, int right, float x, float y) {
    if (!host_menu_pointer_listening(m)) return;
    m->touch = 0;
    if (right) {
        m->right_clicks++;
        return;
    }
    m->left_clicks++;
    m->click_x = x;
    m->click_y = y;
}
/* notches of a wheel: positive away from the user, as the desktop's wheel (up the list) */
static inline void host_menu_pointer_device_wheel(struct host_menu_pointer *m, int notches) {
    if (!host_menu_pointer_listening(m)) return;
    m->touch = 0;
    m->wheel_steps -= notches;
}

static inline void host_menu_pointer_read(struct host_menu_pointer *m, struct host_ui_pointer *out) {
    int steps = m->wheel_steps;
    memset(out, 0, sizeof(*out));
    out->active = 1;
    out->x = m->x;
    out->y = m->y;
    out->click_x = m->click_x;
    out->click_y = m->click_y;
    out->surface_width = m->surface_width;
    out->surface_height = m->surface_height;
    out->radius = m->touch == 2 ? HOST_MENU_POINTER_GAZE_RADIUS : m->touch ? HOST_MENU_POINTER_RADIUS : 0.0f;
    out->picture = m->touch == 2;
    if (!host_menu_pointer_listening(m)) {
        host_menu_pointer_clear(m);
        return;
    }
    out->moved = m->moved;
    out->left_clicks = m->left_clicks;
    out->right_clicks = m->right_clicks;
    out->wheel_steps = steps < -HOST_MENU_POINTER_MAXIMUM_STEPS ? -HOST_MENU_POINTER_MAXIMUM_STEPS :
        steps > HOST_MENU_POINTER_MAXIMUM_STEPS ? HOST_MENU_POINTER_MAXIMUM_STEPS : steps;
    m->moved = m->left_clicks = m->right_clicks = m->wheel_steps = 0;
}
#endif
