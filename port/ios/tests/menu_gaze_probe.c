/* Look and pinch on the Vision Pro's menus: the ray to the menus' surface, the surface's size and the hand's
   travel in points (host_menu_gaze.h), the pinch through the menus' pointer (host_menu_pointer.h), and the
   fraction of the picture in the menus' coordinates (halo_ui_pointer_map.h). */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "host_menu_gaze.h"
#include "halo_ui_pointer_map.h"


static int near(float a, float b, float tolerance) { return fabsf(a - b) <= tolerance; }

/* the UI's quad as host_stereo_hud_ui makes it: 2 m ahead, 1.2 m by 0.9 m, its frame turned by yaw (left
   positive) and standing at the head */
static struct host_menu_gaze_surface quad(float yaw, const float head[3]) {
    struct host_menu_gaze_surface s;
    float frame[16] = { cosf(yaw), 0, -sinf(yaw), 0, 0, 1, 0, 0, sinf(yaw), 0, cosf(yaw), 0, head[0], head[1], head[2], 1 };
    float center[3] = { 0, 0, -2 }, x_axis[3] = { 0.6f, 0, 0 }, y_axis[3] = { 0, 0.45f, 0 };
    host_menu_gaze_surface_set(&s, frame, center, x_axis, y_axis, head, 10.0);
    return s;
}

static void hit(void) {
    const float head[3] = { 0.5f, 1.6f, -1.0f };
    struct host_menu_gaze_surface s = quad(0, head);
    float u, v;
    /* straight ahead: the middle */
    float ahead[6] = { 0.5f, 1.6f, -1.0f, 0, 0, -1 };
    assert(host_menu_gaze_hit(&s, ahead, ahead + 3, 0, &u, &v) && near(u, 0.5f, 1e-4f) && near(v, 0.5f, 1e-4f));
    /* up and right: the top right quarter; the direction's length does not matter */
    float corner[6] = { 0.5f, 1.6f, -1.0f, 0.3f * 5, 0.225f * 5, -2 * 5 };
    assert(host_menu_gaze_hit(&s, corner, corner + 3, 0, &u, &v) && near(u, 0.75f, 1e-4f) && near(v, 0.25f, 1e-4f));
    /* from elsewhere (the selection ray starts between the eyes, not at the head's center) */
    float aside[6] = { 0.0f, 1.6f, -1.0f, 0.5f - 0.6f, 0.45f, -2 };
    assert(host_menu_gaze_hit(&s, aside, aside + 3, 0, &u, &v) && near(u, 0.0f, 1e-4f) && near(v, 0.0f, 1e-4f));
    /* behind, alongside, off it */
    float behind[6] = { 0.5f, 1.6f, -1.0f, 0, 0, 1 };
    float along[6] = { 0.5f, 1.6f, -1.0f, 1, 0, 0 };
    float off[6] = { 0.5f, 1.6f, -1.0f, 0.8f, 0, -2 };
    assert(!host_menu_gaze_hit(&s, behind, behind + 3, 0.1f, &u, &v));
    assert(!host_menu_gaze_hit(&s, along, along + 3, 0.1f, &u, &v));
    assert(!host_menu_gaze_hit(&s, off, off + 3, 0.1f, &u, &v));
    /* just past the edge: only with the margin, and outside 0 to 1 */
    float edge[6] = { 0.5f, 1.6f, -1.0f, 0.63f, 0, -2 };
    assert(!host_menu_gaze_hit(&s, edge, edge + 3, 0, &u, &v));
    assert(host_menu_gaze_hit(&s, edge, edge + 3, 0.1f, &u, &v) && u > 1.0f && u < 1.05f);
    /* the quad turned 90 degrees left (it follows the head's yaw): ahead is now -x */
    s = quad((float)M_PI / 2, head);
    float left[6] = { 0.5f, 1.6f, -1.0f, -1, 0, 0 };
    assert(host_menu_gaze_hit(&s, left, left + 3, 0, &u, &v) && near(u, 0.5f, 1e-4f) && near(v, 0.5f, 1e-4f));
    /* its right edge is toward -z now */
    float left_right[6] = { 0.5f, 1.6f, -1.0f, -2, 0, -0.6f };
    assert(host_menu_gaze_hit(&s, left_right, left_right + 3, 0.01f, &u, &v) && near(u, 1.0f, 1e-4f));
    /* a frame turned and moved as note_menu_quad composes it (the device's pose times the level frame): the
       quad's center lands 2 m along the turned forward from the moved origin, its axes turned with it */
    {
        const float origin[3] = { 3, 1, -4 };
        struct host_menu_gaze_surface t = quad((float)M_PI / 4, origin);
        float forward[3] = { -sinf((float)M_PI / 4), 0, -cosf((float)M_PI / 4) };
        float ray[6] = { 3, 1, -4, forward[0], forward[1], forward[2] };
        assert(near(t.center[0], 3 + 2 * forward[0], 1e-4f) && near(t.center[2], -4 + 2 * forward[2], 1e-4f));
        assert(near(t.x_axis[0], 0.6f * cosf((float)M_PI / 4), 1e-4f) && near(t.x_axis[2], -0.6f * sinf((float)M_PI / 4), 1e-4f));
        assert(host_menu_gaze_hit(&t, ray, ray + 3, 0, &u, &v) && near(u, 0.5f, 1e-4f) && near(v, 0.5f, 1e-4f));
    }
    /* an unusable surface takes nothing */
    struct host_menu_gaze_surface none;
    memset(&none, 0, sizeof(none));
    assert(!host_menu_gaze_hit(&none, ahead, ahead + 3, 1, &u, &v));
}

