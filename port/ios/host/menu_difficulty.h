/* Apache-2.0. Matching large difficulty art stays in the player's imported data. */
#pragma once
#include <stdint.h>
/* Decoded straight RGBA hashes identify frame order; unknown sources return -1. */
int menu_difficulty_source(const uint8_t *rgba, unsigned width, unsigned height);
int menu_difficulty_large_source(const uint8_t *rgba, unsigned width, unsigned height);
/* Transactional: 1 publishes an aligned large frame, 0 preserves S4g, -1 cancels.
   Both sources must match the same frame. Output is 64*scale square RGBA8. */
int menu_difficulty_replace(const uint8_t *small, const uint8_t *large,
    uint8_t *output, unsigned scale, const volatile int *cancel);
