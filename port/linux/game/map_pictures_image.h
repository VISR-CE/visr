/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAP_PICTURES_IMAGE_H
#define MAP_PICTURES_IMAGE_H

#include <stddef.h>
#include <stdint.h>

/* Center-crop top-down BGRA to the destination aspect, then sample bilinearly
   at pixel centers. Alpha becomes 255; upscaling is allowed. Returns 0 for
   null buffers, zero sizes, dimensions above MAP_PICTURES_MAX_DIMENSION,
   a stride below src_width*4, or an overflowing source span. Buffers must
   have enough storage and must not overlap. */
int map_pictures_crop_resample(const uint8_t *src, uint32_t src_width, uint32_t src_height,
	size_t src_stride, uint8_t *dst, uint32_t dst_width, uint32_t dst_height);

/* Size of a bottom-up 32-bit BI_RGB BMP with BITMAPINFOHEADER; zero for
   zero dimensions or sizes that overflow size_t or the BMP header fields. */
size_t map_pictures_bmp_size(uint32_t width, uint32_t height);
/* Encode top-down packed BGRA. Returns bytes written, or zero on invalid
   arguments or insufficient output storage. Buffers must not overlap. */
size_t map_pictures_bmp_encode(uint8_t *out, size_t size, const uint8_t *bgra,
	uint32_t width, uint32_t height);

#endif
