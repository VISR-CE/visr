#include "host_posture.h"
#include "host_geometry_debounce.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static struct host_posture_inputs laptop(void)
{
    return (struct host_posture_inputs){
        .hinge = {.available = 1, .status = HOST_HINGE_PARTIALLY_OPEN,
            .angle = 2.5, .division_active = 1,
            .division_x = 0, .division_y = 470, .division_width = 669, .division_height = 10},
        .view_width = 669, .view_height = 951,
        .gravity_known = 1, .gravity_y = -1
    };
}

static void check_posture(void)
{
    struct host_posture_state state = {0};
    struct host_posture_inputs in = laptop();
    in.hinge.status = HOST_HINGE_FULLY_OPEN;
    in.hinge.angle = 3.141592653589793;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_FLAT);
    in.hinge.status = HOST_HINGE_PARTIALLY_OPEN;
    in.hinge.angle = HOST_POSTURE_L_ENTER_ANGLE + 0.01;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_FLAT);
    in.hinge.angle = HOST_POSTURE_L_ENTER_ANGLE - 0.01;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_LAPTOP_L);
    assert(state.upper_half.x == 0 && state.upper_half.y == 0);
    assert(state.upper_half.width == 669 && state.upper_half.height == 470);
    for (int i = 0; i < 10; ++i) {
        in.hinge.angle = HOST_POSTURE_L_ENTER_ANGLE + (i % 2 ? 0.05 : 0.15);
        assert(host_posture_update(&state, &in) == HOST_POSTURE_LAPTOP_L);
    }
    in.hinge.angle = HOST_POSTURE_L_EXIT_ANGLE + 0.01;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_FLAT);
    assert(state.upper_half.width == 0 && state.upper_half.height == 0);

    in = laptop();
    in.hinge.division_width = 10;
    in.hinge.division_height = 951;
    in.gravity_y = 1;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_BOOK_L);
    in = laptop();
    in.gravity_known = 0;
    in.hinge.angle = HOST_POSTURE_L_ENTER_ANGLE + 0.01;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_FLAT);
    in.hinge.angle = HOST_POSTURE_L_ENTER_ANGLE - 0.01;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_LAPTOP_L);
    in.hinge.status = HOST_HINGE_CLOSED;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_CLOSED);
    assert(state.upper_half.height == 0);

    /* Gravity does not matter for a horizontal fold in the L range, so a
       device lying nearly flat still reports a laptop (probe reading 13). */
    in = laptop();
    in.hinge.angle = 98 * 3.141592653589793 / 180;
    in.hinge.division_y = 455.5;
    in.hinge.division_height = 40;
    in.gravity_x = -0.17;
    in.gravity_y = 0.07;
    in.gravity_z = -0.98;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_LAPTOP_L);
    assert(state.upper_half.height == 455.5);
    in.gravity_x = 1;
    in.gravity_y = 0;
    in.gravity_z = 0;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_LAPTOP_L);
    in.gravity_known = 0;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_LAPTOP_L);
    /* A tilted upper half with the lower half level still qualifies. */
    in.hinge.angle = 2.3;
    in.gravity_y = -sin(3.141592653589793 - 2.3);
    in.gravity_z = -cos(3.141592653589793 - 2.3);
    assert(host_posture_update(&state, &in) == HOST_POSTURE_LAPTOP_L);
    in.hinge.division_y = 450;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_LAPTOP_L);
    assert(state.upper_half.height == 450);
    in.hinge.available = 0;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_FLAT);
    in = laptop();
    in.hinge.division_active = 0;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_FLAT);
    in = laptop();
    in.hinge.angle = NAN;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_FLAT);
    in = laptop();
    in.view_height = 0;
    assert(host_posture_update(&state, &in) == HOST_POSTURE_FLAT);
}

static void check_debounce(void)
{
    struct host_geometry_debounce debounce = {0};
    struct host_geometry geometry = {0};
    assert(!host_geometry_debounce_settled(&debounce, 1, &geometry));
    host_geometry_debounce_note(&debounce, 0, 466, 678, 1);
    host_geometry_debounce_note(&debounce, 0.03, 678, 466, 3);
    host_geometry_debounce_note(&debounce, 0.06, 669, 951, 1);
    assert(!host_geometry_debounce_settled(&debounce, 0.159, &geometry));
    assert(host_geometry_debounce_settled(&debounce, 0.16, &geometry));
    assert(geometry.width == 669 && geometry.height == 951 && geometry.orientation == 1);
    assert(!host_geometry_debounce_settled(&debounce, 2, &geometry));
    host_geometry_debounce_note(&debounce, 2, 669, 951, 1);
    assert(!host_geometry_debounce_settled(&debounce, 3, &geometry));
    host_geometry_debounce_note(&debounce, 3, 669, 951, 2);
    host_geometry_debounce_note(&debounce, 3.09, 669, 951, 2);
    assert(host_geometry_debounce_settled(&debounce, 3.101, &geometry));
    assert(geometry.orientation == 2);
    host_geometry_debounce_note(&debounce, 4, 0, 951, 2);
    assert(!host_geometry_debounce_settled(&debounce, 5, &geometry));
}

int main(void)
{
    check_posture();
    check_debounce();
    puts("PASS: posture thresholds, hysteresis, gravity, upper-half geometry, closed posture, and geometry debounce");
    return 0;
}
