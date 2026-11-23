/* Adaptive frames in flight (frame_pacer_adapt.h) against synthetic streams: each stream says how a frame looks
at one and at two in flight, and the probe checks where the rule settles and how often it switches. The class:
a frames-in-flight limit applied where its condition does not hold (one in flight costing frames where the work
does not fit, two costing latency where the rate was never at stake). */
#include "frame_pacer_adapt.h"
#include "frame_pacer_adapt_model.h"

#include <stdio.h>

static int checks, failures;

#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    fprintf(stderr, "frame_pacer_adapt_probe: line %d: %s\n", __LINE__, #condition); } } while (0)

/* how a frame looks under each limit (index 1: one in flight, 2: two) */
struct stream
{
    double period;                 /* the frame period in force */
    double interval[3];            /* time between shown frames */
    double gpu, grant, lead;
    double jitter;                 /* the display link's period wobbles by up to this share, frame to frame */
    double hitch_at;               /* a 0.4 s load at this second of the run; 0: none */
    int cpu_caused;                /* late frames at one never waited for their slot (the CPU ran long) */
};

static double shown_rate;          /* the run's frames a second */
static double floor_share;         /* the run's share of time at one in flight */
static int exits;                  /* left the floor on misses or rate loss */

static struct frame_pacer_adapt run(const struct stream *s, int start_limit, double seconds, int transition)
{
    struct frame_pacer_adapt adapt;
    double at = 1.0, interval = 0.0;
    unsigned long frames = 0, n = 0;
    int hitched = 0;
    double at_floor = 0.0;

    exits = 0;

    frame_pacer_adapt_reset(&adapt, 1, 2);
    adapt.limit = start_limit;
    while (at < 1.0 + seconds)
    {
        int limit = adapt.limit;
        /* (a deterministic wobble: +-jitter of the period, never the same two frames running) */
        double wobble = s->jitter * s->period * ((double)((n * 7919) % 201) / 100.0 - 1.0);
        struct frame_pacer_adapt_frame frame = {
            .at = at, .interval = interval, .period = s->period + wobble, .gpu = s->gpu, .grant_to_present = s->grant,
            .lead = s->lead, .slot_waited = limit == 1 && !s->cpu_caused,
            /* (a frame at one that waited for its slot waited what one costs over two: the difference of intervals) */
            .waited = limit == 1 && !s->cpu_caused && s->interval[1] > s->interval[2] ? s->interval[1] - s->interval[2] : 0.0,
            .transition = transition,
        };

        unsigned long before = adapt.switches;

        frame_pacer_adapt_frame(&adapt, &frame);
        if (adapt.switches != before)
        {
            exits += adapt.reason == FRAME_PACER_ADAPT_MISSES || adapt.reason == FRAME_PACER_ADAPT_RATE_LOST;
        }
        n++;
        frames++;
        interval = s->interval[adapt.limit];
        if (s->hitch_at > 0.0 && !hitched && at >= 1.0 + s->hitch_at)
        {
            hitched = 1;
            interval += 0.4;
        }
        if (adapt.limit == 1)
            at_floor += interval;
        at += interval;
    }
    shown_rate = frames / seconds;
    floor_share = at_floor / seconds;
    return adapt;
}

