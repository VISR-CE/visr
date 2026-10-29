/* Fingers and pointer devices on the menus: the state machine (host_menu_pointer.h) and the
   mapping from a surface to the menus' coordinates (halo_ui_pointer_map.h). */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "host_menu_pointer.h"
#include "halo_ui_pointer_map.h"
#include "halo_display.h"

static struct host_menu_pointer fresh(void) {
    struct host_menu_pointer m;
    memset(&m, 0, sizeof(m));
    host_menu_pointer_set_surface(&m, 874, 402);
    host_menu_pointer_set_menus(&m, 1);
    return m;
}
static struct host_ui_pointer take(struct host_menu_pointer *m) {
    struct host_ui_pointer out;
    host_menu_pointer_read(m, &out);
    return out;
}

static void tap(void) {
    struct host_menu_pointer m = fresh();
    struct host_ui_pointer p;
    host_menu_pointer_touch_begin(&m, 1, 100, 200);
    p = take(&m);
    assert(p.active && p.moved && !p.left_clicks && p.x == 100 && p.y == 200);
    assert(p.radius == HOST_MENU_POINTER_RADIUS && p.surface_width == 874);
    host_menu_pointer_touch_move(&m, 1, 104, 203);   /* inside the slop */
    host_menu_pointer_touch_end(&m, 1);
    p = take(&m);
    assert(!p.moved && p.left_clicks == 1 && p.click_x == 100 && p.click_y == 200 && !p.wheel_steps);
    p = take(&m);                                    /* a read clears */
    assert(!p.moved && !p.left_clicks);
    /* a tap that begins and ends between two reads is both a move and a click */
    host_menu_pointer_touch_begin(&m, 2, 10, 20);
    host_menu_pointer_touch_end(&m, 2);
    p = take(&m);
    assert(p.moved && p.left_clicks == 1 && p.click_x == 10);
}

static void drag(void) {
    struct host_menu_pointer m = fresh();
    struct host_ui_pointer p;
    host_menu_pointer_touch_begin(&m, 1, 100, 200);
    take(&m);
    host_menu_pointer_touch_move(&m, 1, 100, 215);   /* past the slop: scrolling, no notch yet */
    p = take(&m);
    assert(!p.wheel_steps && !p.moved);
    host_menu_pointer_touch_move(&m, 1, 100, 200 + 10.0f + 2 * HOST_MENU_POINTER_NOTCH + 3);   /* two notches down */
    p = take(&m);
    assert(p.wheel_steps == -2);
    host_menu_pointer_touch_move(&m, 1, 100, 200 + 10.0f + 2 * HOST_MENU_POINTER_NOTCH - 3);   /* the remainder carries */
    host_menu_pointer_touch_move(&m, 1, 100, 100);                                                /* a long way up */
    p = take(&m);
    assert(p.wheel_steps > 0 && p.wheel_steps <= HOST_MENU_POINTER_MAXIMUM_STEPS);
    host_menu_pointer_touch_end(&m, 1);
    p = take(&m);
    assert(!p.left_clicks);                          /* a drag is not a tap */
    host_menu_pointer_touch_begin(&m, 1, 0, 0);
    take(&m);
    host_menu_pointer_touch_move(&m, 1, 0, 5000);
    p = take(&m);
    assert(p.wheel_steps == -HOST_MENU_POINTER_MAXIMUM_STEPS);
}

static void fingers(void) {
    struct host_menu_pointer m = fresh();
    struct host_ui_pointer p;
    host_menu_pointer_touch_begin(&m, 1, 100, 200);
    host_menu_pointer_touch_begin(&m, 2, 300, 300);  /* a second finger is ignored */
    host_menu_pointer_touch_move(&m, 2, 300, 900);
    host_menu_pointer_touch_end(&m, 2);
    p = take(&m);
    assert(p.x == 100 && !p.left_clicks && !p.wheel_steps);
    host_menu_pointer_touch_cancel(&m, 1);           /* a cancel leaves nothing */
    host_menu_pointer_touch_end(&m, 1);
    p = take(&m);
    assert(!p.left_clicks);
    host_menu_pointer_touch_begin(&m, 3, 50, 60);    /* and the next finger is followed */
    host_menu_pointer_touch_end(&m, 3);
    assert(take(&m).left_clicks == 1);
}

