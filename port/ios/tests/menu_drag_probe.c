/* A finger dragging a strip of menu items (halo_ui_drag.h): tap or drag, following the finger, the pull past
   the ends, the coast and the settle on a whole item, and the host's finger-down state (host_menu_pointer.h). */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "host_menu_pointer.h"
#include "halo_ui_drag.h"

#define PITCH 100.0f
#define FRAME (1.0f / 60.0f)

static void settle(struct halo_ui_drag *d) {
    int frames = 0;
    while (halo_ui_drag_step(d, FRAME)) assert(++frames < 600);
}

static void tap(void) {
    struct halo_ui_drag d;
    halo_ui_drag_reset(&d, 200.0f);
    halo_ui_drag_press(&d, 50.0f, 50.0f);
    assert(!halo_ui_drag_move(&d, 56.0f, 54.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 400.0f));
    assert(d.offset == 200.0f);
    assert(halo_ui_drag_release(&d, PITCH, 0.0f, 400.0f) == 1);
    settle(&d);
    assert(d.offset == 200.0f && d.state == HALO_UI_DRAG_IDLE);
}

static void slow_drag(void) {
    struct halo_ui_drag d;
    halo_ui_drag_reset(&d, 0.0f);
    halo_ui_drag_press(&d, 300.0f, 10.0f);
    /* past the slop: the strip starts from there, not jumping by the slop */
    assert(halo_ui_drag_move(&d, 288.0f, 10.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 400.0f));
    assert(d.state == HALO_UI_DRAG_DRAGGING && d.offset == 0.0f);
    /* the finger moves up 60 more over a second, slowly: the strip follows it exactly */
    for (int i = 1; i <= 60; i++)
        halo_ui_drag_move(&d, 288.0f - (float)i, 10.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 400.0f);
    assert(fabsf(d.offset - 60.0f) < 0.001f);
    /* resting before the lift: no coast, settles on the nearest item */
    for (int i = 0; i < 12; i++)
        halo_ui_drag_move(&d, 228.0f, 10.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 400.0f);
    assert(halo_ui_drag_release(&d, PITCH, 0.0f, 400.0f) == 0);
    assert(d.target == 100.0f);
    settle(&d);
    assert(d.offset == 100.0f);
}

static void flick(void) {
    struct halo_ui_drag d;
    halo_ui_drag_reset(&d, 0.0f);
    halo_ui_drag_press(&d, 400.0f, 0.0f);
    /* 30 units a frame (1800 a second) up the screen */
    for (int i = 1; i <= 4; i++)
        halo_ui_drag_move(&d, 400.0f - 30.0f * (float)i, 0.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 1000.0f);
    assert(d.velocity > 1000.0f);
    halo_ui_drag_release(&d, PITCH, 0.0f, 1000.0f);
    /* it carries on well past where the finger left it, onto a whole item */
    assert(d.target >= 400.0f && fmodf(d.target, PITCH) == 0.0f);
    settle(&d);
    assert(d.offset == d.target);
    /* a hard flick toward the end stops at the end */
    halo_ui_drag_reset(&d, 900.0f);
    halo_ui_drag_press(&d, 400.0f, 0.0f);
    for (int i = 1; i <= 4; i++)
        halo_ui_drag_move(&d, 400.0f - 40.0f * (float)i, 0.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 1000.0f);
    halo_ui_drag_release(&d, PITCH, 0.0f, 1000.0f);
    assert(d.target == 1000.0f);
    /* and one back toward the start stops at the start */
    halo_ui_drag_reset(&d, 100.0f);
    halo_ui_drag_press(&d, 0.0f, 0.0f);
    for (int i = 1; i <= 4; i++)
        halo_ui_drag_move(&d, 40.0f * (float)i, 0.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 1000.0f);
    halo_ui_drag_release(&d, PITCH, 0.0f, 1000.0f);
    assert(d.target == 0.0f);
}

static void edges(void) {
    struct halo_ui_drag d;
    halo_ui_drag_reset(&d, 0.0f);
    halo_ui_drag_press(&d, 0.0f, 0.0f);
    /* pulled 100 down from the start: gives way by only part of it */
    halo_ui_drag_move(&d, 20.0f, 0.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 300.0f);
    for (int i = 0; i < 10; i++)
        halo_ui_drag_move(&d, 100.0f, 0.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 300.0f);
    assert(d.offset < 0.0f && d.offset > -80.0f * 0.5f);
    halo_ui_drag_release(&d, PITCH, 0.0f, 300.0f);
    settle(&d);
    assert(d.offset == 0.0f);
}