int main(void)
{
    const double p60 = 1.0 / 60.0;
    struct frame_pacer_adapt a, b;

    /* fits at 60 (GPU 6 ms, grant to present 3, lead 3): one in flight, from either start, one switch at most */
    struct stream fits = { p60, { 0, p60, p60 }, 0.006, 0.003, 0.003, 0, 0, 0 };
    a = run(&fits, 2, 10.0, 0);
    b = run(&fits, 1, 10.0, 0);
    CHECK(a.limit == 1 && a.switches == 1 && a.reason == FRAME_PACER_ADAPT_FITS);
    CHECK(b.limit == 1 && b.switches == 0);

    /* GPU-bound at 60 (the iPad Air 4: GPU 14 ms): one in flight would halve the rate, so it stays at two */
    struct stream gpu_bound = { p60, { 0, 2 * p60, p60 }, 0.014, 0.003, 0.003, 0, 0, 0 };
    a = run(&gpu_bound, 2, 30.0, 0);
    CHECK(a.limit == 2 && a.switches == 0);
    /* from the floor it leaves on slot-caused misses and does not come back on its own */
    b = run(&gpu_bound, 1, 30.0, 0);
    CHECK(b.limit == 2 && b.switches == 1 && b.reason == FRAME_PACER_ADAPT_MISSES);

    /* the model says it fits but the floor misses: the retry delay bounds the switches */
    struct stream wrong = { p60, { 0, 2 * p60, p60 }, 0.005, 0.003, 0.003, 0, 0, 0 };
    a = run(&wrong, 2, 300.0, 0);
    CHECK(a.switches >= 4 && a.switches <= 24 && a.retry_delay == FRAME_PACER_ADAPT_RETRY_MOST);

    /* under rate, CPU-bound (the A10X iPad Pro at 128 players, about 18 a second): the floor */
    struct stream cpu_under = { p60, { 0, 0.053, 0.055 }, 0.030, 0.004, 0.003, 0, 0, 0 };
    a = run(&cpu_under, 2, 30.0, 0);
    CHECK(exits == 0 && floor_share > 0.9 && a.switches == 1);
    /* (from the floor it has no rate at the ceiling to judge against: it goes up once to learn it, then back) */
    b = run(&cpu_under, 1, 30.0, 0);
    CHECK(exits <= 1 && floor_share > 0.8);

    /* under rate, GPU-bound (50 a second with the second slot in use): two stays */
    /* (one halves it, 50 to 30 a second: the rate-loss exit leaves, the retries grow, and the run keeps most of
       fixed two's rate) */
    struct stream gpu_under = { p60, { 0, 2 * p60, 0.020 }, 0.019, 0.003, 0.003, 0, 0, 0 };
    a = run(&gpu_under, 2, 300.0, 0);
    CHECK(a.limit == 2 && a.switches >= 4 && a.switches <= 24 && a.retry_delay == FRAME_PACER_ADAPT_RETRY_MOST);
    CHECK(shown_rate > 0.95 * 50.0);

    /* the A10X iPad Pro at 128 players (traces, 2026-11-22): GPU-bound with the second slot in use, 20.5 a second at
       two and 19.5 at one, 5% fewer: one in flight, for 25 to 70 ms less delay */
    struct stream a10x = { p60, { 0, 1.0 / 19.5, 1.0 / 20.5 }, 0.040, 0.0002, 0.003, 0, 0, 0 };
    a = run(&a10x, 2, 120.0, 0);
    CHECK(exits == 0 && floor_share > 0.95 && a.switches == 1);

    /* the iPad Air 4 at 32 players (traces): GPU 14 ms, one in flight shows 30 a second: two, never one */
    struct stream air4 = { p60, { 0, 2 * p60, p60 }, 0.021, 0.0003, 0.003, 0, 0, 0 };
    a = run(&air4, 2, 120.0, 0);
    CHECK(a.limit == 2 && a.switches == 0 && shown_rate > 59.0);

    /* under rate, but the floor loses rate: it leaves, and the retries grow */
    /* (30 ms at the floor against 22 at the ceiling: never one and a half intervals late, so the rate test leaves) */
    struct stream lossy = { p60, { 0, 0.030, 0.022 }, 0.012, 0.003, 0.003, 0, 0, 0 };
    a = run(&lossy, 2, 120.0, 0);
    CHECK(a.reason == FRAME_PACER_ADAPT_RATE_LOST || (a.limit == 1 && a.switches > 2));
    CHECK(a.switches >= 2 && a.switches <= 16 && a.retry_delay >= 32.0);
    CHECK(shown_rate > 0.95 / 0.022 * 0.9);
    /* (33 ms at the floor: over one and a half of 22, so the misses leave first) */
    struct stream missing = { p60, { 0, 2 * p60, 0.022 }, 0.012, 0.003, 0.003, 0, 0, 0 };
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
    struct stream cap30 = { 2 * p60, { 0, 2 * p60, 2 * p60 }, 0.016, 0.004, 0.003, 0, 0, 0 };
    a = run(&cap30, 2, 10.0, 0);
    CHECK(a.limit == 1 && a.reason == FRAME_PACER_ADAPT_FITS);

    /* 120 Hz: GPU 6 ms does not fit 8.3 ms with the grant and the lead */
    struct stream hz120 = { 1.0 / 120.0, { 0, 2.0 / 120.0, 1.0 / 120.0 }, 0.006, 0.002, 0.003, 0, 0, 0 };
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
        CHECK(exits == 0 && floor_share > 0.8);
    }

    /* under rate with the same interval at one and at two (CPU-bound): it enters once and stays, the rate test not
       counting the frame that starts each second */
    {
        struct stream same = { p60, { 0, 0.055, 0.055 }, 0.030, 0.004, 0.003, 0, 0, 0 };

        a = run(&same, 2, 60.0, 0);
        CHECK(exits == 0 && floor_share > 0.85);
    }

    /* CPU-caused misses at the floor (the frame never waited for its slot) do not leave it */
    {
        struct stream cpu_late = fits;

        cpu_late.interval[1] = 2 * p60;
        cpu_late.cpu_caused = 1;
        b = run(&cpu_late, 1, 30.0, 0);
        CHECK(b.limit == 1 && b.switches == 0);
    }

    /* the workload changes under the rule (device model, frame_pacer_adapt_model.h): in every phase the rule shows at
       least 90% of what fixed two shows, so no floor outlives the workload that justified it (second code review,
       B1: an under-rate entry judged only against the rate at entry kept the floor after the load eased) */
    {
        const double refresh = 1.0 / 60.0;
        static const struct { const char *name; int share; int count; struct frame_pacer_model_phase phases[4]; } cases[] = {
            { "Air 4 like, a 5 s stretch at 31 a second", 1, 3, { { 20, 0.010, 0.014 }, { 25, 0.032, 0.014 }, { 325, 0.010, 0.014 } } },
            { "Air 4 like, a 5 s stretch at 36 a second", 1, 3, { { 20, 0.010, 0.014 }, { 25, 0.028, 0.014 }, { 325, 0.010, 0.014 } } },
            { "A10X like at 30, CPU eases from 49 to 30 ms", 2, 2, { { 30, 0.049, 0.040 }, { 330, 0.030, 0.040 } } },
            { "A10X like at 30, load eases to CPU 25, GPU 31 ms", 2, 2, { { 30, 0.049, 0.040 }, { 330, 0.025, 0.031 } } },
            { "A10X like at 30, load eases to fit", 2, 2, { { 30, 0.049, 0.040 }, { 330, 0.020, 0.020 } } },
            { "60: CPU-bound under rate, then GPU-bound", 1, 2, { { 30, 0.024, 0.010 }, { 330, 0.012, 0.0155 } } },
            { "60: deeper under rate, then GPU-bound", 1, 2, { { 30, 0.030, 0.010 }, { 330, 0.012, 0.0155 } } },
            /* a 3 s load (one frame): a gap, not rate; read as rate, the span that holds it reads under 25 a second at two
               and enters the floor, where one in flight (30) is never late against it and never loses rate */
            { "Air 4 like, a 3 s load", 1, 3, { { 20, 0.010, 0.014 }, { 23, 3.0, 0.014 }, { 83, 0.010, 0.014 } } },
        };

        for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            struct frame_pacer_model_result two = frame_pacer_model_run(cases[i].phases, cases[i].count, refresh,
                cases[i].share, 0.003, 2);
            struct frame_pacer_model_result rule = frame_pacer_model_run(cases[i].phases, cases[i].count, refresh,
                cases[i].share, 0.003, 0);
            int last = cases[i].count - 1;

            if (!(rule.frames[last] >= 0.9 * two.frames[last]))
                fprintf(stderr, "frame_pacer_adapt_probe: %s: %.0f frames against %.0f at two\n", cases[i].name,
                    rule.frames[last], two.frames[last]);
            CHECK(rule.frames[last] >= 0.9 * two.frames[last]);
        }
        /* after exits that grew the retry delay (one in flight halving a 50-frame load), a long calm stay forgives it */
        {
            static const struct frame_pacer_model_phase calm[] = { { 60, 0.010, 0.019 }, { 200, 0.049, 0.040 } };
            struct frame_pacer_model_result rule = frame_pacer_model_run(calm, 2, refresh, 1, 0.003, 0);

            CHECK(rule.reasons[FRAME_PACER_ADAPT_RATE_LOST] + rule.reasons[FRAME_PACER_ADAPT_MISSES] >= 2);
            CHECK(rule.retry_delay == FRAME_PACER_ADAPT_RETRY_FIRST);
        }
        /* the 3 s load is a gap: no span reads it as rate, so the rule never leaves two for it */
        {
            static const struct frame_pacer_model_phase load[] = { { 20, 0.010, 0.014 }, { 23, 3.0, 0.014 }, { 83, 0.010, 0.014 } };
            struct frame_pacer_model_result rule = frame_pacer_model_run(load, 3, refresh, 1, 0.003, 0);

            CHECK(rule.switches == 0);
        }
        /* a stay entered under rate while a level loads, whose work then fits (the A10X campaign at 30: CPU 8.5 ms,
           GPU 14 in a 33 ms period), is a fits stay: one in flight for the rest, with no exit (on device a rate kept
           from the load made it leave twice) */
        {
            static const struct frame_pacer_model_phase loaded[] = { { 10, 0.060, 0.045 }, { 130, 0.0085, 0.014 } };
            struct frame_pacer_model_result rule = frame_pacer_model_run(loaded, 2, refresh, 2, 0.003, 0);

            CHECK(rule.reasons[FRAME_PACER_ADAPT_UNDER] == 1 && rule.reasons[FRAME_PACER_ADAPT_FITS] == 0);
            CHECK(rule.stay == FRAME_PACER_ADAPT_FITS && rule.switches == 1 && rule.at_one[1] > 0.99 * 120.0);
        }
        /* and the A10X at 128 players held steady keeps most of its time at one, re-measuring now and then */
        {
            static const struct frame_pacer_model_phase steady[] = { { 120, 0.049, 0.040 } };
            struct frame_pacer_model_result one = frame_pacer_model_run(steady, 1, refresh, 2, 0.003, 1);
            struct frame_pacer_model_result rule = frame_pacer_model_run(steady, 1, refresh, 2, 0.003, 0);

            CHECK(rule.at_one[0] > 0.8 * 120.0 && rule.frames[0] >= 0.95 * one.frames[0]);
            CHECK(rule.reasons[FRAME_PACER_ADAPT_MISSES] + rule.reasons[FRAME_PACER_ADAPT_RATE_LOST] <= 2);
        }
    }

    if (failures)
        return 1;
    printf("frame_pacer_adapt_probe: %d checks passed\n", checks);
    return 0;
}
