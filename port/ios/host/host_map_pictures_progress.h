/* The import's existing bar counts finished maps; kept plain C for native probes. */
#ifndef HALO_HOST_MAP_PICTURES_PROGRESS_H
#define HALO_HOST_MAP_PICTURES_PROGRESS_H

static inline float host_map_pictures_progress_fraction(int done, int total) {
    if (total <= 0 || done <= 0) return 0;
    if (done >= total) return 1;
    return (float)((double)done / (double)total);
}

#endif
