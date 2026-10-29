/* The A/B window consumes the existing stereo pacing GPU samples (seconds).
   The first 60 presented frames let reallocation and delayed GPU completions settle. */
#pragma once
#include <stdlib.h>

struct host_stereo_ab_window {
	unsigned int frames;
	int active;
	double times[300];
};

static inline void host_stereo_ab_restart(struct host_stereo_ab_window *window)
{
	window->frames = 0;
	window->active = 1;
}

static inline int host_stereo_ab_compare(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;
	return x < y ? -1 : x > y;
}

/* The same upper median and percentile ranks as gpu_metal.m's pacing_percentiles. */
static inline int host_stereo_ab_add(struct host_stereo_ab_window *window, double seconds,
	double *median, double *high)
{
	if (!window->active) return 0;
	unsigned int frame = window->frames++;
	if (frame < 60) return 0;
	window->times[frame - 60] = seconds;
	if (window->frames != 360) return 0;
	window->active = 0;
	qsort(window->times, 300, sizeof(double), host_stereo_ab_compare);
	*median = window->times[150] * 1000.0;
	*high = window->times[285] * 1000.0;
	return 1;
}
