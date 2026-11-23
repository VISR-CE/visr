#include "host_main_thread.h"
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

static _Thread_local unsigned wait_forbidden;
static atomic_ulong forbidden_waits;

void host_main_wait_forbid(void) { wait_forbidden++; }
void host_main_wait_allow(void) { if (wait_forbidden) wait_forbidden--; }
unsigned long host_main_forbidden_waits(void) { return atomic_load(&forbidden_waits); }
/* (a wait from main runs inline and is not one) */
static void count_wait(void) {
    if (wait_forbidden) atomic_fetch_add(&forbidden_waits, 1);
}

/* the calling thread's waits on main since its last host_main_sync_take (debug.main_sync_log: how long the game
thread blocks on main inside a frame) */
static _Thread_local struct host_main_sync_waits thread_waits;

static uint64_t uptime_nanoseconds(void) {
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}
static void note_wait(uint64_t started) {
    uint64_t waited = uptime_nanoseconds() - started;

    thread_waits.count++;
    thread_waits.nanoseconds += waited;
    if (waited > thread_waits.longest_nanoseconds) thread_waits.longest_nanoseconds = waited;
}
void host_main_sync_take(struct host_main_sync_waits *waits) {
    *waits = thread_waits;
    thread_waits = (struct host_main_sync_waits){ 0 };
}

int host_on_main_thread(void) {
    return pthread_main_np();
}
void host_main_async(dispatch_block_t block) {
    if (host_on_main_thread()) block();
    else dispatch_async(dispatch_get_main_queue(), block);
}
void host_main_sync(dispatch_block_t block) {
    if (host_on_main_thread()) block();
    else {
        uint64_t started = uptime_nanoseconds();

        count_wait();
        dispatch_sync(dispatch_get_main_queue(), block);
        note_wait(started);
    }
}
void host_main_async_f(void *context, void (*fn)(void *)) {
    if (host_on_main_thread()) fn(context);
    else dispatch_async_f(dispatch_get_main_queue(), context, fn);
}
void host_main_sync_f(void *context, void (*fn)(void *)) {
    if (host_on_main_thread()) fn(context);
    else {
        uint64_t started = uptime_nanoseconds();

        count_wait();
        dispatch_sync_f(dispatch_get_main_queue(), context, fn);
        note_wait(started);
    }
}
