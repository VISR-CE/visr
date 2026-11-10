#ifndef HOST_POSTURE_LAYOUT_H
#define HOST_POSTURE_LAYOUT_H
#include "host_posture.h"

struct host_posture_insets { double top, left, bottom, right; };
struct host_posture_layout {
    int laptop;
    enum host_posture posture;
    unsigned generation;
    double scale;
    struct host_posture_rect upper_half, flat_half, backdrop, pause_button;
    struct host_posture_insets safe_area;
    struct host_hinge_snapshot hinge;
};

/* All rectangles use root-view points. The backdrop covers the reserved fold;
   interactive content starts below its lower edge (including system margins). */
static inline struct host_posture_layout host_posture_layout_make(int enabled,
    struct host_posture_snapshot posture, struct host_hinge_snapshot hinge,
    double width, double height, struct host_posture_insets safe, double scale)
{
    struct host_posture_layout out = {0};
    out.generation = posture.generation;
    out.posture = posture.posture;
    out.safe_area = safe;
    out.scale = scale;
    out.hinge = hinge;
    if (!isfinite(width) || !isfinite(height) || width <= 0 || height <= 0) return out;
    out.upper_half = (struct host_posture_rect){0, 0, width, height};
    double upper = posture.upper_half.height;
    double lower = hinge.division_y + hinge.division_height;
    if (!enabled || posture.posture != HOST_POSTURE_LAPTOP_L || !hinge.division_active ||
        !isfinite(upper) || !isfinite(lower) || upper <= 0 || lower < upper || lower >= height)
        return out;
    out.laptop = 1;
    out.upper_half.height = upper;
    out.flat_half = (struct host_posture_rect){0, lower, width, height - lower};
    out.backdrop = (struct host_posture_rect){0, upper, width, height - upper};
    double left = fmax(0, safe.left), right = width - fmax(0, safe.right);
    double top = fmax(lower, safe.top), bottom = height - fmax(0, safe.bottom);
    /* Suppress the button if a transient geometry cannot fit a 44 pt target. */
    if (right - left >= 44 && bottom - top >= 44)
        out.pause_button = (struct host_posture_rect){(left + right - 44) / 2,
            (top + bottom - 44) / 2, 44, 44};
    return out;
}
/* Hinge angle samples have their own generation. They do not cancel a tap or
   drain the renderer unless the published layout actually changes. */
static inline int host_posture_layout_changed(const struct host_posture_layout *a,
    const struct host_posture_layout *b)
{
#define RECT_CHANGED(field) (a->field.x != b->field.x || a->field.y != b->field.y || \
    a->field.width != b->field.width || a->field.height != b->field.height)
    return a->laptop != b->laptop || a->posture != b->posture || a->scale != b->scale ||
        RECT_CHANGED(upper_half) || RECT_CHANGED(flat_half) || RECT_CHANGED(backdrop) ||
        RECT_CHANGED(pause_button) || a->safe_area.top != b->safe_area.top ||
        a->safe_area.left != b->safe_area.left || a->safe_area.bottom != b->safe_area.bottom ||
        a->safe_area.right != b->safe_area.right;
#undef RECT_CHANGED
}
#ifdef __cplusplus
extern "C" {
#endif
/* Coherent hand-off; generation advances on geometry, safe area or posture
   changes. Consumers cancel held touches whenever the generation changes. */
void host_posture_layout_snapshot(struct host_posture_layout *out);
/* Main-thread refresh of the existing touch overlay after posture polling. */
void host_ios_touch_posture_changed(void);
#ifdef __cplusplus
}
#endif
#endif
