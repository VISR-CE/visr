#include "host_modal_wait.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>

static atomic_int finished;
static void timeout_handler(int value) { (void)value; _exit(1); }
static void *worker(void *context) {
    (void)context;
    __block int result = 0, ticks = 0;
    host_modal_wait(0.01, ^bool {
        assert(host_on_main_thread());
        if (++ticks < 3) return false;
        result = 42;
        return true;
    });
    assert(result == 42 && ticks == 3);
    /* Completion before the wait starts must also be retained. */
    host_modal_wait(0.01, ^bool { assert(host_on_main_thread()); return true; });
    atomic_store(&finished, 1);
    return NULL;
}
int main(void) {
    signal(SIGALRM, timeout_handler); alarm(10);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, worker, NULL) == 0);
    while (!atomic_load(&finished)) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    assert(pthread_join(thread, NULL) == 0);
    alarm(0);
    puts("PASS: modal worker waits while main polls and delivers its result");
}
