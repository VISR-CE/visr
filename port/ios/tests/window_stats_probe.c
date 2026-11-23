#include "host_window_stats.h"
#include <assert.h>
#include <math.h>

int main(void) {
    /* the median is values[count / 2] and the 95th percentile values[count * 95 / 100], after sorting */
    double values[300];
    for (int index = 0; index < 300; index++)
        values[index] = (double)((index * 7) % 300);    /* every value 0 to 299 once, out of order */
    struct host_window_stats stats = host_window_stats(values, 300);
    assert(stats.median == 150.0 && stats.high == 285.0 && stats.worst == 299.0);
    assert(fabs(stats.mean - 149.5) < 1e-9);
    for (int index = 1; index < 300; index++)
        assert(values[index - 1] <= values[index]);
    /* one value is every statistic; no values are all zero */
    double one[1] = {4.0};
    stats = host_window_stats(one, 1);
    assert(stats.median == 4.0 && stats.high == 4.0 && stats.worst == 4.0 && stats.mean == 4.0);
    stats = host_window_stats(values, 0);
    assert(stats.median == 0.0 && stats.high == 0.0 && stats.worst == 0.0 && stats.mean == 0.0);
    return 0;
}