static void catch_and_glide(void) {
    struct halo_ui_drag d;
    halo_ui_drag_reset(&d, 0.0f);
    halo_ui_drag_glide(&d, 300.0f);
    for (int i = 0; i < 3; i++) halo_ui_drag_step(&d, FRAME);
    assert(d.offset > 0.0f && d.offset < 300.0f);
    /* a press catches the moving strip where it is; a tap there leaves it on the nearest item */
    float caught = d.offset;
    halo_ui_drag_press(&d, 0.0f, 0.0f);
    assert(d.offset == caught && d.state == HALO_UI_DRAG_PRESSED);
    halo_ui_drag_glide(&d, 0.0f);   /* (no glide while a finger holds it) */
    assert(d.state == HALO_UI_DRAG_PRESSED);
    assert(halo_ui_drag_release(&d, PITCH, 0.0f, 300.0f) == 1);
    assert(d.target == halo_ui_drag_snap(caught, PITCH, 0.0f, 300.0f));
    settle(&d);
    /* a canceled drag settles without a tap */
    halo_ui_drag_press(&d, 0.0f, 0.0f);
    halo_ui_drag_move(&d, -50.0f, 0.0f, HALO_UI_DRAG_SLOP_POINTS, FRAME, 0.0f, 300.0f);
    halo_ui_drag_cancel(&d, PITCH, 0.0f, 300.0f);
    settle(&d);
    assert(fmodf(d.offset, PITCH) == 0.0f);
}

static void host_down(void) {
    struct host_menu_pointer m;
    struct host_ui_pointer p;
    memset(&m, 0, sizeof(m));
    host_menu_pointer_set_surface(&m, 874, 402);
    host_menu_pointer_set_menus(&m, 1);
    assert(HOST_MENU_POINTER_SLOP == HALO_UI_DRAG_SLOP_POINTS);
    host_menu_pointer_touch_begin(&m, 1, 100, 300);
    host_menu_pointer_read(&m, &p);
    assert(p.held && p.y == 300);
    /* inside the slop the point still follows the finger, with no step and no click yet */
    host_menu_pointer_touch_move(&m, 1, 100, 295);
    host_menu_pointer_read(&m, &p);
    assert(p.held && p.y == 295 && !p.wheel_steps && !p.left_clicks);
    host_menu_pointer_touch_move(&m, 1, 100, 200);
    host_menu_pointer_read(&m, &p);
    assert(p.held && p.y == 200 && p.wheel_steps);
    host_menu_pointer_touch_end(&m, 1);
    host_menu_pointer_read(&m, &p);
    assert(!p.held && !p.left_clicks);
    /* a pointer device is never down */
    host_menu_pointer_device_move(&m, 10, 10);
    host_menu_pointer_device_click(&m, 0, 10, 10);
    host_menu_pointer_read(&m, &p);
    assert(!p.held && p.left_clicks == 1);
}

/* the class, host to guest: a drag down moves the highlight down (rows) once past the rows, and moves a dropdown's
   content with the finger (first falls); over a row nothing moves; a wheel keeps the d-pad's direction */
static void list_follows_finger(void) {
    struct host_menu_pointer m;
    struct host_ui_pointer p;
    int guest;
    memset(&m, 0, sizeof(m));
    host_menu_pointer_set_surface(&m, 874, 402);
    host_menu_pointer_set_menus(&m, 1);
    host_menu_pointer_touch_begin(&m, 1, 100, 300);
    host_menu_pointer_read(&m, &p);
    host_menu_pointer_touch_move(&m, 1, 100, 300.0f + 10.0f + 2 * HOST_MENU_POINTER_NOTCH + 3);   /* down */
    host_menu_pointer_read(&m, &p);
    assert(p.moved);
    guest = halo_ui_wheel_steps_from_host(p.wheel_steps);
    assert(halo_ui_list_wheel_steps(guest, 1, 0) > 0);    /* rows: the highlight goes down with the finger */
    assert(halo_ui_list_wheel_steps(guest, 1, 1) == 0);   /* over a row: hover owns the highlight */
    assert(-guest < 0);                                   /* dropdown: first falls, the options move down with it */
    host_menu_pointer_touch_move(&m, 1, 100, 100);   /* up */
    host_menu_pointer_read(&m, &p);
    guest = halo_ui_wheel_steps_from_host(p.wheel_steps);
    assert(halo_ui_list_wheel_steps(guest, 1, 0) < 0 && -guest > 0);
    /* a wheel turned toward the user (host steps positive) goes on down the list, as the d-pad does */
    assert(halo_ui_list_wheel_steps(halo_ui_wheel_steps_from_host(3), 0, 1) == 3);
    assert(halo_ui_wheel_steps_from_host(99) == -8 && halo_ui_wheel_steps_from_host(-99) == 8);
}

int main(void) {
    list_follows_finger();
    tap();
    slow_drag();
    flick();
    edges();
    catch_and_glide();
    host_down();
    puts("menu drag probe: ok");
    return 0;
}
