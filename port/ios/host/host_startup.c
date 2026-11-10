#include "host_startup.h"
#include "host_main_thread.h"
#include <stdatomic.h>

static struct host_startup_info captured;
static atomic_int ready;
static _Thread_local struct host_startup_info fallback;

static void read_live(void *context) {
    if (atomic_load_explicit(&ready, memory_order_acquire))
        *(struct host_startup_info *)context = captured;
    else
        host_startup_read_live(context);
}
static void capture_on_main(void *unused) {
    (void)unused;
    if (atomic_load_explicit(&ready, memory_order_acquire)) return;
    host_startup_read_live(&captured);
    atomic_store_explicit(&ready, 1, memory_order_release);
}
void host_startup_capture(void) {
    if (atomic_load_explicit(&ready, memory_order_acquire)) return;
    host_main_sync_f(NULL, capture_on_main);
}
const struct host_startup_info *host_startup_get(void) {
    if (atomic_load_explicit(&ready, memory_order_acquire)) return &captured;
    host_main_sync_f(&fallback, read_live);
    return &fallback;
}
