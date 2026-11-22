/* Probes that run threads beside main's work, for the thread sanitizer: proof that every thread really ran during
   main's work, however the threads are scheduled. Each thread adds one to its own counter after each pass of its loop.
   Main calls thread_passes_start once every thread is created, which waits for each thread's first pass, then keeps
   working while thread_passes_continue says so (its minimum count of steps, and then until every thread has made two
   more passes; two, because the first may have finished before main's first step and only counted after the
   snapshot), and calls thread_passes_finish after joining. A thread that never runs fails the probe after
   THREAD_PASSES_DEADLINE_SECONDS with each thread's count; it cannot read as a pass or hang the job. */
#ifndef THREAD_PASSES_H
#define THREAD_PASSES_H

#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { THREAD_PASSES_MAX = 8, THREAD_PASSES_DEADLINE_SECONDS = 30, THREAD_PASSES_DURING = 2 };

struct thread_passes {
    int threads;
    atomic_long passes[THREAD_PASSES_MAX];   /* thread i's context is &passes[i] */
    long since[THREAD_PASSES_MAX];           /* each count when main's work began */
    double deadline;
};

static inline double thread_passes_now(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

static inline void thread_passes_fail(struct thread_passes *passes, const char *what) {
    fprintf(stderr, "FAIL: %s; passes per thread:", what);
    for (int i = 0; i < passes->threads; i++)
        fprintf(stderr, " %ld (%ld when main's work began)", atomic_load(&passes->passes[i]), passes->since[i]);
    fprintf(stderr, "\n");
    exit(1);
}

/* true once every thread has made at least `more` passes since its `since` count */
static inline int thread_passes_all_made(struct thread_passes *passes, long more) {
    for (int i = 0; i < passes->threads; i++)
        if (atomic_load(&passes->passes[i]) < passes->since[i] + more) return 0;
    return 1;
}

static inline void thread_passes_check_deadline(struct thread_passes *passes, const char *what) {
    if (thread_passes_now() >= passes->deadline) thread_passes_fail(passes, what);
}

/* Waits for every thread's first pass, then takes the snapshot main's work is measured from. */
static inline void thread_passes_start(struct thread_passes *passes, int threads) {
    if (threads < 1 || threads > THREAD_PASSES_MAX) {
        fprintf(stderr, "FAIL: thread_passes_start: %d threads (1 to %d)\n", threads, THREAD_PASSES_MAX);
        exit(1);
    }
    passes->threads = threads;
    for (int i = 0; i < threads; i++) passes->since[i] = 0;
    passes->deadline = thread_passes_now() + THREAD_PASSES_DEADLINE_SECONDS;
    while (!thread_passes_all_made(passes, 1)) {
        thread_passes_check_deadline(passes, "a thread made no first pass before main's work, within the deadline");
        sched_yield();
    }
    for (int i = 0; i < threads; i++) passes->since[i] = atomic_load(&passes->passes[i]);
    passes->deadline = thread_passes_now() + THREAD_PASSES_DEADLINE_SECONDS;
}

/* Whether main should do step `step` (counting from 0): always below `minimum`, then until every thread has made
   THREAD_PASSES_DURING passes since the snapshot, yielding between those extra steps. */
static inline int thread_passes_continue(struct thread_passes *passes, int step, int minimum) {
    if (step < minimum) return 1;
    if (thread_passes_all_made(passes, THREAD_PASSES_DURING)) return 0;
    thread_passes_check_deadline(passes, "a thread made too few passes during main's work, within the deadline");
    sched_yield();
    return 1;
}

/* After the threads are joined: every thread made its passes during main's work. Returns the total passes. */
static inline long thread_passes_finish(struct thread_passes *passes) {
    if (!thread_passes_all_made(passes, THREAD_PASSES_DURING))
        thread_passes_fail(passes, "a thread made too few passes during main's work");
    long total = 0;
    for (int i = 0; i < passes->threads; i++) total += atomic_load(&passes->passes[i]);
    return total;
}

#endif
