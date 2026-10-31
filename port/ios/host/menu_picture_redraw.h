/* Apache-2.0. Authored keyboard and source-guarded button-map labels. */
#pragma once
#include <stdint.h>
#include <stddef.h>
struct menu_picture_redraw_metrics { double seconds; size_t scratch_bytes; };
/* -1 unknown; 0 keyboard; 4..8 button map. Hashes cover decoded straight RGBA. */
int menu_picture_redraw_source(const uint8_t *source, unsigned width, unsigned height);
/* Transactional: 0 keeps plain bpf; -1 cancels; 1 publishes the redraw.
   Resources come from the app bundle; probes may supply a resource directory. */
int menu_picture_redraw(const uint8_t *source, unsigned width, unsigned height,
    uint8_t *bpf, unsigned scale, const volatile int *cancel, const char *resources,
    struct menu_picture_redraw_metrics *metrics);
