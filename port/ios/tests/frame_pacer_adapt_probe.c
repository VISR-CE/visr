/* Adaptive frames in flight (frame_pacer_adapt.h) against synthetic streams: each stream says how a frame looks
at one and at two in flight, and the probe checks where the rule settles and how often it switches. The class:
a frames-in-flight limit applied where its condition does not hold (one in flight costing frames where the work
does not fit, two costing latency where the rate was never at stake). */
#include "frame_pacer_adapt.h"

#include <stdio.h>

static int checks, failures;

#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    fprintf(stderr, "frame_pacer_adapt_probe: line %d: %s\n", __LINE__, #condition); } } while (0)

/* how a frame looks under each limit (index 1: one in flight, 2: two) */
struct stream
{
    double period;                 /* the frame period in force */
    double interval[3];            /* time between shown frames */
    int others[3];                 /* frames holding a slot when it took its own */
    double gpu, grant, lead;
    double jitter;                 /* the display link's period wobbles by up to this share, frame to frame */
    double hitch_at;               /* a 0.4 s load at this second of the run; 0: none */
    int cpu_caused;                /* late frames at one never waited for their slot (the CPU ran long) */
};

static double shown_rate;          /* the run's frames a second */

static struct frame_pacer_adapt run(const struct stream *s, int start_limit, double seconds, int transition)
{
    struct frame_pacer_adapt adapt;
    double at = 1.0, interval = 0.0;
    unsigned long frames = 0, n = 0;
    int hitched = 0;

    frame_pacer_adapt_reset(&adapt, 1, 2);
    adapt.limit = start_limit;
    while (at < 1.0 + seconds)
    {
        int limit = adapt.limit;
        /* (a deterministic wobble: +-jitter of the period, never the same two frames running) */
        double wobble = s->jitter * s->period * ((double)((n * 7919) % 201) / 100.0 - 1.0);
        struct frame_pacer_adapt_frame frame = {
            .at = at, .interval = interval, .period = s->period + wobble, .gpu = s->gpu, .grant_to_present = s->grant,
            .lead = s->lead, .slot_waited = limit == 1 && !s->cpu_caused, .others_in_flight = s->others[limit],
            .transition = transition,
        };

        frame_pacer_adapt_frame(&adapt, &frame);
        n++;
        frames++;
        interval = s->interval[adapt.limit];
        if (s->hitch_at > 0.0 && !hitched && at >= 1.0 + s->hitch_at)
        {
            hitched = 1;
            interval += 0.4;
        }
        at += interval;
    }
    shown_rate = frames / seconds;
    return adapt;
}

