#ifndef HOST_POSTURE_H
#define HOST_POSTURE_H

#include "host_hinge.h"
#include <math.h>

#ifndef HOST_POSTURE_L_ENTER_ANGLE
#define HOST_POSTURE_L_ENTER_ANGLE 2.6
#endif
#ifndef HOST_POSTURE_L_EXIT_ANGLE
#define HOST_POSTURE_L_EXIT_ANGLE 2.8
#endif

enum host_posture {
    HOST_POSTURE_FLAT,
    HOST_POSTURE_LAPTOP_L,
    HOST_POSTURE_BOOK_L,
    HOST_POSTURE_CLOSED
};

struct host_posture_rect { double x, y, width, height; };
struct host_posture_inputs {
    struct host_hinge_snapshot hinge;
    double view_width, view_height;
    /* Gravity points down: x right, y up the screen, z out of the screen. */
    int gravity_known;
    double gravity_x, gravity_y, gravity_z;
};
struct host_posture_state {
    enum host_posture posture;
    /* Root-view points (origin at top left); zero outside LAPTOP_L. */
    struct host_posture_rect upper_half;
};

static inline enum host_posture host_posture_update(struct host_posture_state *state,
    const struct host_posture_inputs *in)
{
    enum host_posture next = HOST_POSTURE_FLAT;
    const struct host_hinge_snapshot *hinge = &in->hinge;
    struct host_posture_rect upper = {0};
    if (hinge->available && hinge->status == HOST_HINGE_CLOSED) {
        next = HOST_POSTURE_CLOSED;
    } else if (hinge->available && hinge->status == HOST_HINGE_PARTIALLY_OPEN &&
        hinge->division_active && isfinite(in->view_width) && isfinite(in->view_height) &&
        in->view_width > 0 && in->view_height > 0 &&
        isfinite(hinge->division_x) && isfinite(hinge->division_y) &&
        isfinite(hinge->division_width) && isfinite(hinge->division_height) &&
        hinge->division_width > 0 && hinge->division_height > 0 &&
        hinge->division_x < in->view_width && hinge->division_y < in->view_height &&
        hinge->division_x + hinge->division_width > 0 &&
        hinge->division_y + hinge->division_height > 0) {
        if (hinge->division_height > 4 * hinge->division_width) {
            next = HOST_POSTURE_BOOK_L;
        } else if (hinge->division_width > 4 * hinge->division_height &&
            hinge->division_y > 0 && hinge->division_y + hinge->division_height < in->view_height &&
            isfinite(hinge->angle) && hinge->angle >= 0 &&
            hinge->angle < (state->posture == HOST_POSTURE_LAPTOP_L ?
                HOST_POSTURE_L_EXIT_ANGLE : HOST_POSTURE_L_ENTER_ANGLE)) {
            double x = in->gravity_x, y = in->gravity_y, z = in->gravity_z;
            double length = sqrt(x*x + y*y + z*z);
            int known = in->gravity_known && isfinite(length) && length > 0.1;
            /* pi is flat open. Rotate the upper half's outward normal toward
               screen +y by the closing angle to estimate the lower normal.
               A positive upward projection distinguishes laptop from inverted L. */
            double bend = 3.141592653589793 - hinge->angle;
            double lower_up = -(y * sin(bend) + z * cos(bend));
            if (!known || -y >= 0.7 * length ||
                (lower_up >= 0.7 * length && lower_up > -z + 0.1 * length)) {
                next = HOST_POSTURE_LAPTOP_L;
                upper.width = in->view_width;
                upper.height = hinge->division_y;
            }
        }
    }
    state->posture = next;
    state->upper_half = upper;
    return next;
}

#ifdef __cplusplus
extern "C" {
#endif
struct host_posture_snapshot {
    enum host_posture posture;
    unsigned generation;
    struct host_posture_rect upper_half;
};
/* Attach and poll on the main thread after host_hinge_attach/poll. */
void host_posture_attach(void *root_view);
void host_posture_poll(void);
/* A coherent copy, safe to read from any thread; unsupported hosts stay FLAT. */
void host_posture_snapshot(struct host_posture_snapshot *out);
#ifdef __cplusplus
}
#endif
#endif
