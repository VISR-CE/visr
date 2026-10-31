/* Apache-2.0. Source-fitted menu band and source-preserved frame strips. */
#pragma once
#include <stdint.h>
#include <stddef.h>
struct menu_background_redraw_metrics { double seconds; size_t scratch_bytes; };
/* Hashes cover decoded straight RGBA. Returns 0 for the approved source, -1 otherwise. */
int menu_background_redraw_source(const uint8_t *source, unsigned width, unsigned height);
/* Transactional: 1 publishes, 0 keeps the upscale, -1 cancels. Scale is 1, 2 or 4. */
int menu_background_redraw(const uint8_t *source, unsigned width, unsigned height,
    uint8_t *rgba, unsigned scale, const volatile int *cancel,
    struct menu_background_redraw_metrics *metrics);