int main(void)
{
    const double p60 = 1.0 / 60.0;
    struct frame_pacer_adapt a, b;

    /* fits at 60 (GPU 6 ms, grant to present 3, lead 3): one in flight, from either start, one switch at most */
    struct stream fits = { p60, { 0, p60, p60 }, { 0, 0, 1 }, 0.006, 0.003, 0.003, 0, 0, 0 };
    a = run(&fits, 2, 10.0, 0);
    b = run(&fits, 1, 10.0, 0);
    CHECK(a.limit == 1 && a.switches == 1 && a.reason == FRAME_PACER_ADAPT_FITS);
    CHECK(b.limit == 1 && b.switches == 0);

    /* GPU-bound at 60 (the iPad Air 4: GPU 14 ms): one in flight would halve the rate, so it stays at two */
    struct stream gpu_bound = { p60, { 0, 2 * p60, p60 }, { 0, 0, 1 }, 0.014, 0.003, 0.003, 0, 0, 0 };
    a = run(&gpu_bound, 2, 30.0, 0);
    CHECK(a.limit == 2 && a.switches == 0);
    /* from the floor it leaves on slot-caused misses and does not come back on its own */
    b = run(&gpu_bound, 1, 30.0, 0);
    CHECK(b.limit == 2 && b.switches == 1 && b.reason == FRAME_PACER_ADAPT_MISSES);

    /* the model says it fits but the floor misses: the retry delay bounds the switches */
    struct stream wrong = { p60, { 0, 2 * p60, p60 }, { 0, 0, 1 }, 0.005, 0.003, 0.003, 0, 0, 0 };
    a = run(&wrong, 2, 300.0, 0);
    CHECK(a.switches >= 4 && a.switches <= 24 && a.retry_delay == FRAME_PACER_ADAPT_RETRY_MOST);

    /* under rate, CPU-bound (the A10X iPad Pro at 128 players, about 18 a second, second slot idle): the floor */
    struct stream cpu_under = { p60, { 0, 0.053, 0.055 }, { 0, 0, 0 }, 0.030, 0.004, 0.003, 0, 0, 0 };
    a = run(&cpu_under, 2, 30.0, 0);
    CHECK(a.limit == 1 && a.switches == 1 && a.reason == FRAME_PACER_ADAPT_UNDER);
    /* (from the floor it has no rate at the ceiling to judge against: it goes up once to learn it, then back) */
    b = run(&cpu_under, 1, 30.0, 0);
    CHECK(b.limit == 1 && b.switches <= 2);

    /* under rate, GPU-bound (50 a second with the second slot in use): two stays */
    struct stream gpu_under = { p60, { 0, 2 * p60, 0.020 }, { 0, 0, 1 }, 0.019, 0.003, 0.003, 0, 0, 0 };
    a = run(&gpu_under, 2, 30.0, 0);
    CHECK(a.limit == 2 && a.switches == 0);

    /* under rate with the second slot idle, but the floor loses rate: it leaves, and the retries grow */
    /* (30 ms at the floor against 22 at the ceiling: never one and a half intervals late, so the rate test leaves) */
    struct stream lossy = { p60, { 0, 0.030, 0.022 }, { 0, 0, 0 }, 0.012, 0.003, 0.003, 0, 0, 0 };
    a = run(&lossy, 2, 120.0, 0);
    CHECK(a.reason == FRAME_PACER_ADAPT_RATE_LOST || (a.limit == 1 && a.switches > 2));
    CHECK(a.switches >= 2 && a.switches <= 16 && a.retry_delay >= 32.0);
    CHECK(shown_rate > 0.95 / 0.022 * 0.9);
    /* (33 ms at the floor: over one and a half of 22, so the misses leave first) */
    struct stream missing = { p60, { 0, 2 * p60, 0.022 }, { 0, 0, 0 }, 0.012, 0.003, 0.003, 0, 0, 0 };
    a = run(&missing, 2, 120.0, 0);
    CHECK(a.reason == FRAME_PACER_ADAPT_MISSES || (a.limit == 1 && a.switches > 2));
    CHECK(a.switches >= 2 && a.switches <= 16 && a.retry_delay >= 32.0);
    CHECK(shown_rate > 0.95 / 0.022 * 0.9);

    /* transition frames count toward nothing */
    a = run(&fits, 2, 10.0, 1);
    CHECK(a.limit == 2 && a.switches == 0);

    /* no valid GPU time: holds its limit */
    struct stream blind = fits;
    blind.gpu = 0.0;
    a = run(&blind, 2, 10.0, 0);
    CHECK(a.limit == 2 && a.switches == 0);

    /* no known grant-to-present span (below 0): holds its limit */
    struct stream no_grant = fits;
    no_grant.grant = -1.0;
    a = run(&no_grant, 2, 10.0, 0);
    CHECK(a.limit == 2 && a.switches == 0);

    /* a 30 cap on a 60 Hz display (a share of two refreshes): fits at 33 ms */
    struct stream cap30 = { 2 * p60, { 0, 2 * p60, 2 * p60 }, { 0, 0, 1 }, 0.016, 0.004, 0.003, 0, 0, 0 };
    a = run(&cap30, 2, 10.0, 0);
    CHECK(a.limit == 1 && a.reason == FRAME_PACER_ADAPT_FITS);

    /* 120 Hz: GPU 6 ms does not fit 8.3 ms with the grant and the lead */
    struct stream hz120 = { 1.0 / 120.0, { 0, 2.0 / 120.0, 1.0 / 120.0 }, { 0, 0, 1 }, 0.006, 0.002, 0.003, 0, 0, 0 };
    a = run(&hz120, 2, 10.0, 0);
    CHECK(a.limit == 2 && a.switches == 0);

    /* a fixed limit (floor at the ceiling) never moves */
    frame_pacer_adapt_reset(&a, 2, 2);
    for (int i = 0; i < 600; i++)
    {
        struct frame_pacer_adapt_frame frame = { .at = 1.0 + i * p60, .interval = p60, .period = p60, .gpu = 0.004,
            .grant_to_present = 0.002, .lead = 0.003 };
        CHECK(frame_pacer_adapt_frame(&a, &frame) == 2);
    }

    /* a change of the period keeps the limit and forgives the delay */
    a = run(&gpu_bound, 1, 30.0, 0);
    CHECK(a.limit == 2 && a.retry_delay > FRAME_PACER_ADAPT_RETRY_FIRST);
    {
        struct frame_pacer_adapt_frame frame = { .at = 100.0, .interval = 2 * p60, .period = 2 * p60, .gpu = 0.014,
            .grant_to_present = 0.003, .lead = 0.003 };
        frame_pacer_adapt_frame(&a, &frame);
        CHECK(a.limit == 2 && a.retry_delay == FRAME_PACER_ADAPT_RETRY_FIRST && a.gpu_count == 1);
    }

    /* the display link's jitter (one part in 10^6 to a percent) is not a new rate: a fitting stream still settles at
       one, and a GPU-bound one started at one still leaves */
    {
        static const double jitters[] = { 1e-6, 1e-3, 1e-2 };

        for (int j = 0; j < 3; j++)
        {
            struct stream wobbling = fits, bound = gpu_bound;

            wobbling.jitter = bound.jitter = jitters[j];
            a = run(&wobbling, 2, 10.0, 0);
            CHECK(a.limit == 1 && a.switches == 1);
            b = run(&bound, 1, 30.0, 0);
            CHECK(b.limit == 2 && b.switches == 1 && shown_rate > 55.0);
        }
    }

    /* a 0.4 s load just before an entry is not the rate at the ceiling: a fitting stream at the floor that misses
       still leaves, and an under-rate CPU-bound stream is judged on its frames, not the hitch */
    {
        struct stream hitch = wrong;

        hitch.hitch_at = 0.5;
        a = run(&hitch, 2, 30.0, 0);
        CHECK(a.switches >= 2 && a.reason == FRAME_PACER_ADAPT_MISSES);
        hitch = cpu_under;
        hitch.hitch_at = 0.5;
        a = run(&hitch, 2, 30.0, 0);
        CHECK(a.limit == 1 && a.switches == 1 && a.reason == FRAME_PACER_ADAPT_UNDER);
    }

    /* under rate with the same interval at one and at two (CPU-bound): it enters once and stays, the rate test not
       counting the frame that starts each second */
    {
        struct stream same = { p60, { 0, 0.055, 0.055 }, { 0, 0, 0 }, 0.030, 0.004, 0.003, 0, 0, 0 };

        a = run(&same, 2, 60.0, 0);
        CHECK(a.limit == 1 && a.switches == 1);
    }

    /* CPU-caused misses at the floor (the frame never waited for its slot) do not leave it */
    {
        struct stream cpu_late = fits;

        cpu_late.interval[1] = 2 * p60;
        cpu_late.cpu_caused = 1;
        b = run(&cpu_late, 1, 30.0, 0);
        CHECK(b.limit == 1 && b.switches == 0);
    }

    if (failures)
        return 1;
    printf("frame_pacer_adapt_probe: %d checks passed\n", checks);
    return 0;
}
