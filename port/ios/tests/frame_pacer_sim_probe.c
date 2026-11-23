/* The extended late start (frame_pacer.h, display.late_start = "extended")
in a model of the whole paced loop (frame_pacer_sim.h): the class under test
is "the wait for the display sits before the input read, and nothing else in
the loop waits". Each case runs the same loads with late start off, with the
late start as it was ("classic") and extended, and checks what the player
gets: the time from the input read to the screen, frames shown late, and
that dynamic resolution still lowers and raises the picture.

tools/ios_test.py builds and runs it. With "table" it prints the delays for
a set of loads instead (the sizing table in the design document). */
#include "frame_pacer_sim.h"

#include <stdio.h>

static int failures;

#define CHECK(condition) \
	do \
	{ \
		if (!(condition)) \
		{ \
			printf("frame_pacer_sim_probe: line %d: %s\n", __LINE__, #condition); \
			failures++; \
		} \
	} while (0)

#define P120 (1.0 / 120.0)
#define P60 (1.0 / 60.0)

/* a load: steady costs with noise, a spike on one frame, and a GPU load that rises and falls over time */
struct load
{
	double cpu, gpu_fixed, gpu_pixels;  /* gpu_pixels: the per-pixel part at the full picture */
	double cpu_noise, gpu_noise;        /* each frame adds up to this much, evenly spread */
	long spike_frame;                   /* 0 none */
	long spike_every;                   /* 0: only spike_frame; else every so many frames from it */
	long tick_every;                    /* 0 none: every so many frames the CPU's work is tick_cpu longer (a game tick) */
	double spike_cpu, tick_cpu;
	double rise_from, rise_to, fall_from, fall_to, risen; /* the GPU's per-pixel part times up to `risen` */
	double full_pixels;
	unsigned random;
};

static double load_noise(struct load *load)
{
	load->random = load->random * 1664525u + 1013904223u;
	return (double)(load->random >> 8) / (double)(1u << 24);
}

static void load_cost(const struct sim *sim, long frame, double now, double *cpu, double *gpu_fixed, double *gpu_per_pixel)
{
	struct load *load = sim->user;
	double scale = 1.0, since = now - 100.0;

	if (load->risen > 0.0)
	{
		if (since >= load->fall_to)
			scale = 1.0;
		else if (since >= load->fall_from)
			scale = load->risen + (1.0 - load->risen) * (since - load->fall_from) / (load->fall_to - load->fall_from);
		else if (since >= load->rise_to)
			scale = load->risen;
		else if (since >= load->rise_from)
			scale = 1.0 + (load->risen - 1.0) * (since - load->rise_from) / (load->rise_to - load->rise_from);
	}
	*cpu = load->cpu + load->cpu_noise * load_noise(load);
	if (load->spike_frame && (frame == load->spike_frame || (load->spike_every && frame > load->spike_frame &&
		(frame - load->spike_frame) % load->spike_every == 0)))
		*cpu += load->spike_cpu;
	if (load->tick_every && frame % load->tick_every == 0)
		*cpu += load->tick_cpu;
	*gpu_fixed = load->gpu_fixed + load->gpu_noise * load_noise(load);
	*gpu_per_pixel = load->gpu_pixels * scale / (load->full_pixels > 0.0 ? load->full_pixels : 1.0);
}

static struct sim *run(int mode, double panel, double link, double share, int limit, struct load *load, long frames)
{
	struct sim *sim = calloc(1, sizeof(*sim));

	load->random = 2026u;
	sim->panel = panel;
	sim->link = link;
	sim->share = share;
	sim->limit = limit;
	sim->mode = mode;
	sim->latch = 0.0015;
	sim->notice = 0.0008;
	sim->cost = load_cost;
	sim->user = load;
	sim_start(sim, frames);
	return sim;
}

static void finish(struct sim *sim)
{
	free(sim->frames);
	free(sim);
}

static const char *mode_name(int mode)
{
	return mode == SIM_OFF ? "off" : mode == SIM_CLASSIC ? "classic" : "extended";
}

static void row(const char *name, int mode, const struct sim *sim, long first)
{
	struct sim_result result = sim_measure(sim, first, sim->count);

	printf("%-34s %-8s %6.1f %6.1f %6.1f  %6.1f fps  late %4ld  engaged %5.1f%%  missed %3ld  waits %5.2f ms  lead %.1f\n",
		name, mode_name(mode), result.read_to_shown_median * 1e3, result.read_to_shown_p95 * 1e3,
		result.read_to_shown_worst * 1e3, result.rate, result.late,
		100.0 * (double)result.engaged / (double)result.frames, result.missed, result.waited_mean * 1e3,
		(mode == SIM_EXTENDED ? frame_pacer_extended_lead(&sim->extended) : frame_pacer_late_lead(&sim->classic)) * 1e3);
}

static void table(void)
{
	static const struct
	{
		const char *name;
		double panel, link, share;
		int limit;
		struct load load;
	} cases[] = {
		{ "120 Hz light (2+3 ms)", P120, P120, 1.0, 2, { .cpu = 0.002, .gpu_fixed = 0.003, .cpu_noise = 0.0005, .gpu_noise = 0.0005 } },
		{ "120 Hz DRS band (3+7 ms)", P120, P120, 1.0, 2, { .cpu = 0.003, .gpu_fixed = 0.007, .cpu_noise = 0.0008, .gpu_noise = 0.0005 } },
		{ "120 Hz CPU-heavy (6+7 ms)", P120, P120, 1.0, 2, { .cpu = 0.006, .gpu_fixed = 0.007, .cpu_noise = 0.001, .gpu_noise = 0.0005 } },
		{ "120 Hz, one in flight (3+7 ms)", P120, P120, 1.0, 1, { .cpu = 0.003, .gpu_fixed = 0.007, .cpu_noise = 0.0008, .gpu_noise = 0.0005 } },
		{ "120 Hz, one in flight (2+3 ms)", P120, P120, 1.0, 1, { .cpu = 0.002, .gpu_fixed = 0.003, .cpu_noise = 0.0005, .gpu_noise = 0.0005 } },
		{ "60 cap on 120 panel (3+7 ms)", P120, P60, 1.0, 2, { .cpu = 0.003, .gpu_fixed = 0.007, .cpu_noise = 0.0008, .gpu_noise = 0.0005 } },
		{ "60 cap on 120 panel (3+14 ms)", P120, P60, 1.0, 2, { .cpu = 0.003, .gpu_fixed = 0.014, .cpu_noise = 0.0008, .gpu_noise = 0.0008 } },
		{ "60 Hz DRS band (6+14.5 ms)", P60, P60, 1.0, 2, { .cpu = 0.006, .gpu_fixed = 0.0145, .cpu_noise = 0.001, .gpu_noise = 0.0008 } },
		{ "60 Hz light (4+5 ms)", P60, P60, 1.0, 2, { .cpu = 0.004, .gpu_fixed = 0.005, .cpu_noise = 0.001, .gpu_noise = 0.0008 } },
		{ "60 Hz steady (4+4 ms, no noise)", P60, P60, 1.0, 2, { .cpu = 0.004, .gpu_fixed = 0.004 } },
		{ "30 cap on 60 link (8+14 ms)", P60, P60, 2.0, 2, { .cpu = 0.008, .gpu_fixed = 0.014, .cpu_noise = 0.001, .gpu_noise = 0.001 } },
		{ "30 cap on 60 link (12+28 ms)", P60, P60, 2.0, 2, { .cpu = 0.012, .gpu_fixed = 0.028, .cpu_noise = 0.002, .gpu_noise = 0.001 } },
		{ "120 Hz below rate (3+10 ms)", P120, P120, 1.0, 2, { .cpu = 0.003, .gpu_fixed = 0.010, .cpu_noise = 0.0008, .gpu_noise = 0.0005 } },
	};
	unsigned index;
	int mode;

	printf("%-34s %-8s %6s %6s %6s\n", "load (CPU+GPU)", "start", "median", "p95", "worst");
	for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++)
		for (mode = SIM_OFF; mode <= SIM_EXTENDED; mode++)
		{
			struct load load = cases[index].load;
			struct sim *sim = run(mode, cases[index].panel, cases[index].link, cases[index].share, cases[index].limit,
				&load, 6000);

			sim_run(sim, 6000);
			row(cases[index].name, mode, sim, 1200);
			finish(sim);
		}
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "table"))
	{
		table();
		return 0;
	}
	int verbose = argc > 1 && !strcmp(argv[1], "verbose");

	/* ---- starting from frames in flight (the loop without a late start keeps two there): the late start as it was
	aims at a refresh they own, is shown late and gives up; extended waits for them and is never late */
	{
		struct load load = { .cpu = 0.004, .gpu_fixed = 0.004 };
		struct sim *off = run(SIM_OFF, P60, P60, 1.0, 2, &load, 3000), *classic, *extended;
		struct sim_result result;

		sim_run(off, 3000);
		result = sim_measure(off, 600, 3000);
		/* three slots from the read to the screen, two frames in flight at each Present's start */
		CHECK(fabs(result.read_to_shown_median - 3.0 * P60) < 0.002);
		CHECK(off->frames[2000].in_flight_at_present == 2);
		classic = run(SIM_CLASSIC, P60, P60, 1.0, 2, &load, 3000);
		sim_run(classic, 3000);
		result = sim_measure(classic, 600, 3000);
		if (verbose)
			row("from two in flight", SIM_CLASSIC, classic, 600);
		CHECK(result.engaged == 0 && fabs(result.read_to_shown_median - 3.0 * P60) < 0.002);
		extended = run(SIM_EXTENDED, P60, P60, 1.0, 2, &load, 3000);
		sim_run(extended, 3000);
		result = sim_measure(extended, 0, 3000);
		if (verbose)
			row("from two in flight", SIM_EXTENDED, extended, 0);
		CHECK(result.missed == 0 && result.late == 0);
		CHECK(extended->extended.misses == 0 && extended->extended.jumps == 0);
		result = sim_measure(extended, 600, 3000);
		CHECK(result.engaged == result.frames);
		/* the read is the cost and the lead before the screen, to within a millisecond; nothing waits in Present */
		CHECK(result.read_to_shown_p95 < 0.008 + FRAME_PACER_EXTENDED_LEAD + 0.001);
		CHECK(result.waited == 0);
		finish(off);
		finish(classic);
		finish(extended);
	}
	/* ---- 120 Hz with the GPU where dynamic resolution keeps it (85% of a refresh): the budget is 1.4 refreshes,
	the CPU's work overlaps the GPU's on the frame before, and no frame waits for a slot */
	{
		struct load load = { .cpu = 0.003, .gpu_fixed = 0.007, .cpu_noise = 0.0008, .gpu_noise = 0.0005 };
		struct sim *off = run(SIM_OFF, P120, P120, 1.0, 2, &load, 6000), *extended;
		struct sim_result before, after;

		sim_run(off, 6000);
		before = sim_measure(off, 1200, 6000);
		extended = run(SIM_EXTENDED, P120, P120, 1.0, 2, &load, 6000);
		sim_run(extended, 6000);
		after = sim_measure(extended, 1200, 6000);
		if (verbose)
			row("120 Hz, GPU at 85%", SIM_EXTENDED, extended, 1200);
		CHECK(fabs(before.read_to_shown_median - 3.0 * P120) < 0.002);
		CHECK(after.engaged == after.frames && after.missed == 0 && after.late == 0 && after.waited == 0);
		CHECK(after.read_to_shown_p95 < 0.0038 + 0.0075 + FRAME_PACER_EXTENDED_LEAD + 0.001);
		CHECK(fabs(after.rate - 120.0) < 0.1 && fabs(before.rate - 120.0) < 0.1);
		/* noise alone is not a jump: late starts never stopped */
		CHECK(extended->extended.jumps == 0 && extended->extended.held == 0);
		finish(off);
		finish(extended);
	}
	/* ---- the GPU's time a hair under one refresh: each frame's own GPU time is what is measured, so the cost does
	not swing between nothing and double as the frames' ends fall either side of a frame start */
	{
		struct load load = { .cpu = 0.002, .gpu_fixed = 0.0079, .gpu_noise = 0.0002 };
		struct sim *extended = run(SIM_EXTENDED, P120, P120, 1.0, 2, &load, 6000);
		struct sim_result result;

		sim_run(extended, 6000);
		result = sim_measure(extended, 1200, 6000);
		if (verbose)
			row("120 Hz, GPU at 97%", SIM_EXTENDED, extended, 1200);
		CHECK(result.engaged == result.frames && result.missed == 0 && result.late == 0);
		finish(extended);
	}
	/* ---- a spike: one frame's work takes 6 ms longer. One frame is shown late, late starts stop for a second and
	come back without another */
	{
		struct load load = { .cpu = 0.002, .gpu_fixed = 0.003, .cpu_noise = 0.0003, .spike_frame = 3000, .spike_cpu = 0.006 };
		struct sim *extended = run(SIM_EXTENDED, P120, P120, 1.0, 2, &load, 6000);
		struct sim_result result;
		long index, held = 0;

		sim_run(extended, 6000);
		result = sim_measure(extended, 1200, 6000);
		if (verbose)
			row("a 6 ms spike", SIM_EXTENDED, extended, 1200);
		CHECK(result.late == 1);
		CHECK(extended->extended.misses >= 1 && extended->extended.misses <= 2);
		for (index = 3000; index < 6000; index++)
			held += extended->frames[index].state == FRAME_PACER_START_HELD;
		CHECK(held >= 100 && held <= 140);
		result = sim_measure(extended, 3400, 6000);
		CHECK(result.engaged == result.frames && result.missed == 0);
		finish(extended);
	}
	/* ---- dynamic resolution keeps dropping and climbing. The GPU's load rises by half over two seconds, stays, and
	falls back: with the extended late start the picture is lowered as far as without it, no more frames are late
	than without it plus a few, and the climb back (each step up costs more than the window's worst) does not stop
	late starts */
	{
		struct load load = { .cpu = 0.003, .gpu_fixed = 0.0015, .gpu_pixels = 0.0052, .cpu_noise = 0.0005,
			.gpu_noise = 0.0003, .rise_from = 5.0, .rise_to = 7.0, .fall_from = 15.0, .fall_to = 17.0, .risen = 1.5 };
		struct sim *sims[2];
		struct sim_result results[2], climb;
		int mode;
		long index, first_climb = 0, held_in_climb = 0, steps_up = 0;

		for (mode = 0; mode < 2; mode++)
		{
			struct load copy = load;
			struct sim *sim = sims[mode] = run(mode ? SIM_EXTENDED : SIM_OFF, P120, P120, 1.0, 2, &copy, 6000);

			sim->drs_width = 1600;
			sim->drs_height = 1200;
			sim->drs_floor = 600;
			dynamic_resolution_init(&sim->plan, sim->drs_width, sim->drs_height, sim->drs_floor, 0.0, 0.0);
			copy.full_pixels = sim->plan.pixels[0];
			sim_run(sim, 6000);
			results[mode] = sim_measure(sim, 240, sim->count);
			if (verbose)
			{
				row("dynamic resolution, load x1.5", mode ? SIM_EXTENDED : SIM_OFF, sim, 240);
				printf("  lowest level %d, last level %d, jumps %lu, held %lu, no fit %lu\n", results[mode].level_low,
					results[mode].level_last, sim->extended.jumps, sim->extended.held, sim->extended.no_fit);
			}
		}
		CHECK(results[0].level_low > 0);
		CHECK(results[1].level_low >= results[0].level_low);
		CHECK(results[1].late <= results[0].late + 6);
		CHECK(results[0].level_last == 0 && results[1].level_last == 0);
		/* the climb: from the first step up after the load fell */
		for (index = 1; index < sims[1]->count; index++)
		{
			const struct sim_frame *frame = &sims[1]->frames[index];

			if (frame->read - 100.0 < 17.0)
				continue;
			if (frame->level < sims[1]->frames[index - 1].level)
			{
				steps_up++;
				if (!first_climb)
					first_climb = index;
			}
			if (first_climb)
				held_in_climb += frame->state == FRAME_PACER_START_HELD;
		}
		CHECK(steps_up >= 3);
		CHECK(first_climb && held_in_climb == 0);
		climb = sim_measure(sims[1], first_climb, sims[1]->count);
		CHECK(climb.missed == 0 && climb.late == 0);
		CHECK(climb.engaged > climb.frames * 9 / 10);
		finish(sims[0]);
		finish(sims[1]);
	}
	/* ---- below its rate: the GPU needs 1.2 refreshes and there is nothing lower to drop to. No late start can
	help, and trying must not cost frames: none starts late, and the frame rate is the same as without */
	{
		struct load load = { .cpu = 0.003, .gpu_fixed = 0.010, .cpu_noise = 0.0005, .gpu_noise = 0.0005 };
		struct sim *off = run(SIM_OFF, P120, P120, 1.0, 2, &load, 4000), *extended;
		struct sim_result before, after;

		sim_run(off, 4000);
		before = sim_measure(off, 600, 4000);
		extended = run(SIM_EXTENDED, P120, P120, 1.0, 2, &load, 4000);
		sim_run(extended, 4000);
		after = sim_measure(extended, 600, 4000);
		CHECK(after.engaged == 0 && extended->extended.misses == 0);
		CHECK(fabs(after.rate - before.rate) < 0.01 && after.late == before.late);
		finish(off);
		finish(extended);
	}
	/* ---- a 30 cap on a 60 Hz link (display.frame_rate = 30, Low Power Mode): a frame's slot is every second
	refresh, in the phase of the frames before it */
	{
		struct load load = { .cpu = 0.008, .gpu_fixed = 0.014, .cpu_noise = 0.001, .gpu_noise = 0.001 };
		struct sim *off = run(SIM_OFF, P60, P60, 2.0, 2, &load, 3000), *extended;
		struct sim_result before, after;
		long index;

		sim_run(off, 3000);
		before = sim_measure(off, 600, 3000);
		extended = run(SIM_EXTENDED, P60, P60, 2.0, 2, &load, 3000);
		sim_run(extended, 3000);
		after = sim_measure(extended, 600, 3000);
		if (verbose)
			row("30 cap on a 60 Hz link", SIM_EXTENDED, extended, 600);
		CHECK(fabs(before.read_to_shown_median - 3.0 * 2.0 * P60) < 0.002);
		CHECK(after.engaged == after.frames && after.missed == 0 && after.late == 0 && after.waited == 0);
		CHECK(after.read_to_shown_p95 < 0.009 + 0.015 + FRAME_PACER_EXTENDED_LEAD + 0.001);
		for (index = 600; index < 3000; index++)
			CHECK(fabs(extended->frames[index].interval - 2.0 * P60) < 1e-6);
		finish(off);
		finish(extended);
	}
	/* ---- a scene class's 30 on a 60 Hz link: without an aim Present waits for the frame's refreshes
	(frame_context_wait), after the input read; a frame with an aim has done its waiting before the read */
	{
		struct load load = { .cpu = 0.008, .gpu_fixed = 0.020, .cpu_noise = 0.001, .gpu_noise = 0.001 };
		struct sim *off = run(SIM_OFF, P60, P60, 2.0, 2, &load, 3000), *extended;
		struct sim_result before, after;

		off->context_wait = 1;
		sim_run(off, 3000);
		before = sim_measure(off, 600, 3000);
		extended = run(SIM_EXTENDED, P60, P60, 2.0, 2, &load, 3000);
		extended->context_wait = 1;
		sim_run(extended, 3000);
		after = sim_measure(extended, 600, 3000);
		if (verbose)
		{
			row("scene class 30", SIM_OFF, off, 600);
			row("scene class 30", SIM_EXTENDED, extended, 600);
		}
		CHECK(before.waited == before.frames);
		CHECK(after.engaged == after.frames && after.missed == 0 && after.late == 0 && after.waited == 0);
		CHECK(after.read_to_shown_median < before.read_to_shown_median - 0.010);
		CHECK(fabs(after.rate - 30.0) < 0.1);
		finish(off);
		finish(extended);
	}
	/* ---- the system needs 5 ms between the GPU's end and the refresh, more than the lead starts at: a few frames
	are shown late while the lead learns it, then none, and the slow shrink does not go back under it */
	{
		struct load load = { .cpu = 0.004, .gpu_fixed = 0.004, .cpu_noise = 0.0003 };
		struct sim *extended = run(SIM_EXTENDED, P60, P60, 1.0, 2, &load, 30000);
		struct sim_result result;

		extended->latch = 0.005;
		sim_run(extended, 30000);
		result = sim_measure(extended, 3000, 30000);
		if (verbose)
			row("a 5 ms latch", SIM_EXTENDED, extended, 3000);
		if (verbose)
			printf("  misses %lu pushed %lu failed %.1f\n", extended->extended.misses, extended->extended.pushed, extended->extended.lead_failed * 1e3);
		CHECK(extended->extended.misses >= 1 && extended->extended.misses <= FRAME_PACER_EXTENDED_LEAD_MISSES);
		CHECK(result.missed == 0 && result.late == 0);
		CHECK(frame_pacer_extended_lead(&extended->extended) >= 0.005);
		CHECK(frame_pacer_extended_lead(&extended->extended) <= 0.0075);
		finish(extended);
	}
	/* ---- one frame in flight: the frame may not reach its Present before the frame before is shown, so only a
	budget that leaves the CPU's part inside one slot fits */
	{
		struct load heavy = { .cpu = 0.003, .gpu_fixed = 0.007 }, light = { .cpu = 0.003, .gpu_fixed = 0.002 };
		struct sim *sim = run(SIM_EXTENDED, P120, P120, 1.0, 1, &heavy, 3000);
		struct sim_result result;

		sim_run(sim, 3000);
		result = sim_measure(sim, 600, 3000);
		CHECK(result.engaged == 0);
		finish(sim);
		sim = run(SIM_EXTENDED, P120, P120, 1.0, 1, &light, 3000);
		sim_run(sim, 3000);
		result = sim_measure(sim, 600, 3000);
		if (verbose)
			row("one in flight, light", SIM_EXTENDED, sim, 600);
		/* (the lead does not shrink under the one it starts from, and with it this cost does not fit one slot: frames
		start at once, as with no late start) */
		CHECK(result.engaged == 0 && result.missed == 0);
		finish(sim);
	}
	/* ---- the CPU's work over a slot, on every frame or on the frames that run a game tick: no start can help, and
	the frame after a long one must not sleep toward a later slot. The frame rate and the late frames are those
	without a late start */
	{
		static const struct load loads[] = {
			{ .cpu = 0.009, .gpu_fixed = 0.003 },
			{ .cpu = 0.008, .gpu_fixed = 0.003, .cpu_noise = 0.0008 },
			{ .cpu = 0.0055, .gpu_fixed = 0.004, .tick_every = 4, .tick_cpu = 0.0035 },
			{ .cpu = 0.0055, .gpu_fixed = 0.004, .tick_every = 2, .tick_cpu = 0.0035 },
		};
		unsigned index;

		for (index = 0; index < sizeof(loads) / sizeof(loads[0]); index++)
		{
			struct load load = loads[index];
			struct sim *off = run(SIM_OFF, P120, P120, 1.0, 2, &load, 6000), *extended;
			struct sim_result before, after;

			sim_run(off, 6000);
			before = sim_measure(off, 1200, 6000);
			load = loads[index];
			extended = run(SIM_EXTENDED, P120, P120, 1.0, 2, &load, 6000);
			sim_run(extended, 6000);
			after = sim_measure(extended, 1200, 6000);
			if (verbose)
			{
				row("CPU over a slot", SIM_OFF, off, 1200);
				row("CPU over a slot", SIM_EXTENDED, extended, 1200);
			}
			CHECK(after.rate > before.rate - 0.5);
			CHECK(after.late <= before.late);
			finish(off);
			finish(extended);
		}
	}
	/* ---- a shown notice that never comes: the count of frames in flight is made up by the next notice, and no slot
	is skipped afterward */
	{
		struct load load = { .cpu = 0.003, .gpu_fixed = 0.007, .cpu_noise = 0.0005, .gpu_noise = 0.0003 };
		struct sim *extended = run(SIM_EXTENDED, P120, P120, 1.0, 2, &load, 6000);
		struct sim_result result;

		extended->lose_notice = 3000;
		sim_run(extended, 6000);
		result = sim_measure(extended, 3010, 6000);
		if (verbose)
			row("a lost shown notice", SIM_EXTENDED, extended, 3010);
		CHECK(result.late == 0 && fabs(result.rate - 120.0) < 0.1);
		CHECK(result.engaged == result.frames && result.missed == 0);
		finish(extended);
	}
	/* ---- a spike every 1,500 frames: each is one frame shown late, counted as that (not as pushed), and the lead,
	which the spikes say nothing about, stays where the system's need puts it */
	{
		struct load load = { .cpu = 0.003, .gpu_fixed = 0.004, .cpu_noise = 0.0003, .spike_frame = 1500, .spike_every = 1500,
			.spike_cpu = 0.006 };
		struct sim *extended = run(SIM_EXTENDED, P120, P120, 1.0, 2, &load, 15000);
		struct sim_result result;

		sim_run(extended, 15000);
		result = sim_measure(extended, 1200, 15000);
		if (verbose)
		{
			row("a spike every 1,500 frames", SIM_EXTENDED, extended, 1200);
			printf("  misses %lu pushed %lu jumps %lu failed %.1f\n", extended->extended.misses, extended->extended.pushed,
				extended->extended.jumps, extended->extended.lead_failed * 1e3);
		}
		CHECK(extended->extended.misses >= 8 && extended->extended.misses <= 10);
		CHECK(result.late <= 10);
		CHECK(extended->extended.lead_failed == 0.0);
		CHECK(frame_pacer_extended_lead(&extended->extended) <= FRAME_PACER_EXTENDED_LEAD + FRAME_PACER_LATE_LEAD_STEP);
		finish(extended);
	}
	/* ---- the display shows one frame in 200 a refresh late by itself, and frames that start at once report three
	quarters of their work (both measured on a 120 Hz phone): late starts stay on, and the lead does not grow */
	{
		struct load load = { .cpu = 0.0035, .gpu_fixed = 0.0038, .cpu_noise = 0.0003, .gpu_noise = 0.0003 };
		struct load load_off = load;
		struct sim *off = run(SIM_OFF, P120, P120, 1.0, 2, &load_off, 24000);
		struct sim *extended = run(SIM_EXTENDED, P120, P120, 1.0, 2, &load, 24000);
		struct sim_result result, baseline;

		off->latch = extended->latch = 0.0045;
		off->slip_every = extended->slip_every = 200;
		extended->unaimed_cost = 0.75;
		sim_run(off, 24000);
		sim_run(extended, 24000);
		baseline = sim_measure(off, 1200, 24000);
		result = sim_measure(extended, 1200, 24000);
		if (verbose)
			printf("slips: engaged %ld of %ld, late %ld (off %ld), misses %lu pushed %lu lead %.1f failed %.1f read %.1f (off %.1f)\n",
				result.engaged, result.frames, result.late, baseline.late, extended->extended.misses,
				extended->extended.pushed, frame_pacer_extended_lead(&extended->extended) * 1e3,
				extended->extended.lead_failed * 1e3, result.read_to_shown_median * 1e3, baseline.read_to_shown_median * 1e3);
		CHECK(result.engaged > result.frames * 95 / 100);
		CHECK(result.late <= baseline.late + 8);
		CHECK(frame_pacer_extended_lead(&extended->extended) <= 0.0075);
		CHECK(result.read_to_shown_median < baseline.read_to_shown_median - 0.008);
		finish(off);
		finish(extended);
	}
	/* ---- display.late_start = "extended" acts at 60 frames a second and under: a game at the rate of a 120 Hz
	panel is left exactly as with no late start, a 60 cap on that panel, a 60 Hz display and a 30 cap start late */
	{
		static const struct { double panel, link, share; int aimed; } rates[] =
		{
			{ P120, P120, 1.0, 0 }, { P120, P60, 1.0, 1 }, { P60, P60, 1.0, 1 }, { P60, P60, 2.0, 1 }, { P120, P120, 2.0, 1 },
		};
		int index;

		for (index = 0; index < (int)(sizeof(rates) / sizeof(rates[0])); index++)
		{
			struct load load = { .cpu = 0.003, .gpu_fixed = 0.004, .cpu_noise = 0.0003, .gpu_noise = 0.0003 }, load_off = load;
			struct sim *off = run(SIM_OFF, rates[index].panel, rates[index].link, rates[index].share, 2, &load_off, 3000);
			struct sim *extended = run(SIM_EXTENDED, rates[index].panel, rates[index].link, rates[index].share, 2, &load, 3000);
			struct sim_result result, baseline;

			extended->slot_least = FRAME_PACER_EXTENDED_SLOT;
			sim_run(off, 3000);
			sim_run(extended, 3000);
			baseline = sim_measure(off, 600, 3000);
			result = sim_measure(extended, 600, 3000);
			if (rates[index].aimed)
				CHECK(result.engaged > result.frames * 99 / 100 && result.read_to_shown_median < 0.6 * baseline.read_to_shown_median);
			else
				CHECK(result.engaged == 0 && result.read_to_shown_median == baseline.read_to_shown_median &&
					result.late == baseline.late && result.waited_mean == baseline.waited_mean);
			finish(off);
			finish(extended);
		}
	}
	/* ---- a 60 Hz display that needs a frame a little earlier than the starting lead (5.2 to 7 ms against 5 ms): the
	lead grows once, to the lead that failed plus the margin, late starts stay on, and the only late frames left are
	the display's own (one in 1,000 here) */
	{
		static const double latches[] = { 0.0052, 0.0055, 0.007 };
		int index;

		for (index = 0; index < (int)(sizeof(latches) / sizeof(latches[0])); index++)
		{
			struct load load = { .cpu = 0.004, .gpu_fixed = 0.004, .cpu_noise = 0.001, .gpu_noise = 0.001 }, load_off = load;
			struct sim *off = run(SIM_OFF, P60, P60, 1.0, 2, &load_off, 36000);
			struct sim *extended = run(SIM_EXTENDED, P60, P60, 1.0, 2, &load, 36000);
			struct sim_result result, baseline;
			double lead;

			off->latch = extended->latch = latches[index];
			off->slip_every = extended->slip_every = 1000;
			extended->slot_least = FRAME_PACER_EXTENDED_SLOT;
			sim_run(off, 36000);
			sim_run(extended, 36000);
			baseline = sim_measure(off, 3600, 36000);
			result = sim_measure(extended, 3600, 36000);
			lead = frame_pacer_extended_lead(&extended->extended);
			if (verbose)
				printf("60 Hz latch %.1f: engaged %ld of %ld, late %ld (off %ld), lead %.1f failed %.1f read %.1f (off %.1f)\n",
					latches[index] * 1e3, result.engaged, result.frames, result.late, baseline.late, lead * 1e3,
					extended->extended.lead_failed * 1e3, result.read_to_shown_median * 1e3, baseline.read_to_shown_median * 1e3);
			CHECK(lead >= latches[index] && lead <= latches[index] + FRAME_PACER_LATE_LEAD_STEP);
			CHECK(fabs(lead - (extended->extended.lead_failed + FRAME_PACER_EXTENDED_LEAD_MARGIN)) < 1e-9);
			CHECK(result.engaged == result.frames);
			CHECK(result.late <= result.frames / 1000);
			CHECK(result.read_to_shown_median < 0.5 * baseline.read_to_shown_median);
			finish(off);
			finish(extended);
		}
	}
	/* ---- the heavy 60 class on a 120 Hz panel (7 ms of game, 15 ms of GPU: 22 of the 33.3 ms between the start of
	one frame and the refresh two slots on): every frame is aimed, the rate holds at 60, and the display's own late
	frames are the only ones */
	{
		struct load load = { .cpu = 0.007, .gpu_fixed = 0.015, .cpu_noise = 0.0008, .gpu_noise = 0.0008 }, load_off = load;
		struct sim *off = run(SIM_OFF, P120, P120, 2.0, 2, &load_off, 36000);
		struct sim *extended = run(SIM_EXTENDED, P120, P120, 2.0, 2, &load, 36000);
		struct sim_result result, baseline;

		off->latch = extended->latch = 0.0045;
		off->slip_every = extended->slip_every = 1000;
		extended->unaimed_cost = 0.75;
		extended->slot_least = FRAME_PACER_EXTENDED_SLOT;
		sim_run(off, 36000);
		sim_run(extended, 36000);
		baseline = sim_measure(off, 3600, 36000);
		result = sim_measure(extended, 3600, 36000);
		if (verbose)
			printf("heavy 60 class: engaged %ld of %ld, late %ld (off %ld), lead %.1f read %.1f (off %.1f) fps %.1f\n",
				result.engaged, result.frames, result.late, baseline.late, frame_pacer_extended_lead(&extended->extended) * 1e3,
				result.read_to_shown_median * 1e3, baseline.read_to_shown_median * 1e3, result.rate);
		CHECK(result.engaged == result.frames && fabs(result.rate - 60.0) < 0.1);
		CHECK(result.late <= result.frames / 1000);
		CHECK(fabs(frame_pacer_extended_lead(&extended->extended) - FRAME_PACER_EXTENDED_LEAD) < 1e-12);
		CHECK(result.read_to_shown_median < 0.65 * baseline.read_to_shown_median);
		finish(off);
		finish(extended);
	}
	/* ---- the rules alone */
	{
		struct frame_pacer_extended late;
		struct frame_pacer_extended_input in = { 10.0, 10.0, P60, P120, 1.0, 2, 0, 0.0, 0.0 };
		struct frame_pacer_extended_start start;
		int index;

		memset(&late, 0, sizeof(late));
		/* too little history */
		start = frame_pacer_extended_wait(&late, &in);
		CHECK(start.state == FRAME_PACER_START_NO_FIT && start.wait == 0.0 && start.aim == 0.0);
		for (index = 0; index < FRAME_PACER_LATE_WINDOW; index++)
			frame_pacer_extended_cost(&late, 0.010, 0.006, 0.004, 0.0, 9.0);
		/* nothing shown lately: the first refresh of the panel the frame can make */
		in.now = 10.001;
		start = frame_pacer_extended_wait(&late, &in);
		CHECK(start.state == FRAME_PACER_START_ENGAGED && fabs(start.aim - (10.0 + 2.0 * P120)) < 1e-9);
		CHECK(fabs(start.wait - (start.aim - 0.010 - FRAME_PACER_EXTENDED_LEAD - in.now)) < 1e-9 && fabs(start.allowed - 0.010) < 1e-12);
		/* shown on a panel refresh between the link's (a 60 cap on a 120 Hz panel): a frame one panel refresh late
		is late, though that is half a link period. One late frame alone stops nothing */
		frame_pacer_extended_shown(&late, start.aim, start.aim + P120, 0.001, frame_pacer_extended_step(P60, P120), P60,
			10.03);
		CHECK(late.misses == 1 && late.holdoff_until == 0.0);
		in.now = 10.04;
		CHECK(frame_pacer_extended_wait(&late, &in).state == FRAME_PACER_START_ENGAGED);
		/* the frames behind it, pushed late with it, are the same event */
		frame_pacer_extended_shown(&late, 10.04, 10.04 + P60, 0.006, P120, P60, 10.03 + 3.0 * P60);
		CHECK(late.misses == 1 && late.pushed == 1 && late.holdoff_until == 0.0);
		/* a second frame late for its cost within a second stops late starts. (Their GPU work ended under a lead
		before their aims: they cost too much, and the lead is not marked too short.) */
		frame_pacer_extended_shown(&late, 10.4, 10.4 + P60, 0.001, P120, P60, 10.5);
		CHECK(late.misses == 2 && late.holdoff == FRAME_PACER_EXTENDED_HOLDOFF && fabs(late.holdoff_until - 11.5) < 1e-9);
		CHECK(late.lead_failed == 0.0 && fabs(frame_pacer_extended_lead(&late) - FRAME_PACER_EXTENDED_LEAD) < 1e-12);
		in.now = 10.6;
		CHECK(frame_pacer_extended_wait(&late, &in).state == FRAME_PACER_START_HELD);
		/* (a frame shown late while they are stopped changes nothing; nor does a load) */
		frame_pacer_extended_shown(&late, 10.5, 10.5 + P60, 0.006, P120, P60, 10.9);
		frame_pacer_extended_shown(&late, 10.5, 10.5 + 0.3, 0.006, P120, P60, 10.9);
		CHECK(late.misses == 2 && late.pushed == 2);
		/* frames shown late whose GPU end is not known say nothing about the lead: however many, it does not grow */
		{
			struct frame_pacer_extended unknown;
			int late_frames;

			memset(&unknown, 0, sizeof(unknown));
			for (late_frames = 0; late_frames < 3 * FRAME_PACER_EXTENDED_LEAD_MISSES; late_frames++)
				frame_pacer_extended_shown(&unknown, 50.0 + 2.0 * late_frames, 50.0 + 2.0 * late_frames + P60, (double)NAN,
					P120, P60, 50.1 + 2.0 * late_frames);
			CHECK(unknown.misses == 3 * FRAME_PACER_EXTENDED_LEAD_MISSES && unknown.lead_misses == 0);
			CHECK(unknown.lead_failed == 0.0 && fabs(frame_pacer_extended_lead(&unknown) - FRAME_PACER_EXTENDED_LEAD) < 1e-12);
		}
		/* each frame's GPU sum by its slot's tag: a late buffer of a frame whose slot went to the frame eight later is
		counted toward nothing, and a take for a frame that never added is 0 */
		{
			_Atomic uint64_t slot;
			unsigned long frame = 13, later = frame + FRAME_PACER_EXTENDED_FRAMES;

			atomic_init(&slot, 0);
			frame_pacer_gpu_add(&slot, frame, 3000000);
			frame_pacer_gpu_add(&slot, frame, 2000000);
			CHECK(frame_pacer_gpu_take(&slot, frame) == 5000000 && frame_pacer_gpu_take(&slot, frame) == 0);
			frame_pacer_gpu_add(&slot, frame, 1000000);
			frame_pacer_gpu_add(&slot, later, 4000000);
			frame_pacer_gpu_add(&slot, frame, 7000000);
			CHECK(frame_pacer_gpu_take(&slot, frame) == 0 && frame_pacer_gpu_take(&slot, later) == 4000000);
			CHECK(frame_pacer_gpu_take(&slot, later + FRAME_PACER_EXTENDED_FRAMES) == 0);
			frame_pacer_gpu_add(&slot, frame + 2 * FRAME_PACER_EXTENDED_FRAMES, 0xffffffffu);
			frame_pacer_gpu_add(&slot, frame + 2 * FRAME_PACER_EXTENDED_FRAMES, 5);
			CHECK(frame_pacer_gpu_take(&slot, frame + 2 * FRAME_PACER_EXTENDED_FRAMES) == 0xffffffffu);
			/* the frame number's low 32 bits wrap: a late buffer of the frame before the wrap is still dropped */
			{
				unsigned long before = 0xfffffff8ul, after = before + FRAME_PACER_EXTENDED_FRAMES;

				atomic_init(&slot, 0);
				frame_pacer_gpu_add(&slot, before, 1000000);
				frame_pacer_gpu_add(&slot, after, 6000000);
				frame_pacer_gpu_add(&slot, before, 9000000);
				CHECK(frame_pacer_gpu_take(&slot, after) == 6000000 && frame_pacer_gpu_take(&slot, before) == 0);
			}
		}
		/* the frames between Present and shown, by number (what the host keeps): a place goes to the frame
		FRAME_PACER_EXTENDED_FRAMES later, after which the earlier frame's reports are a frame that is gone */
		{
			struct frame_pacer_extended rules;
			struct frame_pacer_extended_frames frames;
			unsigned long first = 5, wrapped = first + FRAME_PACER_EXTENDED_FRAMES;

			memset(&rules, 0, sizeof(rules));
			memset(&frames, 0, sizeof(frames));
			CHECK(!frame_pacer_extended_frames_find(&frames, first) && !frame_pacer_extended_frames_find(&frames, 0));
			/* (a report for a frame never added counts nothing) */
			frame_pacer_extended_frame_finished(&rules, frame_pacer_extended_frames_find(&frames, first), 60.008, 0.004, 0.0,
				60.008);
			CHECK(rules.count == 0);
			frame_pacer_extended_frames_add(&frames, first, 60.0, 0.003, 60.0 + P60, 0.008);
			CHECK(frame_pacer_extended_frames_find(&frames, first) && !frame_pacer_extended_frames_find(&frames, wrapped));
			/* the shown notice before the GPU's report (the handlers are on different threads): the frame was late, and
			with its GPU end unknown that says nothing about the lead */
			frame_pacer_extended_frame_shown(&rules, frame_pacer_extended_frames_find(&frames, first), 60.0 + P60,
				60.0 + 2.0 * P60, P60, P60, 1.0, 60.04);
			CHECK(rules.misses == 1 && rules.lead_misses == 0);
			/* the report still counts the frame's work: 8 ms from its start, less a millisecond waiting for the display */
			frame_pacer_extended_frame_finished(&rules, frame_pacer_extended_frames_find(&frames, first), 60.008, 0.004, 0.001,
				60.05);
			CHECK(rules.count == 1 && fabs(rules.work[0] - 0.007) < 1e-12 && fabs(rules.gpu[0] - 0.004) < 1e-12 &&
				fabs(rules.cpu[0] - 0.003) < 1e-12);
			CHECK(fabs(frame_pacer_extended_frames_find(&frames, first)->gpu_end - 60.008) < 1e-12);
			/* a frame shown late whose GPU ended a full lead before its aim is the lead's failure */
			frame_pacer_extended_frame_shown(&rules, frame_pacer_extended_frames_find(&frames, first), 60.008 + 0.006,
				60.008 + 0.006 + P60, P60, P60, 1.0, 62.0);
			CHECK(rules.misses == 2 && rules.lead_misses == 1);
			/* its place taken by the frame eight later, its own reports find nothing */
			frame_pacer_extended_frames_add(&frames, wrapped, 61.0, 0.003, 0.0, 0.0);
			CHECK(!frame_pacer_extended_frames_find(&frames, first));
			CHECK(frame_pacer_extended_frames_find(&frames, wrapped)->began == 61.0 &&
				frame_pacer_extended_frames_find(&frames, wrapped)->gpu_end == 0.0);
			frame_pacer_extended_frame_finished(&rules, frame_pacer_extended_frames_find(&frames, first), 61.5, 0.004, 0.0, 61.5);
			CHECK(rules.count == 1);
		}
		/* the frames in flight the rules are asked about: those handed to the display less the highest shown */
		{
			struct frame_pacer_extended_input asked = frame_pacer_extended_asked(70.0, 69.99, P120, P120, 2.0, 2, 1002, 1000,
				69.98, FRAME_PACER_EXTENDED_SLOT);

			CHECK(asked.in_flight == 2 && asked.limit == 2 && asked.share == 2.0 && asked.shown_last == 69.98 &&
				asked.slot_least == FRAME_PACER_EXTENDED_SLOT && asked.now == 70.0 && asked.refresh == 69.99);
		}
		/* frames shown late with their GPU work done a lead ahead: a few among many aimed frames are the display's
		own, and nothing stops */
		late.holdoff_until = 0.0;
		late.missed = 0;
		for (index = 0; index < FRAME_PACER_EXTENDED_LEAD_MISSES - 1; index++)
			frame_pacer_extended_shown(&late, 12.0 + index, 12.0 + index + P60, 0.0051, P120, P60, 12.1 + index);
		CHECK(late.misses == 2 + FRAME_PACER_EXTENDED_LEAD_MISSES - 1 && late.holdoff_until == 0.0 && late.lead_failed == 0.0);
		for (index = 0; index < FRAME_PACER_EXTENDED_LEAD_WINDOW; index++)
			frame_pacer_extended_shown(&late, 30.0, 30.0, 0.0051, P120, P60, 30.1);
		frame_pacer_extended_shown(&late, 31.0, 31.0 + P60, 0.0051, P120, P60, 31.1);
		CHECK(late.holdoff_until == 0.0 && late.lead_failed == 0.0 && late.lead_misses == 1);
		/* a ready frame shown half a panel refresh after its aim (a phone at 60 on its 120 Hz panel: 4.173 ms against
		the 4.167 ms line for a late frame): the display's timing, however many there are. They are not lead misses,
		nothing stops and the lead stays; a whole refresh late with the same slack still counts */
		{
			struct frame_pacer_extended wobble = { 0 };

			for (index = 0; index < 4 * FRAME_PACER_EXTENDED_LEAD_MISSES; index++)
				frame_pacer_extended_shown(&wobble, 40.0 + index, 40.0 + index + 0.004173, 0.013, P120, P60, 40.1 + index);
			CHECK(wobble.misses == 4 * FRAME_PACER_EXTENDED_LEAD_MISSES && wobble.wobbles == wobble.misses);
			CHECK(wobble.lead_misses == 0 && wobble.lead_failed == 0.0 && wobble.holdoff_until == 0.0 && !wobble.missed);
			CHECK(frame_pacer_extended_lead(&wobble) == FRAME_PACER_EXTENDED_LEAD);
			frame_pacer_extended_shown(&wobble, 90.0, 90.0 + 0.004160, 0.013, P120, P60, 90.1);
			CHECK(wobble.misses == 4 * FRAME_PACER_EXTENDED_LEAD_MISSES);
			frame_pacer_extended_shown(&wobble, 91.0, 91.0 + P120, 0.013, P120, P60, 91.1);
			CHECK(wobble.lead_misses == 1 && wobble.wobbles == 4 * FRAME_PACER_EXTENDED_LEAD_MISSES);
		}
		/* many among few aimed frames: the lead is too short. Late starts stop, the lead grows, and the shrink stops a
		margin over the one that failed */
		for (index = 0; index < FRAME_PACER_EXTENDED_LEAD_MISSES - 1; index++)
			frame_pacer_extended_shown(&late, 32.0 + index, 32.0 + index + P60, 0.0051, P120, P60, 32.1 + index);
		CHECK(late.holdoff == FRAME_PACER_EXTENDED_HOLDOFF && late.holdoff_until > 32.0);
		CHECK(fabs(late.lead_failed - 0.005) < 1e-12 && fabs(frame_pacer_extended_lead(&late) - 0.007) < 1e-12);
		for (index = 0; index < 8; index++)
			frame_pacer_extended_shrink(&late);
		CHECK(fabs(frame_pacer_extended_lead(&late) - (0.005 + FRAME_PACER_EXTENDED_LEAD_MARGIN)) < 1e-12);
		late.lead = late.lead_failed = 0.0;
		/* frames with a late start measured more than the ones that start at once: the next start allows for the
		worst of the last of them */
		frame_pacer_extended_cost(&late, 0.012, 0.006, 0.004, 0.010, 12.0);
		for (index = 0; index < FRAME_PACER_LATE_WINDOW; index++)
			frame_pacer_extended_cost(&late, 0.010, 0.006, 0.004, 0.0, 12.0);
		late.holdoff_until = 0.0;
		in.now = 14.0;
		in.refresh = 14.0;
		start = frame_pacer_extended_wait(&late, &in);
		CHECK(start.state == FRAME_PACER_START_ENGAGED && fabs(start.allowed - 0.012) < 1e-12);
		memset(late.aimed, 0, sizeof(late.aimed));
		/* frames in flight own the next slots: with the last frame shown at 20.0 and two in flight, the frame's
		slot is the third after it, whatever it could make */
		late.holdoff_until = 0.0;
		in.now = 20.001;
		in.refresh = 20.0;
		in.panel = 0.0;
		in.in_flight = 2;
		in.shown_last = 20.0;
		start = frame_pacer_extended_wait(&late, &in);
		CHECK(start.state == FRAME_PACER_START_ENGAGED && fabs(start.aim - (20.0 + 3.0 * P60)) < 1e-9);
		/* the next frame's is the one after its aim, though the shown notices have not caught up */
		in.now = start.aim - 0.012;
		start = frame_pacer_extended_wait(&late, &in);
		CHECK(fabs(start.aim - (20.0 + 4.0 * P60)) < 1e-9);
		/* a share of two: every second refresh, counted from the last frame shown, and a refresh between is not
		taken even when the frame could make it */
		in.share = 2.0;
		in.in_flight = 0;
		in.shown_last = 30.0 + P60;
		in.now = 30.0 + P60 + 0.001;
		in.refresh = 30.0 + P60;
		late.last_aim = 0.0;
		start = frame_pacer_extended_wait(&late, &in);
		CHECK(fabs(start.aim - (30.0 + 3.0 * P60)) < 1e-9);
		/* frames in flight that are never shown must not hold the game: a guess over the limit's slots and one away
		starts at once */
		in.share = 1.0;
		in.in_flight = 6;
		in.limit = 0;
		in.shown_last = in.now - 0.001;
		late.last_aim = 0.0;
		start = frame_pacer_extended_wait(&late, &in);
		CHECK(start.state == FRAME_PACER_START_NO_FIT && start.wait == 0.0);
		/* with a limit, no more frames count as in flight than it lets be: a count that is wrong cannot push the aim
		past the limit's slots */
		in.limit = 2;
		late.last_aim = 0.0;
		start = frame_pacer_extended_wait(&late, &in);
		CHECK(start.state == FRAME_PACER_START_ENGAGED && start.aim < in.shown_last + 3.5 * P60);
		/* behind: with the last slot spoken for a refresh ago and a budget of 13 ms, the frame cannot make the slot
		after it. It does not sleep toward a later one: it starts at once with no aim */
		in.in_flight = 0;
		in.shown_last = in.now - 0.010;
		late.last_aim = 0.0;
		start = frame_pacer_extended_wait(&late, &in);
		CHECK(start.state == FRAME_PACER_START_NO_FIT && start.wait == 0.0 && start.aim == 0.0 && late.behind == 1);
		/* an 80 Hz link on a 120 Hz panel: the link's refreshes are not all the panel's, so the grid is the link's */
		CHECK(frame_pacer_extended_step(1.0 / 80.0, P120) == 1.0 / 80.0);
		CHECK(frame_pacer_extended_step(P60, P120) == P120 && frame_pacer_extended_step(P120, P120) == P120);
		/* more pixels: the GPU's part of every cost in the window grows by the ratio; fewer change nothing */
		frame_pacer_extended_scale(&late, 1.5);
		CHECK(fabs(frame_pacer_extended_worst(late.gpu, late.count) - 0.009) < 1e-12);
		CHECK(fabs(frame_pacer_extended_worst(late.work, late.count) - 0.013) < 1e-12);
		frame_pacer_extended_scale(&late, 0.5);
		CHECK(fabs(frame_pacer_extended_worst(late.gpu, late.count) - 0.009) < 1e-12);
		/* the GPU over a slot: no fit */
		frame_pacer_extended_cost(&late, 0.022, 0.017, 0.004, 0.0, 31.0);
		in.in_flight = 0;
		CHECK(frame_pacer_extended_wait(&late, &in).state == FRAME_PACER_START_NO_FIT);
		/* a cost over what was allowed by less than half the lead is not a jump; by more, it is */
		{
			struct frame_pacer_extended jumps;

			memset(&jumps, 0, sizeof(jumps));
			frame_pacer_extended_cost(&jumps, 0.0074, 0.003, 0.003, 0.005, 40.0);
			CHECK(jumps.jumps == 0 && jumps.holdoff_until == 0.0);
			frame_pacer_extended_cost(&jumps, 0.0076, 0.003, 0.003, 0.005, 40.0);
			CHECK(jumps.jumps == 1 && fabs(jumps.holdoff_until - 41.0) < 1e-9);
			/* a load is not a frame */
			frame_pacer_extended_cost(&jumps, 0.5, 0.003, 0.003, 0.005, 40.0);
			CHECK(jumps.count == 2);
		}
	}
	if (failures)
		return 1;
	printf("frame_pacer_sim_probe: ok\n");
	return 0;
}
