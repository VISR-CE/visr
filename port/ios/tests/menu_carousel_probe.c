/* The carousel's binding, geometry, focus, drag, and tap rules without game data. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "halo_ui_carousel.h"

#define FRAME (1.0f / 60.0f)

static void settle(struct halo_ui_drag *drag)
{
    int frames = 0;
    while (halo_ui_drag_step(drag, FRAME)) assert(++frames < 600);
}

static void binding(void)
{
    const int counts[] = {1, 2, 3, 4, 10};
    for (unsigned i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) {
        int count = counts[i];
        int maximum = count > 3 ? count - 3 : 0;
        assert(halo_ui_carousel_clamp_first(-10, count) == 0);
        assert(halo_ui_carousel_clamp_first(100, count) == maximum);
        assert(halo_ui_carousel_max_first(count) == maximum);
        for (int first = 0; first <= maximum; first++) {
            for (int slot = 0; slot < 3; slot++) {
                int item = halo_ui_carousel_bound_item(first, slot, count);
                assert(item == (first + slot < count ? first + slot : -1));
            }
        }
    }
    assert(halo_ui_carousel_bound_item(0, 0, 0) == -1);
    assert(halo_ui_carousel_bound_item(0, -1, 10) == -1);
    assert(halo_ui_carousel_bound_item(0, 3, 10) == -1);
    assert(halo_ui_carousel_settled_first(2.4f * 167.0f, 10) == 2);
    assert(halo_ui_carousel_settled_first(2000.0f, 10) == 7);
}

static void geometry(void)
{
    int begin, end;
    assert(halo_ui_carousel_origin_x(3) == 74.0f);
    assert(halo_ui_carousel_origin_x(2) == 157.5f);
    assert(halo_ui_carousel_origin_x(1) == 241.0f);
    assert(halo_ui_carousel_item_x(1, 2.0f, 10) == -93.0f);
    assert(halo_ui_carousel_item_x(5, 2.0f, 10) == 575.0f);
    halo_ui_carousel_visible_range(0.0f, 10, &begin, &end);
    assert(begin == 0 && end == 4);
    halo_ui_carousel_visible_range(2.0f, 10, &begin, &end);
    assert(begin == 1 && end == 6);
    halo_ui_carousel_visible_range(2.5f, 10, &begin, &end);
    assert(begin == 2 && end == 6);
    halo_ui_carousel_visible_range(7.0f, 10, &begin, &end);
    assert(begin == 6 && end == 10);
    /* A card touching the frame's left edge with its right edge is invisible. */
    halo_ui_carousel_visible_range(232.0f / 167.0f, 10, &begin, &end);
    assert(begin == 1);
    for (int count = 0; count <= 3; count++) {
        halo_ui_carousel_visible_range(0.0f, count, &begin, &end);
        assert(begin == 0 && end == count);
    }
}

static void controller(void)
{
    struct halo_ui_drag drag;
    int focus = 2, first = 0;
    halo_ui_drag_reset(&drag, 0.0f);
    assert(halo_ui_carousel_controller_move(&drag, &focus, &first, 10, 1));
    assert(focus == 3 && first == 1);
    assert(drag.state == HALO_UI_DRAG_SETTLING && drag.target == 167.0f);
    settle(&drag);
    focus = 9; first = 7;
    halo_ui_drag_reset(&drag, 7.0f * 167.0f);
    assert(!halo_ui_carousel_controller_move(&drag, &focus, &first, 10, 1));
    assert(focus == 9 && first == 7 && drag.state == HALO_UI_DRAG_IDLE);
    focus = 7;
    assert(halo_ui_carousel_controller_move(&drag, &focus, &first, 10, -1));
    assert(focus == 6 && first == 6 && drag.target == 6.0f * 167.0f);
    focus = 0; first = 0;
    halo_ui_drag_reset(&drag, 0.0f);
    assert(!halo_ui_carousel_controller_move(&drag, &focus, &first, 10, -1));
    assert(focus == 0 && first == 0);
    assert(halo_ui_carousel_controller_move(&drag, &focus, &first, 10, 1));
    assert(focus == 1 && first == 0 && drag.state == HALO_UI_DRAG_IDLE);
    assert(halo_ui_carousel_focus_in_window(1, 3, 10) == 3);
    assert(halo_ui_carousel_focus_in_window(8, 3, 10) == 5);
    assert(halo_ui_carousel_focus_in_window(4, 3, 10) == 4);
    assert(halo_ui_carousel_focus_in_window(8, 0, 2) == 1);
}

