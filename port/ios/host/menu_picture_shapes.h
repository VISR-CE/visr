/* Apache-2.0. Authored marker geometry with source-fitted gradients. */
#ifndef MENU_PICTURE_SHAPES_H
#define MENU_PICTURE_SHAPES_H

#include <stdint.h>

/* source is 512 by 256 straight RGBA; rgba is scale times larger.
 * Returns one on success (including pictures without shapes), zero on failure.
 * Cancellation may leave a partially updated output, which the caller discards. */
int menu_picture_shapes(unsigned picture, const uint8_t *source,
                        uint8_t *rgba, unsigned scale,
                        const volatile int *cancel);

/* Four 32 by 256 carousel frames, identified by decoded source hash.
 * Unknown or invalid inputs preserve rgba; cancellation is transactional.
 * Returns 1 on success, 0 for plain upscale fallback, -1 on cancellation. */
int menu_carousel_shapes_source(const uint8_t *source, unsigned width, unsigned height);
int menu_carousel_shapes(const uint8_t *source, unsigned width, unsigned height,
                         uint8_t *rgba, unsigned scale, const volatile int *cancel);

/* The 256 by 128 level-select lock (sp_levels frame 10), identified by decoded
 * source hash; same contract as the carousel frames. */
int menu_lock_shapes_source(const uint8_t *source, unsigned width, unsigned height);
int menu_lock_shapes(const uint8_t *source, unsigned width, unsigned height,
                     uint8_t *rgba, unsigned scale, const volatile int *cancel);

#endif
