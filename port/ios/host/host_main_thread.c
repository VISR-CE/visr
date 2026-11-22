#include "host_main_thread.h"
#include <pthread.h>
#include <stdatomic.h>

static _Thread_local unsigned wait_forbidden;
static atomic_ulong forbidden_waits;

void host_main_wait_forbid(void) { wait_forbidden++; }
void host_main_wait_allow(void) { if (wait_forbidden) wait_forbidden--; }
unsigned long host_main_forbidden_waits(void) { return atomic_load(&forbidden_waits); }
/* (a wait from main runs inline and is not one) */
static void count_wait(void) {
    if (wait_forbidden) atomic_fetch_add(&forbidden_waits, 1);
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
    else { count_wait(); dispatch_sync(dispatch_get_main_queue(), block); }
}
void host_main_async_f(void *context, void (*fn)(void *)) {
    if (host_on_main_thread()) fn(context);
    else dispatch_async_f(dispatch_get_main_queue(), context, fn);
}
void host_main_sync_f(void *context, void (*fn)(void *)) {
    if (host_on_main_thread()) fn(context);
    else { count_wait(); dispatch_sync_f(dispatch_get_main_queue(), context, fn); }
}
