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
    /* one bar: extraction, then textures, then pictures; a pictures-only launch gets the whole bar */
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_EXTRACT, 0, 1) == 0);
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_EXTRACT, 1, 1) == HOST_IMPORT_BAR_EXTRACT_END);
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_TEXTURES, 0, 1) == HOST_IMPORT_BAR_EXTRACT_END);
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_TEXTURES, 1, 1) == HOST_IMPORT_BAR_TEXTURES_END);
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_PICTURES, 0, 1) == HOST_IMPORT_BAR_TEXTURES_END);
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_PICTURES, 1, 1) == 1);
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_PICTURES, 0, 0) == 0);
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_PICTURES, 0.5f, 0) == 0.5f);
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_EXTRACT, 2, 1) == HOST_IMPORT_BAR_EXTRACT_END);
    assert(host_import_bar_fraction(HOST_IMPORT_BAR_EXTRACT, -1, 1) == 0);
    {   /* never moves backward across the stages */
        float last = -1;
        for (int stage = HOST_IMPORT_BAR_EXTRACT; stage <= HOST_IMPORT_BAR_PICTURES; stage++)
            for (int i = 0; i <= 10; i++) {
                float value = host_import_bar_fraction((enum host_import_bar_stage)stage, i / 10.0f, 1);
                assert(value >= last);
                last = value;
            }
    }
    puts("map_pictures_progress_probe: 21 checks passed");
    return 0;
}
