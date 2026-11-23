/*
HOST_WINDOW_STATS.C

host_window_stats.h.
*/

#include "host_window_stats.h"
#include <stdlib.h>

static int ascending(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;

	return (x > y) - (x < y);
}

struct host_window_stats host_window_stats(double *values, unsigned long count)
{
	struct host_window_stats stats = {0};
	double total = 0.0;

	if (!count)
		return stats;
	qsort(values, count, sizeof(*values), ascending);
	for (unsigned long index = 0; index < count; index++)
		total += values[index];
	stats.median = values[count / 2];
	stats.high = values[count * 95 / 100];
	stats.worst = values[count - 1];
	stats.mean = total / (double)count;
	return stats;
}
