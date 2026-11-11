/* A touch press survives until every reader in one guest input frame sees it. */
#ifndef HALO_HOST_TOUCH_LATCH_H
#define HALO_HOST_TOUCH_LATCH_H
#include <stdatomic.h>

struct host_touch_latch { atomic_int state; };
enum { host_touch_pending = 1, host_touch_seen = 2 };

static inline void host_touch_latch_press(struct host_touch_latch *latch) {
    atomic_store(&latch->state, host_touch_pending);
}
static inline int host_touch_latch_read(struct host_touch_latch *latch) {
    int expected = host_touch_pending;
    atomic_compare_exchange_strong(&latch->state, &expected, host_touch_seen);
    return expected != 0;
}
/* Polling can run several times before input is read. Pending presses stay. */
static inline void host_touch_latch_frame(struct host_touch_latch *latch) {
    int expected = host_touch_seen;
    atomic_compare_exchange_strong(&latch->state, &expected, 0);
}
#endif
