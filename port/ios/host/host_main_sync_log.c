/*
HOST_MAIN_SYNC_LOG.C

debug.main_sync_log (host_main_sync_log.h). The waits are counted by
host_main_thread.c for each thread; at each present this thread's counts
are taken, so a frame's figure is everything the presenting thread waited on
main since the previous present.
*/

#include "host_main_sync_log.h"
#include "host_config.h"
#include "host_main_thread.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

enum { WINDOW_FRAMES = 300 };

static int enabled;
static unsigned frames;
static uint64_t window_started, longest;
static double waited[WINDOW_FRAMES], trips[WINDOW_FRAMES];

void host_main_sync_log_install(void)
{
	char value[16];

	host_config_string("debug.main_sync_log", "false", value, sizeof(value));
	enabled = !strcmp(value, "true");
	frames = 0;
	window_started = 0;
	longest = 0;
	if (enabled)
	{
		struct host_main_sync_waits discarded;

		host_main_sync_take(&discarded);
		platform_log("main sync: logging the presenting thread's waits on main every %d frames (debug.main_sync_log)",
			WINDOW_FRAMES);
	}
}

static int ascending(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;

	return (x > y) - (x < y);
}

/* the value at fraction of the sorted values (nearest rank) */
static double rank(const double *sorted, unsigned count, double fraction)
{
	unsigned index = (unsigned)(fraction * (double)count + 0.5);

	return sorted[index ? (index > count ? count : index) - 1 : 0];
}

void host_main_sync_log_frame(void)
{
	struct host_main_sync_waits waits;
	uint64_t now;
	double total = 0.0, wall;

	if (!enabled)
		return;
	host_main_sync_take(&waits);
	now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
	if (!window_started)
	{
		/* (the first present opens the window: what came before it is loading) */
		window_started = now;
		return;
	}
	waited[frames] = (double)waits.nanoseconds / 1e6;
	trips[frames] = (double)waits.count;
	if (waits.longest_nanoseconds > longest)
		longest = waits.longest_nanoseconds;
	if (++frames < WINDOW_FRAMES)
		return;
	for (unsigned index = 0; index < frames; index++)
		total += waited[index];
	wall = (double)(now - window_started) / 1e6;
	qsort(waited, frames, sizeof(waited[0]), ascending);
	qsort(trips, frames, sizeof(trips[0]), ascending);
	platform_log("main sync: %u frames, blocked on main a frame: median %.3f ms, p95 %.3f ms, worst %.3f ms, mean %.3f ms "
		"(%.1f%% of wall time); round trips a frame: median %.0f, p95 %.0f, worst %.0f; longest single wait %.3f ms",
		frames, rank(waited, frames, 0.5), rank(waited, frames, 0.95), waited[frames - 1], total / (double)frames,
		wall > 0.0 ? 100.0 * total / wall : 0.0, rank(trips, frames, 0.5), rank(trips, frames, 0.95), trips[frames - 1],
		(double)longest / 1e6);
	frames = 0;
	longest = 0;
	window_started = now;
}
