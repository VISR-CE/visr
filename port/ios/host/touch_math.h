/* SPDX-License-Identifier: Apache-2.0 */
#ifndef HALO_TOUCH_MATH_H
#define HALO_TOUCH_MATH_H

#include <stddef.h>

typedef struct { double x, y; } touch_point;
typedef struct { double x, y, width, height; } touch_rect;
typedef struct { touch_rect rect; double padding; } touch_target;

/* Dead zone and saturation are fractions of the visible stick radius. */
#define TOUCH_STICK_DEAD_ZONE 0.10
#define TOUCH_STICK_EXPONENT 1.5
#define TOUCH_STICK_SATURATION 0.90

touch_point touch_stick(double x, double y, double radius, double dead_zone, double exponent);
/* Equal distances select the first target; padding is in screen points. */
int touch_hit_test(touch_point point, const touch_target *targets, size_t count);
double touch_scale(double width, double height);
touch_rect touch_move_zone(touch_rect screen, touch_rect safe_area);
/* Keep the whole base inside the zone; center it if the base cannot fit. */
touch_point touch_floating_base(touch_point point, touch_rect zone, double radius);

#endif
