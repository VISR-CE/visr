/* What a finger or a pointer device does to the menus, as plain C so a probe
   builds it alone (host_menu_pointer.m feeds it from UIKit; the guest's
   halo_ui_pointer_update reads it through host_ui_pointer_read, d3d8_device.c).

   A touch: down moves the pointer there (the menus focus what is under it),
   a lift without travel is a click where it went down, travel past the slop
   cancels the click and turns the drag into list steps, one for each notch of
   travel. Only the first finger counts. A pointer device moves, clicks and
   wheels as a mouse does. Its left button holds the pointer down and posts a click
   on release only when travel stayed within the slop. Nothing is tracked while no menu is up or while a
   replay holds the controls. Events accumulate until a read, which clears
   them (platform_ui_pointer_read does the same for the desktop's mouse). */
#ifndef HALO_HOST_MENU_POINTER_H
#define HALO_HOST_MENU_POINTER_H
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "halo_host_pointer.h"
#include "halo_ui_drag.h"

/* in points */
#define HOST_MENU_POINTER_SLOP 10.0f
/* a finger that has rested this long is holding, not scrolling: its own wobble gets a wider slop so a press-and-hold
   (a context menu opens at 0.5 s) is not cancelled as a scroll */
#define HOST_MENU_POINTER_HOLD_AFTER_MS 250
#define HOST_MENU_POINTER_HOLD_SLOP 26.0f
#define HOST_MENU_POINTER_NOTCH 28.0f
/* half of Apple's 44 pt minimum target */
#define HOST_MENU_POINTER_RADIUS 22.0f
#define HOST_MENU_POINTER_MAXIMUM_STEPS 8
/* a swipe in from the left edge: starts within the edge band and travels the distance (points) */
#define HOST_MENU_POINTER_EDGE_BAND 24.0f
#define HOST_MENU_POINTER_EDGE_TRAVEL 64.0f
/* a look and pinch (host_menu_gaze.h): half of visionOS's 60 pt minimum target, and a slop that a pinching
   hand's own wobble stays inside */
#define HOST_MENU_POINTER_GAZE_RADIUS 30.0f
#define HOST_MENU_POINTER_GAZE_SLOP 20.0f

/* The surface's size in points and the drawable's in pixels (0: unknown), written on whichever thread sees the
   view change and read on the game thread at each tap: a sequence lock publishes the four as one snapshot, so a tap
   never maps through one size's width and another's height. */
struct host_pointer_geometry {
    float surface_width, surface_height, drawable_width, drawable_height;
};
struct host_pointer_geometry_lock {
    atomic_flag writing;
    _Atomic unsigned sequence;
    _Atomic uint32_t bits[4];
};

