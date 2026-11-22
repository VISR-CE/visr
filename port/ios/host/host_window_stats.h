/*
HOST_WINDOW_STATS.H

Summaries of a window of per-frame values for the debug logs, so every log
line in a frame-timing comparison uses the same median and 95th percentile.
*/

#ifndef __HALO_HOST_WINDOW_STATS_H
#define __HALO_HOST_WINDOW_STATS_H

/* frames in one window of the periodic debug logs (debug.thread_cpu_log, debug.main_sync_log) */
enum { HOST_LOG_WINDOW_FRAMES = 300 };

struct host_window_stats
{
	double median, high, worst, mean;
};

/* Sorts count values ascending in place. The median is values[count / 2] and
 * the 95th percentile values[count * 95 / 100]; all four are 0 for no values. */
struct host_window_stats host_window_stats(double *values, unsigned long count);

#endif
