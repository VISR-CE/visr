/* The import's existing bar counts finished maps; kept plain C for native probes. */
#ifndef HALO_HOST_MAP_PICTURES_PROGRESS_H
#define HALO_HOST_MAP_PICTURES_PROGRESS_H

static inline float host_map_pictures_progress_fraction(int done, int total) {
    if (total <= 0 || done <= 0) return 0;
    if (done >= total) return 1;
    return (float)((double)done / (double)total);
}

/* One bar over a disc import and the map pictures that follow it: extraction fills the first part, this device's
   texture preparation the next, the map pictures the rest. A launch that only prepares the pictures (an import
   interrupted earlier) gives them the whole bar. */
enum host_import_bar_stage { HOST_IMPORT_BAR_EXTRACT, HOST_IMPORT_BAR_TEXTURES, HOST_IMPORT_BAR_PICTURES };
#define HOST_IMPORT_BAR_EXTRACT_END 0.30f
#define HOST_IMPORT_BAR_TEXTURES_END 0.40f
static inline float host_import_bar_fraction(enum host_import_bar_stage stage, float part, int extracted) {
    float start = 0, end = 1;
    if (!(part > 0)) part = 0;   /* (NaN too) */
    if (part > 1) part = 1;
    if (stage == HOST_IMPORT_BAR_EXTRACT) end = HOST_IMPORT_BAR_EXTRACT_END;
    else if (stage == HOST_IMPORT_BAR_TEXTURES) { start = HOST_IMPORT_BAR_EXTRACT_END; end = HOST_IMPORT_BAR_TEXTURES_END; }
    else if (extracted) start = HOST_IMPORT_BAR_TEXTURES_END;
    return start + (end - start) * part;
}

#endif