static void drag_and_tap(void)
{
    struct halo_ui_drag drag;
    float maximum = halo_ui_carousel_max_first(10) * 167.0f;
    halo_ui_drag_reset(&drag, 167.0f);
    halo_ui_drag_press(&drag, 500.0f, 100.0f);
    halo_ui_drag_move(&drag, 480.0f, 100.0f, 10.0f, 0.0f, 0.0f, maximum);
    halo_ui_drag_move(&drag, 480.0f - 2.4f * 167.0f, 100.0f, 10.0f, 0.0f, 0.0f, maximum);
    assert(!halo_ui_drag_release(&drag, 167.0f, 0.0f, maximum));
    settle(&drag);
    assert(halo_ui_carousel_settled_first(drag.offset, 10) == 3);
    halo_ui_drag_reset(&drag, maximum);
    halo_ui_drag_press(&drag, 500.0f, 100.0f);
    halo_ui_drag_move(&drag, 480.0f, 100.0f, 10.0f, 0.0f, 0.0f, maximum);
    halo_ui_drag_move(&drag, 80.0f, 100.0f, 10.0f, 0.0f, 0.0f, maximum);
    assert(drag.offset > maximum && drag.offset < maximum + 400.0f);
    halo_ui_drag_release(&drag, 167.0f, 0.0f, maximum);
    settle(&drag);
    assert(drag.offset == maximum);
    halo_ui_drag_reset(&drag, 0.0f);
    halo_ui_drag_press(&drag, 500.0f, 100.0f);
    for (int i = 1; i <= 4; i++)
        halo_ui_drag_move(&drag, 500.0f - 30.0f * i, 100.0f, 10.0f, FRAME, 0.0f, maximum);
    float slow_target = halo_ui_drag_snap(drag.offset, 167.0f, 0.0f, maximum);
    halo_ui_drag_release(&drag, 167.0f, 0.0f, maximum);
    assert(drag.target > slow_target);
    int item = -1;
    halo_ui_drag_reset(&drag, 2.0f * 167.0f);
    halo_ui_drag_press(&drag, 250.0f, 100.0f);
    halo_ui_drag_move(&drag, 256.0f, 104.0f, 10.0f, FRAME, 0.0f, maximum);
    assert(halo_ui_drag_release(&drag, 167.0f, 0.0f, maximum));
    assert(halo_ui_carousel_tap(250.0f, 2.0f, 10, &item) == HALO_UI_CAROUSEL_TAP_ACTIVATE && item == 3);
    assert(halo_ui_carousel_tap(20.0f, 2.0f, 10, &item) == HALO_UI_CAROUSEL_TAP_PEEK_LEFT && item == 1);
    assert(halo_ui_carousel_tap(600.0f, 2.0f, 10, &item) == HALO_UI_CAROUSEL_TAP_PEEK_RIGHT && item == 5);
    assert(halo_ui_carousel_tap(20.0f, 0.0f, 10, &item) == HALO_UI_CAROUSEL_TAP_NONE);
    assert(halo_ui_carousel_tap(238.0f, 0.0f, 10, &item) == HALO_UI_CAROUSEL_TAP_NONE);
    assert(halo_ui_carousel_tap(650.0f, 2.0f, 10, &item) == HALO_UI_CAROUSEL_TAP_NONE);
    assert(halo_ui_carousel_tap(320.0f, 0.0f, 1, &item) == HALO_UI_CAROUSEL_TAP_ACTIVATE && item == 0);
    assert(halo_ui_carousel_tap(20.0f, 0.0f, 1, &item) == HALO_UI_CAROUSEL_TAP_NONE);
    assert(halo_ui_carousel_tap(320.0f, 0.0f, 0, &item) == HALO_UI_CAROUSEL_TAP_NONE);
    assert(halo_ui_carousel_tap(20.0f, 2.5f, 10, &item) == HALO_UI_CAROUSEL_TAP_PEEK_LEFT && item == 2);
    assert(halo_ui_carousel_tap(200.0f, 2.5f, 10, &item) == HALO_UI_CAROUSEL_TAP_ACTIVATE && item == 3);
}

int main(void)
{
    binding();
    geometry();
    controller();
    drag_and_tap();
    puts("menu carousel probe: ok");
    return 0;
}
