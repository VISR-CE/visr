/* SPDX-License-Identifier: Apache-2.0 */
#include "touch_math.h"
#include <math.h>

static double clamp(double value, double low, double high) {
    return fmax(low, fmin(high, value));
}

touch_point touch_stick(double x, double y, double radius, double dead_zone, double exponent) {
    double length = hypot(x, y);
    if (radius <= 0 || length <= radius * dead_zone || length == 0)
        return (touch_point){0, 0};
    double magnitude = pow(clamp((length / radius - dead_zone) /
                                (TOUCH_STICK_SATURATION - dead_zone), 0, 1), exponent);
    return (touch_point){x / length * magnitude, y / length * magnitude};
}

int touch_hit_test(touch_point point, const touch_target *targets, size_t count) {
    int nearest = -1;
    double distance = INFINITY;
    for (size_t i = 0; i < count; ++i) {
        touch_rect r = targets[i].rect;
        double padding = targets[i].padding;
        if (point.x < r.x - padding || point.x > r.x + r.width + padding ||
            point.y < r.y - padding || point.y > r.y + r.height + padding) continue;
        double dx = point.x - (r.x + r.width / 2), dy = point.y - (r.y + r.height / 2);
        double squared = dx * dx + dy * dy;
        if (squared < distance) { nearest = (int)i; distance = squared; }
    }
    return nearest;
}

double touch_scale(double width, double height) {
    return clamp(fmin(width / 760, height / 360), .70, 1.40);
}

touch_rect touch_move_zone(touch_rect screen, touch_rect safe_area) {
    double x = fmax(screen.x, safe_area.x);
    double y = fmax(screen.y + screen.height * .28, safe_area.y);
    double right = fmin(screen.x + screen.width * .42, safe_area.x + safe_area.width);
    double bottom = fmin(screen.y + screen.height, safe_area.y + safe_area.height);
    return (touch_rect){x, y, fmax(0, right - x), fmax(0, bottom - y)};
}

touch_point touch_floating_base(touch_point point, touch_rect zone, double radius) {
    double inset_x = fmin(radius, zone.width / 2), inset_y = fmin(radius, zone.height / 2);
    return (touch_point){clamp(point.x, zone.x + inset_x, zone.x + zone.width - inset_x),
                         clamp(point.y, zone.y + inset_y, zone.y + zone.height - inset_y)};
}
