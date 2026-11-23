/*
HOST_MAIN_SYNC_LOG.C

debug.main_sync_log (host_main_sync_log.h). The waits are counted by
host_main_thread.c for each thread; at each present this thread's counts
are taken, so a frame's figure is everything the game thread waited on
main since the previous present.
*/

#include "host_main_sync_log.h"
#include "host_config.h"
#include "host_main_thread.h"
#include "host_window_stats.h"
#include <string.h>
#include <time.h>

void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

static int enabled;
static unsigned frames;
static uint64_t window_started, longest;
static double waited[HOST_LOG_WINDOW_FRAMES], trips[HOST_LOG_WINDOW_FRAMES];

void host_main_sync_log_install(void)
{
	char value[16];

	host_config_string("debug.main_sync_log", "false", value, sizeof(value));
	enabled = !strcmp(value, "true");
	frames = 0;
	window_started = 0;
	longest = 0;
	if (enabled)
		platform_log("main sync: logging the game thread's waits on main every %d frames (debug.main_sync_log); "
			"with debug.encode_thread = \"thread\" the renderer's waits on the encode thread are not counted",
			HOST_LOG_WINDOW_FRAMES);
}

void host_main_sync_log_frame(void)
{
	struct host_main_sync_waits waits;
	uint64_t now;
	struct host_window_stats wait, trip;
	double wall;

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
	if (++frames < HOST_LOG_WINDOW_FRAMES)
		return;
	wall = (double)(now - window_started) / 1e6;
	wait = host_window_stats(waited, frames);
	trip = host_window_stats(trips, frames);
	platform_log("main sync: %u frames, blocked on main a frame: median %.3f ms, p95 %.3f ms, worst %.3f ms, mean %.3f ms "
		"(%.1f%% of wall time); round trips a frame: median %.0f, p95 %.0f, worst %.0f; longest single wait %.3f ms",
		frames, wait.median, wait.high, wait.worst, wait.mean, wall > 0.0 ? 100.0 * wait.mean * (double)frames / wall : 0.0,
		trip.median, trip.high, trip.worst, (double)longest / 1e6);
	frames = 0;
	longest = 0;
	window_started = now;
}
