#include "host_posture_layout.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct host_posture_snapshot posture = {HOST_POSTURE_LAPTOP_L, 7, {0, 0, 669, 475}};
    struct host_hinge_snapshot hinge = {.division_active = 1, .division_y = 475,
        .division_width = 669, .division_height = 10, .generation = 4};
    struct host_posture_insets safe = {55, 55, 55, 55};
    struct host_posture_layout l = host_posture_layout_make(1, posture, hinge, 669, 960, safe, 3);
    assert(l.laptop && l.upper_half.width == 669 && l.upper_half.height == 475);
    assert(l.flat_half.y == 485 && l.flat_half.height == 475);
    assert(l.backdrop.y == 475 && l.backdrop.height == 485);
    assert(l.pause_button.width == 44 && l.pause_button.height == 44);
    assert(l.pause_button.x >= safe.left && l.pause_button.x + 44 <= 669 - safe.right);
    assert(l.pause_button.y >= 485 && l.pause_button.y + 44 <= 960 - safe.bottom);
    assert(l.safe_area.top == 55 && l.scale == 3 && l.hinge.generation == 4);
    struct host_posture_layout old = l;
    hinge.angle = 1.8; ++hinge.generation;
    l = host_posture_layout_make(1, posture, hinge, 669, 960, safe, 3);
    assert(!host_posture_layout_changed(&old, &l));
    hinge.division_height = 20;
    l = host_posture_layout_make(1, posture, hinge, 669, 960, safe, 3);
    assert(host_posture_layout_changed(&old, &l) && l.flat_half.y == 495);
    safe.bottom = 80;
    l = host_posture_layout_make(1, posture, hinge, 669, 960, safe, 3);
    assert(host_posture_layout_changed(&old, &l));
    for (int p = HOST_POSTURE_FLAT; p <= HOST_POSTURE_CLOSED; ++p) {
        if (p == HOST_POSTURE_LAPTOP_L) continue;
        posture.posture = p;
        l = host_posture_layout_make(1, posture, hinge, 669, 960, safe, 3);
        assert(!l.laptop && l.upper_half.height == 960 && !l.flat_half.height);
        assert(host_posture_layout_changed(&old, &l));
    }
    posture.posture = HOST_POSTURE_LAPTOP_L;
    l = host_posture_layout_make(0, posture, hinge, 669, 960, safe, 3);
    assert(!l.laptop && l.upper_half.height == 960);
    hinge.division_y = NAN;
    l = host_posture_layout_make(1, posture, hinge, 669, 960, safe, 3);
    assert(!l.laptop);
    hinge.division_y = 475; hinge.division_height = 10;
    l = host_posture_layout_make(1, posture, hinge, 669, 500, safe, 3);
    assert(l.laptop && !l.pause_button.width);
    l = host_posture_layout_make(1, posture, hinge, 669, 0, safe, 3);
    assert(!l.laptop);
    puts("PASS: posture picture, fold backdrop, flat region, safe pause target, and layout cancellation");
    return 0;
}
