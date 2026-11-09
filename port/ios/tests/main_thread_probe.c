#include "host_main_thread.h"
#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>

static atomic_int step, done;
static dispatch_semaphore_t submitted;

static void timed_out(int signal_number) {
    (void)signal_number;
    _exit(1);
}
static void advance(void *context) {
    int expected = *(int *)context;
    assert(host_on_main_thread());
    assert(atomic_fetch_add(&step, 1) == expected);
}
static void *worker(void *context) {
    (void)context;
    assert(!host_on_main_thread());
    host_main_async(^{ int expected = 0; advance(&expected); });
    dispatch_semaphore_signal(submitted);
    host_main_sync(^{ int expected = 1; advance(&expected); });
    assert(atomic_load(&step) == 2);
    static int async_expected = 2;
    host_main_async_f(&async_expected, advance);
    int sync_expected = 3;
    host_main_sync_f(&sync_expected, advance);
    assert(atomic_load(&step) == 4);
    atomic_store(&done, 1);
    return NULL;
}
int main(void) {
    /* Also bounds dispatch_sync and pthread_join if either deadlocks. */
    signal(SIGALRM, timed_out);
    alarm(10);
    assert(host_on_main_thread());
    host_main_sync(^{ int expected = 0; advance(&expected); });
    assert(atomic_load(&step) == 1);
    host_main_async(^{ int expected = 1; advance(&expected); });
    assert(atomic_load(&step) == 2);
    int expected = 2;
    host_main_sync_f(&expected, advance);
    assert(atomic_load(&step) == 3);
    expected = 3;
    host_main_async_f(&expected, advance);
    assert(atomic_load(&step) == 4);

    atomic_store(&step, 0);
    submitted = dispatch_semaphore_create(0);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, worker, NULL) == 0);
    assert(dispatch_semaphore_wait(submitted, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC)) == 0);
    /* The worker's async call cannot run until main services its queue. */
    assert(atomic_load(&step) == 0);
    CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + 5;
    while (!atomic_load(&done) && CFAbsoluteTimeGetCurrent() < deadline)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
    assert(atomic_load(&done));
    assert(pthread_join(thread, NULL) == 0);
    assert(atomic_load(&step) == 4);
    dispatch_release(submitted);
    alarm(0);
    puts("PASS: main-thread calls run inline in order; worker calls dispatch to main and sync waits");
    return 0;
}
