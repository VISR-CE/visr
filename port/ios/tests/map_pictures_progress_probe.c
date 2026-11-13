#include "host_map_pictures_progress.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
    assert(host_map_pictures_progress_fraction(0, 0) == 0);
    assert(host_map_pictures_progress_fraction(5, 0) == 0);
    assert(host_map_pictures_progress_fraction(5, -1) == 0);
    assert(host_map_pictures_progress_fraction(-1, 8) == 0);
    assert(host_map_pictures_progress_fraction(0, 8) == 0);
    assert(host_map_pictures_progress_fraction(3, 8) == 0.375f);
    assert(host_map_pictures_progress_fraction(8, 8) == 1);
    assert(host_map_pictures_progress_fraction(9, 8) == 1);
    assert(host_map_pictures_progress_fraction(INT_MAX, INT_MAX) == 1);
    assert(host_map_pictures_progress_fraction(INT_MIN, INT_MAX) == 0);
    puts("map_pictures_progress_probe: 10 checks passed");
    return 0;
}
