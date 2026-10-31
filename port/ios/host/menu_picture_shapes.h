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

#endif
