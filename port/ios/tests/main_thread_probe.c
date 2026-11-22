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
static void nothing(void *context) { (void)context; }
static void sleep_briefly(void *context) {
    (void)context;
    usleep(5000);
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
    /* the two sync calls blocked this thread on main, and each wait is counted with its time (debug.main_sync_log) */
    struct host_main_sync_waits waits;
    host_main_sync_take(&waits);
    assert(waits.count == 2);
    /* both entry points time their wait, and the longest is kept beside the total */
    host_main_sync(^{ usleep(5000); });
    host_main_sync_take(&waits);
    assert(waits.count == 1);
    assert(waits.nanoseconds >= 5000000 && waits.longest_nanoseconds == waits.nanoseconds);
    host_main_sync_f(NULL, sleep_briefly);
    host_main_sync(^{ usleep(20000); });
    host_main_sync_take(&waits);
    assert(waits.count == 2);
    assert(waits.nanoseconds >= 25000000);
    assert(waits.longest_nanoseconds >= 20000000 && waits.longest_nanoseconds < waits.nanoseconds);
    host_main_sync_take(&waits);
    assert(waits.count == 0 && waits.nanoseconds == 0);
    /* main is waiting in its run loop now: none of this thread's waits may land in main's totals */
    host_main_sync(^{ usleep(1000); });
    /* Waits made outside a no-wait zone are not counted; inside one (nested too) every wait is,
       and asyncs are not. */
    assert(host_main_forbidden_waits() == 0);
    host_main_wait_forbid();
    host_main_async(^{});
    assert(host_main_forbidden_waits() == 0);
    host_main_sync(^{});
    assert(host_main_forbidden_waits() == 1);
    host_main_wait_forbid();
    host_main_sync_f(NULL, nothing);
    host_main_wait_allow();
    assert(host_main_forbidden_waits() == 2);
    host_main_sync(^{});
    host_main_wait_allow();
    assert(host_main_forbidden_waits() == 3);
    host_main_sync(^{});
    host_main_sync_f(NULL, nothing);
    assert(host_main_forbidden_waits() == 3);
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
    /* main runs its own calls inline, so a zone on main counts nothing */
    host_main_wait_forbid();
    host_main_sync(^{});
    host_main_sync_f(NULL, nothing);
    host_main_wait_allow();
    assert(host_main_forbidden_waits() == 0);
    /* calls made on main run inline: nothing waited */
    struct host_main_sync_waits main_waits;
    host_main_sync_take(&main_waits);
    assert(main_waits.count == 0 && main_waits.nanoseconds == 0);

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
    /* the worker's waits were counted on the worker: main's own totals stay empty */
    host_main_sync_take(&main_waits);
    assert(main_waits.count == 0 && main_waits.nanoseconds == 0);
    assert(atomic_load(&step) == 4);
    assert(host_main_forbidden_waits() == 3);
    dispatch_release(submitted);
    alarm(0);
    puts("PASS: main-thread calls run inline in order; worker calls dispatch to main and sync waits, each counted with its time; waits inside a no-wait zone are counted");
    return 0;
}
