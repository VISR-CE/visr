/* Apache-2.0. Foreground publication and the worker's predicate check share a
   mutex so a notification cannot fall between the check and the wait. */
#ifndef HOST_LIFECYCLE_WAIT_H
#define HOST_LIFECYCLE_WAIT_H
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

struct host_lifecycle_wait {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
};
#define HOST_LIFECYCLE_WAIT_INIT { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER }

static inline void host_lifecycle_wait_set(struct host_lifecycle_wait *wait, _Atomic int *backgrounded, int value)
{
    pthread_mutex_lock(&wait->mutex);
    atomic_store(backgrounded, value);
    if (!value) pthread_cond_broadcast(&wait->condition);
    pthread_mutex_unlock(&wait->mutex);
}

/* Only the worker calls this. Timeout also rechecks a clear from a future
   caller that forgets to signal. Spurious wakes must not release the hold. */
static inline void host_lifecycle_wait_until_foreground(struct host_lifecycle_wait *wait, _Atomic int *backgrounded)
{
    pthread_mutex_lock(&wait->mutex);
    while (atomic_load(backgrounded)) {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_nsec += 100000000;
        if (deadline.tv_nsec >= 1000000000) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000;
        }
        pthread_cond_timedwait(&wait->condition, &wait->mutex, &deadline);
    }
    pthread_mutex_unlock(&wait->mutex);
}
#endif