static inline void host_pointer_geometry_publish(struct host_pointer_geometry_lock *lock,
                                                 const struct host_pointer_geometry *g) {
    uint32_t bits[4];
    memcpy(bits, g, sizeof(bits));
    while (atomic_flag_test_and_set_explicit(&lock->writing, memory_order_acquire)) {}
    unsigned sequence = atomic_load_explicit(&lock->sequence, memory_order_relaxed);
    atomic_store_explicit(&lock->sequence, sequence + 1, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
    for (int i = 0; i < 4; i++) atomic_store_explicit(&lock->bits[i], bits[i], memory_order_relaxed);
    atomic_store_explicit(&lock->sequence, sequence + 2, memory_order_release);
    atomic_flag_clear_explicit(&lock->writing, memory_order_release);
}

static inline struct host_pointer_geometry host_pointer_geometry_snapshot(struct host_pointer_geometry_lock *lock) {
    struct host_pointer_geometry g;
    uint32_t bits[4];
    unsigned before, after;
    do {
        before = atomic_load_explicit(&lock->sequence, memory_order_acquire);
        for (int i = 0; i < 4; i++) bits[i] = atomic_load_explicit(&lock->bits[i], memory_order_relaxed);
        atomic_thread_fence(memory_order_acquire);
        after = atomic_load_explicit(&lock->sequence, memory_order_relaxed);
    } while ((before & 1) || before != after);
    memcpy(&g, bits, sizeof(g));
    return g;
}

struct host_menu_pointer {
    int menus_active, blocked;
    struct host_pointer_geometry_lock geometry;
    /* the finger being followed, if any */
    int tracking, device_held;
    intptr_t finger;
    float down_x, down_y, anchor_y;
    unsigned long long down_ms;
    int scrolling;
    /* the finger's swipe from the left edge has counted: it is not a click or a scroll */
    int swiped, back_swipes;
    /* the latest position, and what has happened since the last read */
    float x, y, click_x, click_y;
    int moved, left_clicks, right_clicks, wheel_steps;
    /* the latest input was a finger (1) or a look and pinch (2; it needs the reach), or a pointer device (0) */
    int touch;
};

/* A system text prompt belongs to direct touch, never a mouse, gaze, or a hardware keyboard. */
static inline int host_menu_pointer_auto_keyboard(const struct host_menu_pointer *m, int keyboard) {
    return m->touch == 1 && !keyboard;
}

static inline void host_menu_pointer_clear(struct host_menu_pointer *m) {
    m->device_held = 0;
    m->tracking = 0;
    m->scrolling = 0;
    m->swiped = m->back_swipes = 0;
    m->moved = m->left_clicks = m->right_clicks = m->wheel_steps = 0;
}
/* the surface the pointer's points are on, and the drawable the picture is fit into there (0x0: unknown) */
static inline void host_menu_pointer_set_geometry(struct host_menu_pointer *m, float surface_width,
                                                  float surface_height, float drawable_width,
                                                  float drawable_height) {
    struct host_pointer_geometry g = {surface_width, surface_height, drawable_width, drawable_height};
    host_pointer_geometry_publish(&m->geometry, &g);
}
static inline void host_menu_pointer_set_surface(struct host_menu_pointer *m, float width, float height) {
    host_menu_pointer_set_geometry(m, width, height, 0.0f, 0.0f);
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

static inline unsigned long long host_menu_pointer_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000ull + (unsigned long long)ts.tv_nsec / 1000000ull;
}

static inline void host_menu_pointer_touch_begin(struct host_menu_pointer *m, intptr_t finger, float x, float y) {
    if (!host_menu_pointer_listening(m) || m->tracking || m->device_held) return;
    m->tracking = 1;
    m->finger = finger;
    m->scrolling = 0;
    m->swiped = 0;
    m->down_x = m->x = x;
    m->down_y = m->y = y;
    m->touch = 1;
    m->down_ms = host_menu_pointer_now_ms();
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
    if (m->touch == 1 && m->swiped) return;
    if (m->touch == 1 && !m->scrolling && m->down_x <= HOST_MENU_POINTER_EDGE_BAND &&
        x - m->down_x >= HOST_MENU_POINTER_EDGE_TRAVEL && fabsf(y - m->down_y) < x - m->down_x) {
        m->swiped = 1;
        m->back_swipes++;
        return;
    }
    m->x = x;
    m->y = y;
    if (!m->scrolling) {
        float slop = m->touch == 2 ? HOST_MENU_POINTER_GAZE_SLOP : HOST_MENU_POINTER_SLOP;
        if (m->touch == 1 && host_menu_pointer_now_ms() - m->down_ms >= HOST_MENU_POINTER_HOLD_AFTER_MS)
            slop = HOST_MENU_POINTER_HOLD_SLOP;
        if (hypotf(x - m->down_x, y - m->down_y) <= slop) return;
        m->scrolling = 1;
        m->anchor_y = m->down_y;
    }
    /* a dragging finger also moves the pointer, so the row under it takes the highlight */
    m->moved = 1;
    float travel = y - m->anchor_y;
    int steps = (int)(travel / HOST_MENU_POINTER_NOTCH);
    if (steps) {
        /* the list follows the finger as a scrolled page does: a finger moving up brings the later items (the host's
           steps are positive down the screen; the guest negates them, halo_ui_wheel_steps_from_host) */
        m->wheel_steps -= steps;
        m->anchor_y += (float)steps * HOST_MENU_POINTER_NOTCH;
    }
}
static inline void host_menu_pointer_touch_end(struct host_menu_pointer *m, intptr_t finger) {
    if (!m->tracking || m->finger != finger) return;
    if (!m->scrolling && !m->swiped && host_menu_pointer_listening(m)) {
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
/* A complete synthetic click for scripts and callers without separate button events. */
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
/* Physical mouse clicks are reported on release so sliders can distinguish a tap
   from a held drag. Scripted instantaneous clicks retain device_click above. */
static inline void host_menu_pointer_device_button(struct host_menu_pointer *m, int down, float x, float y) {
    if (!host_menu_pointer_listening(m)) return;
    host_menu_pointer_device_move(m, x, y);
    if (!down && m->device_held) host_menu_pointer_device_click(m, 0, x, y);
    m->device_held = down != 0;
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
    struct host_pointer_geometry g = host_pointer_geometry_snapshot(&m->geometry);
    out->surface_width = g.surface_width;
    out->surface_height = g.surface_height;
    out->drawable_width = g.drawable_width;
    out->drawable_height = g.drawable_height;
    out->radius = m->touch == 2 ? HOST_MENU_POINTER_GAZE_RADIUS : m->touch ? HOST_MENU_POINTER_RADIUS : 0.0f;
    out->picture = m->touch == 2;
    if (!host_menu_pointer_listening(m)) {
        out->down = 0;
        host_menu_pointer_clear(m);
        return;
    }
    out->held = m->tracking || m->device_held;
    out->moved = m->moved;
    out->left_clicks = m->left_clicks;
    out->right_clicks = m->right_clicks;
    out->back_swipes = m->back_swipes;
    out->down = m->tracking && m->touch == 1 && !m->scrolling && !m->swiped;
    out->wheel_steps = steps < -HOST_MENU_POINTER_MAXIMUM_STEPS ? -HOST_MENU_POINTER_MAXIMUM_STEPS :
        steps > HOST_MENU_POINTER_MAXIMUM_STEPS ? HOST_MENU_POINTER_MAXIMUM_STEPS : steps;
    m->moved = m->left_clicks = m->right_clicks = m->wheel_steps = 0;
    m->back_swipes = 0;
}
#endif
