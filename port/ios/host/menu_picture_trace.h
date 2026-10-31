/* Apache-2.0. Source-derived menu silhouettes; no sampled paths are stored. */
#pragma once
#include <stddef.h>
#include <stdint.h>
struct menu_picture_trace_metrics {
    unsigned paths, area, boundary;
    size_t scratch_bytes;
    double seconds;
};
/* Recognize only the six approved decoded 512x256 RGBA sources. -1 means plain bpf. */
int menu_picture_trace_source(const uint8_t *source, unsigned width, unsigned height);
/* Transactional: fallback (0) and cancellation (-1) leave bpf byte-identical. 1 means traced.
   Source is straight RGBA8, output is its 2x or 4x straight RGBA8 bpf. */
int menu_picture_trace(const uint8_t *source, unsigned width, unsigned height,
    uint8_t *bpf, unsigned scale, const volatile int *cancel, struct menu_picture_trace_metrics *metrics);
