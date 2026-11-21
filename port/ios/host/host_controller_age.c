#include "host_controller_age.h"
#include "host.h"
#include "host_config.h"
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_timer.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

#define BUCKET_NS 250000ull
#define BUCKETS 80 /* 20 ms; older reads share the last bucket */
#define WINDOW_NS 5000000000ull

static _Atomic uint64_t last_update_ns;
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned histogram[BUCKETS];
static unsigned long long count, sum_ns, max_ns, window_start_ns;
static int log_enabled = -1;

void host_controller_refreshed(void) {
    atomic_store_explicit(&last_update_ns, SDL_GetTicksNS(), memory_order_release);
}

void host_controller_refresh(void) {
    SDL_UpdateJoysticks();
    host_controller_refreshed();
}

uint64_t host_controller_age_ns(void) {
    uint64_t last = atomic_load_explicit(&last_update_ns, memory_order_acquire);
    if (!last) return UINT64_MAX;
    uint64_t now = SDL_GetTicksNS();
    return now > last ? now - last : 0;
}

static unsigned long long percentile_ns(double fraction) {
    unsigned long long wanted = (unsigned long long)(fraction * (double)count), seen = 0;
    if (!wanted) wanted = 1;
    for (int index = 0; index < BUCKETS; index++) {
        seen += histogram[index];
        if (seen >= wanted) return (unsigned long long)(index + 1) * BUCKET_NS;
    }
    return max_ns;
}

void host_controller_age_sample(void) {
    if (log_enabled < 0) {
        char value[16];
        host_config_string("debug.controller_age_log", "false", value, sizeof(value));
        log_enabled = strcmp(value, "true") == 0;
    }
    if (!log_enabled) return;
    uint64_t age = host_controller_age_ns();
    if (age == UINT64_MAX) return;
    uint64_t now = SDL_GetTicksNS();
    pthread_mutex_lock(&stats_lock);
    if (!window_start_ns) window_start_ns = now;
    unsigned long long bucket = age / BUCKET_NS;
    histogram[bucket >= BUCKETS ? BUCKETS - 1 : bucket]++;
    count++;
    sum_ns += age;
    if (age > max_ns) max_ns = age;
    if (now - window_start_ns >= WINDOW_NS) {
        host_logf(HOST_LOG_INFO, "controller age: reads %llu mean %.2f ms p50 %.2f p99 %.2f max %.2f",
            count, (double)sum_ns / (double)count / 1e6, (double)percentile_ns(0.5) / 1e6,
            (double)percentile_ns(0.99) / 1e6, (double)max_ns / 1e6);
        memset(histogram, 0, sizeof(histogram));
        count = sum_ns = max_ns = 0;
        window_start_ns = now;
    }
    pthread_mutex_unlock(&stats_lock);
}