static void sizes(void) {
    const float head[3] = { 0, 0, 0 };
    struct host_menu_gaze_surface s = quad(0, head);
    float width, height, dx, dy;
    /* 1.2 m by 0.9 m at 2 m: about 580 by 442 points (a point is a milliradian) */
    host_menu_gaze_points(&s, head, &width, &height);
    assert(near(width, 2000.0f * atanf(0.3f), 0.5f) && near(height, 2000.0f * atanf(0.225f), 0.5f));
    assert(width > 570 && width < 590);
    /* the hand's travel along the surface: right is right, down is down, toward it is nothing */
    float travel[3] = { 0.01f, -0.02f, -0.05f };
    host_menu_gaze_travel(&s, travel, &dx, &dy);
    assert(near(dx, 10.0f, 1e-3f) && near(dy, 20.0f, 1e-3f));
    /* turned: travel along the turned axes */
    s = quad((float)M_PI / 2, head);
    float turned[3] = { 0, 0, -0.01f };
    host_menu_gaze_travel(&s, turned, &dx, &dy);
    assert(near(dx, 10.0f, 1e-3f) && near(dy, 0.0f, 1e-3f));
    /* freshness */
    assert(host_menu_gaze_surface_fresh(&s, 10.2) && !host_menu_gaze_surface_fresh(&s, 11.0) &&
        !host_menu_gaze_surface_fresh(&s, 9.0) && !host_menu_gaze_surface_fresh(NULL, 10.0));
}

static struct host_menu_pointer menus(int up) {
    struct host_menu_pointer m;
    memset(&m, 0, sizeof(m));
    host_menu_pointer_set_menus(&m, up);
    return m;
}
static struct host_ui_pointer take(struct host_menu_pointer *m) {
    struct host_ui_pointer out;
    host_menu_pointer_read(m, &out);
    return out;
}

