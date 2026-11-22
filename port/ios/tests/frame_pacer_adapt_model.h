/* A small device model for frame_pacer_adapt_probe.c: CPU and GPU time per frame by phase, a display of period
`refresh` showing a frame every `share` refreshes, the pacer's slot rule (a frame starts once fewer than the limit
are waiting to be shown) and frame spacing. It feeds frame_pacer_adapt.h the frames it shows, as host_frame_pacer.c
does, and counts the frames each phase shows, so the probe can compare the rule with fixed one and fixed two while
the workload changes under it. */
#pragma once

#include "frame_pacer_adapt.h"

#include <math.h>
#include <string.h>

#define FRAME_PACER_MODEL_PHASES 8
#define FRAME_PACER_MODEL_HELD 8

struct frame_pacer_model_phase
{
	double until;     /* seconds from the start the phase ends */
	double cpu, gpu;  /* per frame */
};

struct frame_pacer_model_result
{
	double frames[FRAME_PACER_MODEL_PHASES];   /* shown in each phase */
	double at_one[FRAME_PACER_MODEL_PHASES];   /* seconds each phase spent at one in flight */
	int reasons[8];                            /* switches by reason */
	unsigned long switches;
};

/* fixed: 1 or 2 for a fixed limit, 0 for the adaptive rule */
static inline struct frame_pacer_model_result frame_pacer_model_run(const struct frame_pacer_model_phase *phases, int count,
	double refresh, int share, double lead, int fixed)
{
	struct frame_pacer_model_result r;
	struct frame_pacer_adapt adapt;
	double shown[FRAME_PACER_MODEL_HELD], gpu_of[FRAME_PACER_MODEL_HELD];
	unsigned long number[FRAME_PACER_MODEL_HELD];
	int waited_of[FRAME_PACER_MODEL_HELD], held = 0;
	double start = 1.0, gpu_done = 0.0, last_shown = 0.0, last_fed = 0.0;
	unsigned long n = 0, last_granted = 0, drain_through = 0;
	int limit = fixed ? fixed : 2;
	double end = phases[count - 1].until + 1.0;

	memset(&r, 0, sizeof(r));
	frame_pacer_adapt_reset(&adapt, 1, 2);
	while (start < end)
	{
		int p = 0, waited = 0;
		double ready, grant;

		while (p < count - 1 && start - 1.0 >= phases[p].until)
			p++;
		ready = start + phases[p].cpu;
		for (;;)
		{
			/* feed every frame shown by now */
			while (held && shown[0] <= ready)
			{
				struct frame_pacer_adapt_frame f = {
					.at = shown[0], .interval = last_fed > 0.0 ? shown[0] - last_fed : 0.0, .period = refresh * share,
					.gpu = gpu_of[0], .grant_to_present = 0.0003, .lead = lead, .slot_waited = waited_of[0],
					.transition = number[0] <= drain_through,
				};
				unsigned long before = adapt.switches;
				int q = 0;

				while (q < count - 1 && shown[0] - 1.0 >= phases[q].until)
					q++;
				r.frames[q]++;
				if (limit == 1 && f.interval < FRAME_PACER_ADAPT_GAP)
					r.at_one[q] += f.interval;
				if (!fixed)
				{
					int now_limit = frame_pacer_adapt_frame(&adapt, &f);

					if (adapt.switches != before)
					{
						limit = now_limit;
						drain_through = last_granted;
						r.reasons[adapt.reason]++;
					}
				}
				last_fed = shown[0];
				held--;
				memmove(shown, shown + 1, held * sizeof(shown[0]));
				memmove(number, number + 1, held * sizeof(number[0]));
				memmove(gpu_of, gpu_of + 1, held * sizeof(gpu_of[0]));
				memmove(waited_of, waited_of + 1, held * sizeof(waited_of[0]));
			}
			if (held < limit)
				break;
			waited = 1;
			ready = shown[0];
		}
		grant = ready;
		last_granted = ++n;
		gpu_done = (grant > gpu_done ? grant : gpu_done) + phases[p].gpu;
		{
			double at = ceil((gpu_done + lead) / refresh - 1e-9) * refresh;

			if (last_shown > 0.0 && at < last_shown + share * refresh - 1e-9)
				at = last_shown + share * refresh;
			shown[held] = at;
			number[held] = n;
			gpu_of[held] = phases[p].gpu;
			waited_of[held] = waited;
			held++;
			last_shown = at;
		}
		start = grant;
	}
	r.switches = adapt.switches;
	return r;
}
