/* Exercise the production wait and hold routing without UIKit or an app. */
#include "test_wait.h"
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
    assert(TEST_WAIT_UNTIL(atomic_load(&waits) >= count));
}
/* the work a hold does before it waits (the encode queue's drain): counted, with the background flag it saw */
static _Atomic unsigned befores, befores_in_background;
static _Atomic int before_had_lock;
static void before_waiting(void)
{
    if (!pthread_mutex_trylock(&foreground_wait.mutex)) pthread_mutex_unlock(&foreground_wait.mutex);
    else atomic_store(&before_had_lock, 1);
    atomic_fetch_add(&befores, 1);
    if (atomic_load(&backgrounded)) atomic_fetch_add(&befores_in_background, 1);
}
static void *hold_with_drain(void *unused)
{
    (void)unused;
    host_lifecycle_hold(before_waiting);
    atomic_fetch_add(&returned, 1);
    return NULL;
}
static void *hold(void *unused)
{
    (void)unused;
    host_lifecycle_hold(NULL);
    atomic_fetch_add(&returned, 1);
    return NULL;
}
int main(void)
{
    alarm(TEST_WAIT_WATCHDOG_SECONDS);
    /* Clear before entry must return immediately, including off main. */
    host_lifecycle_hold(NULL);
    assert(!spins && !atomic_load(&waits));
    for (int flag = 0; flag <= 1; flag++) {
        threaded = flag;
        on_main = 1;
        atomic_store(&backgrounded, 1);
        host_lifecycle_hold(NULL);
        assert(!atomic_load(&backgrounded));
    }
    assert(spins == 2 && !atomic_load(&waits));
    /* Flag off preserves the original calling-thread run-loop path. */
    threaded = 0; on_main = 0;
    atomic_store(&backgrounded, 1);
    host_lifecycle_hold(NULL);
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
    /* The work before the wait runs once a hold that waits, in the background, and never for one that does not. */
    threaded = 1;
    host_lifecycle_hold(before_waiting);
    assert(!atomic_load(&befores));
    for (unsigned i = 0; i < 20; i++) {
        atomic_store(&returned, 0);
        host_lifecycle_wait_set(&foreground_wait, &backgrounded, 1);
        unsigned began = atomic_load(&waits);
        assert(!pthread_create(&worker, NULL, hold_with_drain, NULL));
        wait_for(began + 1);
        host_lifecycle_wait_set(&foreground_wait, &backgrounded, 0);
        assert(!pthread_join(worker, NULL));
    }
    assert(atomic_load(&befores) == 20 && atomic_load(&befores_in_background) == 20);
    /* The background arrives while the hold is blocked on the lock (after any check made before it): the work still
       comes before the wait, and the main thread's lock is never held across it. */
    for (unsigned i = 0; i < 20; i++) {
        unsigned before = atomic_load(&befores);
        pthread_mutex_lock(&foreground_wait.mutex);
        unsigned began = atomic_load(&waits);
        assert(!pthread_create(&worker, NULL, hold_with_drain, NULL));
        pause_briefly();
        atomic_store(&backgrounded, 1);
        pthread_mutex_unlock(&foreground_wait.mutex);
        wait_for(began + 1);
        assert(atomic_load(&befores) == before + 1);
        host_lifecycle_wait_set(&foreground_wait, &backgrounded, 0);
        assert(!pthread_join(worker, NULL));
    }
    /* before_waiting runs without the lock: the main thread's notification can take it meanwhile */
    assert(!atomic_load(&before_had_lock));
    assert(!pthread_mutex_trylock(&foreground_wait.mutex));
    pthread_mutex_unlock(&foreground_wait.mutex);
    puts("PASS: lifecycle routing, foreground wakes, spurious wakes, repeated holds, timed fallback, work before the wait");
    return 0;
}