static void pinches(void) {
    const float head[3] = { 0, 1.5f, 0 };
    struct host_menu_gaze_surface s = quad(0, head);
    struct host_menu_pointer m = menus(1);
    struct host_menu_gaze g;
    struct host_ui_pointer p;
    float at_item[6] = { 0, 1.5f, 0, 0.3f, 0.225f, -2 };   /* the top right quarter */
    float hand[3] = { 0.2f, 1.2f, -0.3f };
    memset(&g, 0, sizeof(g));

    /* a pinch: focus where it looks, at the surface's size in points, as a fraction of the picture */
    assert(host_menu_gaze_event(&g, &m, &s, 7, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    p = take(&m);
    assert(p.active && p.moved && !p.left_clicks && p.picture == 1 && p.radius == HOST_MENU_POINTER_GAZE_RADIUS);
    assert(near(p.x / p.surface_width, 0.75f, 1e-3f) && near(p.y / p.surface_height, 0.25f, 1e-3f));
    /* the hand wobbles 1.5 cm while pinched (inside the slop), then lets go: a press where it looked */
    float wobble[3] = { 0.215f, 1.2f, -0.3f };
    assert(host_menu_gaze_event(&g, &m, &s, 7, HOST_MENU_GAZE_ACTIVE, at_item, wobble));
    assert(host_menu_gaze_event(&g, &m, &s, 7, HOST_MENU_GAZE_ENDED, at_item, wobble));
    p = take(&m);
    assert(p.left_clicks == 1 && !p.wheel_steps && near(p.click_x / p.surface_width, 0.75f, 1e-3f));
    assert(!g.tracking && !m.tracking);

    /* a pinch and drag down 10 cm: past the slop, list steps down, no press */
    assert(host_menu_gaze_event(&g, &m, &s, 8, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    take(&m);
    float down[3] = { 0.2f, 1.1f, -0.3f };
    host_menu_gaze_event(&g, &m, &s, 8, HOST_MENU_GAZE_ACTIVE, at_item, down);
    host_menu_gaze_event(&g, &m, &s, 8, HOST_MENU_GAZE_ENDED, at_item, down);
    p = take(&m);
    /* 100 points of travel from where it began (past the 20 of slop): three 28 point notches */
    assert(!p.left_clicks && p.wheel_steps == 3);

    /* a second pinch while one is held is ignored, to its end */
    assert(host_menu_gaze_event(&g, &m, &s, 9, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    assert(!host_menu_gaze_event(&g, &m, &s, 10, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    assert(!host_menu_gaze_event(&g, &m, &s, 10, HOST_MENU_GAZE_ENDED, at_item, hand));
    take(&m);
    /* a cancel presses nothing */
    assert(host_menu_gaze_event(&g, &m, &s, 9, HOST_MENU_GAZE_CANCELLED, at_item, hand));
    p = take(&m);
    assert(!p.left_clicks && !g.tracking);

    /* a pinch that looks away from the menus is not theirs, even if it drags onto them */
    float away[6] = { 0, 1.5f, 0, 0, 0, 1 };
    assert(!host_menu_gaze_event(&g, &m, &s, 11, HOST_MENU_GAZE_ACTIVE, away, hand));
    assert(!host_menu_gaze_event(&g, &m, &s, 11, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    assert(!host_menu_gaze_event(&g, &m, &s, 11, HOST_MENU_GAZE_ENDED, at_item, hand));
    p = take(&m);
    assert(!p.moved && !p.left_clicks);
    /* without a ray, or with no surface drawn: nothing */
    assert(!host_menu_gaze_event(&g, &m, &s, 12, HOST_MENU_GAZE_ACTIVE, NULL, hand));
    assert(!host_menu_gaze_event(&g, &m, &s, 12, HOST_MENU_GAZE_ENDED, NULL, hand));
    assert(!host_menu_gaze_event(&g, &m, NULL, 13, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    assert(!host_menu_gaze_event(&g, &m, NULL, 13, HOST_MENU_GAZE_ENDED, at_item, hand));
    /* a pinch without the hand's pose still presses */
    assert(host_menu_gaze_event(&g, &m, &s, 14, HOST_MENU_GAZE_ACTIVE, at_item, NULL));
    assert(host_menu_gaze_event(&g, &m, &s, 14, HOST_MENU_GAZE_ACTIVE, at_item, NULL));
    host_menu_gaze_event(&g, &m, &s, 14, HOST_MENU_GAZE_ENDED, at_item, NULL);
    p = take(&m);
    assert(p.left_clicks == 1);

    /* in play (no menu up) a pinch is never the menus': tracked aim and the game keep it */
    struct host_menu_pointer play = menus(0);
    struct host_menu_gaze g2;
    memset(&g2, 0, sizeof(g2));
    assert(!host_menu_gaze_event(&g2, &play, &s, 20, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    assert(!host_menu_gaze_event(&g2, &play, &s, 20, HOST_MENU_GAZE_ENDED, at_item, hand));
    p = take(&play);
    assert(!p.moved && !p.left_clicks && !p.wheel_steps);
    /* the menus going away mid-pinch drops it, and its release presses nothing when they come back */
    struct host_menu_pointer m2 = menus(1);
    memset(&g2, 0, sizeof(g2));
    assert(host_menu_gaze_event(&g2, &m2, &s, 21, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    host_menu_pointer_set_menus(&m2, 0);
    host_menu_pointer_set_menus(&m2, 1);
    assert(!host_menu_gaze_event(&g2, &m2, &s, 21, HOST_MENU_GAZE_ENDED, at_item, hand));
    p = take(&m2);
    assert(!p.left_clicks);
    /* a replay holding the controls takes it too */
    host_menu_pointer_set_blocked(&m2, 1);
    assert(!host_menu_gaze_event(&g2, &m2, &s, 22, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    host_menu_gaze_event(&g2, &m2, &s, 22, HOST_MENU_GAZE_ENDED, at_item, hand);
    host_menu_pointer_set_blocked(&m2, 0);
    p = take(&m2);
    assert(!p.left_clicks);
    /* the menus drop a pinch whose end never comes (the space closed mid-pinch): the next pinch still begins */
    struct host_menu_pointer m3 = menus(1);
    struct host_menu_gaze g3;
    memset(&g3, 0, sizeof(g3));
    assert(host_menu_gaze_event(&g3, &m3, &s, 30, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    host_menu_pointer_set_menus(&m3, 0);
    host_menu_pointer_set_menus(&m3, 1);
    assert(host_menu_gaze_event(&g3, &m3, &s, 31, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    assert(host_menu_gaze_event(&g3, &m3, &s, 31, HOST_MENU_GAZE_ENDED, at_item, hand));
    p = take(&m3);
    assert(p.left_clicks == 1);
    /* the dropped pinch's late events stay off the menus */
    assert(!host_menu_gaze_event(&g3, &m3, &s, 30, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    assert(!host_menu_gaze_event(&g3, &m3, &s, 30, HOST_MENU_GAZE_ENDED, at_item, hand));
    p = take(&m3);
    assert(!p.left_clicks && !p.moved);

    /* a pinch held in play stays the game's when the menus come up and the other hand pinches off them too */
    struct host_menu_pointer m4 = menus(0);
    struct host_menu_gaze g4;
    memset(&g4, 0, sizeof(g4));
    assert(!host_menu_gaze_event(&g4, &m4, &s, 40, HOST_MENU_GAZE_ACTIVE, at_item, hand));   /* in play */
    host_menu_pointer_set_menus(&m4, 1);
    assert(!host_menu_gaze_event(&g4, &m4, &s, 41, HOST_MENU_GAZE_ACTIVE, away, hand));      /* looks away */
    assert(!host_menu_gaze_event(&g4, &m4, &s, 40, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    assert(!host_menu_gaze_event(&g4, &m4, &s, 40, HOST_MENU_GAZE_ENDED, at_item, hand));
    p = take(&m4);
    assert(!p.left_clicks && !p.moved);
    /* and once both have ended, a new pinch on the menus is theirs */
    assert(!host_menu_gaze_event(&g4, &m4, &s, 41, HOST_MENU_GAZE_CANCELLED, away, hand));
    assert(g4.ignored_count == 0);
    assert(host_menu_gaze_event(&g4, &m4, &s, 42, HOST_MENU_GAZE_ACTIVE, at_item, hand));
    /* more held pinches than the room for them forgets the oldest, never the newest */
    struct host_menu_gaze g5;
    memset(&g5, 0, sizeof(g5));
    for (intptr_t id = 100; id < 100 + HOST_MENU_GAZE_IGNORED + 2; id++)
        host_menu_gaze_ignore(&g5, id);
    assert(g5.ignored_count == HOST_MENU_GAZE_IGNORED && !host_menu_gaze_ignored(&g5, 100) &&
        host_menu_gaze_ignored(&g5, 100 + HOST_MENU_GAZE_IGNORED + 1));

    /* a finger afterward is a finger again: no picture fractions, the touch reach */
    host_menu_pointer_touch_begin(&m2, 1, 10, 10);
    p = take(&m2);
    assert(!p.picture && p.radius == HOST_MENU_POINTER_RADIUS);
}

static void picture(void) {
    short x, y;
    /* a 16:9 screen is 853 wide: the menus' 640 columns from 106.5 */
    halo_ui_pointer_map_picture(0.5f, 0.5f, 853, &x, &y);
    assert(abs(x - 320) <= 1 && y == 240);
    halo_ui_pointer_map_picture(0.0f, 0.0f, 853, &x, &y);
    assert(x == -107 && y == 0);
    halo_ui_pointer_map_picture(0.9999f, 0.9999f, 640, &x, &y);
    assert(x == 639 && y == 479);
    /* far off the picture, or no width: nothing */
    halo_ui_pointer_map_picture(2.0f, 0.5f, 640, &x, &y);
    assert(x == -1 && y == -1);
    halo_ui_pointer_map_picture(0.5f, 0.5f, 0, &x, &y);
    assert(x == -1 && y == -1);
}

int main(void) {
    hit();
    sizes();
    pinches();
    picture();
    puts("menu gaze: passed");
}
