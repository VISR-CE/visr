#ifndef HOST_GEOMETRY_DEBOUNCE_H
#define HOST_GEOMETRY_DEBOUNCE_H
#include <math.h>

#ifndef HOST_GEOMETRY_SETTLE_SECONDS
#define HOST_GEOMETRY_SETTLE_SECONDS 0.1
#endif

struct host_geometry {
    double width, height;
    int orientation;
};
struct host_geometry_debounce {
    struct host_geometry latest;
    double changed_at;
    int noted, pending;
};

static inline void host_geometry_debounce_note(struct host_geometry_debounce *d,
    double now_seconds, double width, double height, int orientation)
{
    if (!isfinite(now_seconds) || !isfinite(width) || !isfinite(height) || width <= 0 || height <= 0)
        return;
    if (d->noted && d->latest.width == width && d->latest.height == height &&
        d->latest.orientation == orientation) return;
    d->latest = (struct host_geometry){width, height, orientation};
    d->changed_at = now_seconds;
    d->noted = d->pending = 1;
}

static inline int host_geometry_debounce_settled(struct host_geometry_debounce *d,
    double now_seconds, struct host_geometry *out)
{
    if (!d->pending || !isfinite(now_seconds) || now_seconds - d->changed_at < HOST_GEOMETRY_SETTLE_SECONDS)
        return 0;
    if (out) *out = d->latest;
    d->pending = 0;
    return 1;
}
#endif
