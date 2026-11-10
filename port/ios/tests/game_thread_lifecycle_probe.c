/* Exercise the production wait and hold routing without UIKit or an app. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static _Atomic unsigned waits;
static int observed_wait(pthread_cond_t *condition, pthread_mutex_t *mutex, const struct timespec *deadline)
{
    atomic_fetch_add(&waits, 1);
    return pthread_cond_timedwait(condition, mutex, deadline);
}
#define pthread_cond_timedwait observed_wait
#include "host_lifecycle_wait.h"
#undef pthread_cond_timedwait

static struct host_lifecycle_wait foreground_wait = HOST_LIFECYCLE_WAIT_INIT;
static _Atomic int backgrounded, returned;
static int threaded = 1;
static _Thread_local int on_main;
static unsigned spins;
static int host_game_thread_enabled(void) { return threaded; }
static int host_on_main_thread(void) { return on_main; }
#define kCFRunLoopDefaultMode 0
#define true 1
static void CFRunLoopRunInMode(int mode, double seconds, int once)
{
    assert(mode == 0 && seconds == 0.1 && once == 1);
    spins++;
    atomic_store(&backgrounded, 0);
}
#include "game_thread_lifecycle.inc"

static void pause_briefly(void)
{
    struct timespec delay = {0, 1000000};
    nanosleep(&delay, NULL);
}
static void wait_for(unsigned count)
{
    for (unsigned tries = 0; atomic_load(&waits) < count; tries++) {
        assert(tries < 2000);
        pause_briefly();
    }
}
static void *hold(void *unused)
{
    (void)unused;
    host_lifecycle_hold();
    atomic_fetch_add(&returned, 1);
    return NULL;
}
int main(void)
{
    alarm(20);
    /* Clear before entry must return immediately, including off main. */
    host_lifecycle_hold();
    assert(!spins && !atomic_load(&waits));
    for (int flag = 0; flag <= 1; flag++) {
        threaded = flag;
        on_main = 1;
        atomic_store(&backgrounded, 1);
        host_lifecycle_hold();
        assert(!atomic_load(&backgrounded));
    }
    assert(spins == 2 && !atomic_load(&waits));
    /* Flag off preserves the original calling-thread run-loop path. */
    threaded = 0; on_main = 0;
    atomic_store(&backgrounded, 1);
    host_lifecycle_hold();
    assert(spins == 3 && !atomic_load(&waits));
    threaded = 1;

    /* Multiple waiters, spurious broadcasts, and repeated background cycles. */
    for (unsigned cycle = 0; cycle < 100; cycle++) {
        pthread_t workers[2];
        atomic_store(&returned, 0);
        host_lifecycle_wait_set(&foreground_wait, &backgrounded, 1);
        unsigned began = atomic_load(&waits);
        for (unsigned i = 0; i < 2; i++) assert(!pthread_create(&workers[i], NULL, hold, NULL));
        wait_for(began + 2);
        pthread_mutex_lock(&foreground_wait.mutex);
        unsigned before = atomic_load(&waits);
        pthread_cond_broadcast(&foreground_wait.condition);
        pthread_mutex_unlock(&foreground_wait.mutex);
        wait_for(before + 2);
        assert(!atomic_load(&returned));
        host_lifecycle_wait_set(&foreground_wait, &backgrounded, 0);
        for (unsigned i = 0; i < 2; i++) assert(!pthread_join(workers[i], NULL));
        assert(atomic_load(&returned) == 2);
    }
    /* A missed signal still releases the worker at a timed recheck. */
    pthread_t worker;
    atomic_store(&returned, 0);
    host_lifecycle_wait_set(&foreground_wait, &backgrounded, 1);
    unsigned began = atomic_load(&waits);
    assert(!pthread_create(&worker, NULL, hold, NULL));
    wait_for(began + 2);
    atomic_store(&backgrounded, 0);
    assert(!pthread_join(worker, NULL));
    assert(atomic_load(&returned) == 1 && spins == 3);
    /* A foreground notification before the wait starts cannot be lost. */
    for (unsigned i = 0; i < 100; i++) {
        host_lifecycle_wait_set(&foreground_wait, &backgrounded, 1);
        assert(!pthread_create(&worker, NULL, hold, NULL));
        host_lifecycle_wait_set(&foreground_wait, &backgrounded, 0);
        assert(!pthread_join(worker, NULL));
    }
    assert(!pthread_mutex_trylock(&foreground_wait.mutex));
    pthread_mutex_unlock(&foreground_wait.mutex);
    puts("PASS: lifecycle routing, foreground wakes, spurious wakes, repeated holds, timed fallback");
    return 0;
}