static void gates(void) {
    struct host_menu_pointer m = fresh();
    struct host_ui_pointer p;
    /* no menu: nothing is tracked, and the read still says the source is there */
    host_menu_pointer_set_menus(&m, 0);
    host_menu_pointer_touch_begin(&m, 1, 1, 1);
    host_menu_pointer_touch_end(&m, 1);
    host_menu_pointer_device_click(&m, 0, 1, 1);
    p = take(&m);
    assert(p.active && !p.moved && !p.left_clicks);
    /* what happened before the menu came up is not for it */
    host_menu_pointer_set_menus(&m, 1);
    assert(!take(&m).left_clicks);
    host_menu_pointer_touch_begin(&m, 1, 5, 5);
    host_menu_pointer_set_menus(&m, 0);              /* the menu goes away mid-touch */
    host_menu_pointer_set_menus(&m, 1);
    host_menu_pointer_touch_end(&m, 1);
    assert(!take(&m).left_clicks);
    /* a replay holds the controls */
    host_menu_pointer_set_blocked(&m, 1);
    host_menu_pointer_touch_begin(&m, 1, 5, 5);
    host_menu_pointer_touch_end(&m, 1);
    assert(!take(&m).moved);
    host_menu_pointer_set_blocked(&m, 0);
    host_menu_pointer_touch_begin(&m, 1, 5, 5);
    host_menu_pointer_set_blocked(&m, 1);
    host_menu_pointer_touch_end(&m, 1);
    host_menu_pointer_set_blocked(&m, 0);
    assert(!take(&m).left_clicks);
}

static void devices(void) {
    struct host_menu_pointer m = fresh();
    struct host_ui_pointer p;
    host_menu_pointer_device_move(&m, 30, 40);
    p = take(&m);
    assert(p.moved && p.x == 30 && p.y == 40 && p.radius == 0);
    host_menu_pointer_device_click(&m, 0, 31, 41);
    host_menu_pointer_device_click(&m, 1, 31, 41);
    host_menu_pointer_device_wheel(&m, 2);           /* away from the user is up the list: negative steps */
    p = take(&m);
    assert(p.left_clicks == 1 && p.right_clicks == 1 && p.click_x == 31 && p.wheel_steps == -2);
    /* a finger after a device has a reach again; a device after a finger has none */
    host_menu_pointer_touch_begin(&m, 1, 1, 1);
    assert(take(&m).radius == HOST_MENU_POINTER_RADIUS);
    host_menu_pointer_touch_end(&m, 1);
    host_menu_pointer_device_move(&m, 2, 2);
    assert(take(&m).radius == 0);
}

/* the menus' point (mx, my) drawn by the display blit, and back */
static void mapping(void) {
    static const int pictures[][2] = { {640, 480}, {854, 480}, {1280, 720}, {1920, 1080}, {2560, 1080}, {3840, 1600} };
    static const int surfaces[][2] = { {874, 402}, {1366, 1024}, {2420, 1668}, {402, 874}, {1920, 1080} };
    for (unsigned p = 0; p < sizeof(pictures) / sizeof(pictures[0]); p++)
        for (unsigned s = 0; s < sizeof(surfaces) / sizeof(surfaces[0]); s++) {
            int pw = pictures[p][0], ph = pictures[p][1], sw = surfaces[s][0], sh = surfaces[s][1];
            int screen = (int)(480.0 * pw / ph);
            if (screen < 640) screen = 640;
            struct halo_display_size fit = halo_display_present_size(sw, sh, pw, ph);
            /* (the target's size is in the game's screen units; the GL size is its pixels) */
            struct halo_ui_pointer_geometry g = { sw, sh, sw, sh, pw, ph, screen, 480, screen };
            for (int mx = 0; mx < 640; mx += 40)
                for (int my = 0; my < 480; my += 40) {
                    float picture_x = (mx + (screen - 640) / 2.0f) * (float)pw / screen;
                    float picture_y = my * (float)ph / 480;
                    float x = (sw - fit.width) / 2.0f + picture_x * fit.width / pw;
                    float y = (sh - fit.height) / 2.0f + picture_y * fit.height / ph;
                    short rx, ry;
                    halo_ui_pointer_map(&g, x + 0.5f, y + 0.5f, &rx, &ry);
                    assert(abs(rx - mx) <= 2 && abs(ry - my) <= 2);
                }
            /* a point in the bars is outside the menus */
            short rx, ry;
            if (fit.width < sw - 8) {
                halo_ui_pointer_map(&g, 1, sh / 2.0f, &rx, &ry);
                assert(rx < 0);
            }
            if (fit.height < sh - 8) {
                halo_ui_pointer_map(&g, sw / 2.0f, 1, &rx, &ry);
                assert(ry < 0);
            }
        }
    /* an unusable geometry maps to -1, -1 */
    struct halo_ui_pointer_geometry bad = { 0 };
    short x = 5, y = 5;
    halo_ui_pointer_map(&bad, 10, 10, &x, &y);
    assert(x == -1 && y == -1);
    /* a reach in points becomes the same reach in menu units at the picture's scale: 44 pt of a 536 pt wide menu */
    struct halo_ui_pointer_geometry g = { 874, 402, 874, 402, 640, 480, 640, 480, 640 };
    float units = halo_ui_pointer_length(&g, 22.0f);
    assert(units > 25.0f && units < 28.0f);
}

int main(void) {
    tap();
    drag();
    fingers();
    gates();
    devices();
    mapping();
    puts("menu pointer: passed");
}
