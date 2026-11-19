/* SPDX-License-Identifier: Apache-2.0 */
#ifdef MAP_PICTURES_IMAGE_TEST
#include "map_pictures_image.h"
#include "map_pictures.h"
#include "bmp_files.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void crop_rows(void)
{
	uint8_t *src = malloc(1920 * 1080 * 4);
	uint8_t *dst = malloc(2048 * 1024 * 4);
	uint32_t x, y;
	assert(src && dst);
	for (y = 0; y < 1080; y++)
		for (x = 0; x < 1920; x++)
		{
			uint8_t *p = src + (y * 1920 + x) * 4;
			p[0] = (y < 60 || y >= 1020) ? 255 : 0;
			p[1] = y == 60 ? 200 : 0;
			p[2] = y == 1019 ? 200 : 0;
			p[3] = 0;
		}
	assert(map_pictures_crop_resample(src, 1920, 1080, 1920 * 4, dst, 2048, 1024));
	for (y = 0; y < 1024; y++)
		for (x = 0; x < 2048; x++)
			assert(dst[(y * 2048 + x) * 4] == 0);
	assert(dst[1] == 200);
	assert(dst[((1024 - 1) * 2048) * 4 + 2] == 200);
	free(src);
	free(dst);
}

static void colors_and_split(void)
{
	uint8_t src[8 * 6 * 4], dst[13 * 7 * 4];
	size_t i;
	uint32_t x, y;
	for (i = 0; i < sizeof(src); i += 4)
	{
		src[i] = 13; src[i + 1] = 97; src[i + 2] = 211; src[i + 3] = 0;
	}
	assert(map_pictures_crop_resample(src, 8, 6, 32, dst, 13, 7));
	for (i = 0; i < sizeof(dst); i += 4)
		assert(dst[i] == 13 && dst[i + 1] == 97 && dst[i + 2] == 211 && dst[i + 3] == 255);
	memset(src, 0, sizeof(src));
	for (y = 0; y < 6; y++)
		for (x = 0; x < 8; x++)
			src[(y * 8 + x) * 4 + (x < 4 ? 0 : 2)] = 255;
	assert(map_pictures_crop_resample(src, 8, 6, 32, dst, 13, 7));
	for (y = 0; y < 7; y++)
		for (x = 0; x < 13; x++)
		{
			const uint8_t *p = dst + (y * 13 + x) * 4;
			if (x < 6) assert(p[0] > p[2]);
			if (x > 6) assert(p[2] > p[0]);
			assert(p[3] == 255);
		}
	assert(map_pictures_crop_resample(src, 1, 1, 4, dst, 13, 7));
	for (i = 0; i < sizeof(dst); i += 4)
		assert(dst[i] == 255 && dst[i + 1] == 0 && dst[i + 2] == 0 && dst[i + 3] == 255);
}

static void sampling_and_stride(void)
{
	/* Pixel-center interpolation: 2 pixels enlarged to 4 clamp at both edges. */
	uint8_t src[] = {0, 0, 0, 0, 100, 100, 100, 0, 255, 255, 255, 255};
	uint8_t dst[32];
	uint8_t expected[] = {0, 25, 75, 100};
	size_t i;
	assert(map_pictures_crop_resample(src, 2, 1, 12, dst, 4, 2));
	for (i = 0; i < 8; i++) assert(dst[i * 4] == expected[i % 4]);
	{
		uint8_t padded[] = {10, 20, 30, 0, 255, 255, 255, 255,
			40, 50, 60, 0, 255, 255, 255, 255};
		assert(map_pictures_crop_resample(padded, 1, 2, 8, dst, 1, 2));
		assert(dst[0] == 10 && dst[1] == 20 && dst[2] == 30 && dst[3] == 255);
		assert(dst[4] == 40 && dst[5] == 50 && dst[6] == 60 && dst[7] == 255);
	}
	{
		/* 5x3 to 1:1 rounds the centered width to 3, skipping both ends. */
		uint8_t wide[5 * 3 * 4] = {0};
		for (i = 0; i < 15; i++) wide[i * 4] = (uint8_t)((i % 5) * 40);
		assert(map_pictures_crop_resample(wide, 5, 3, 20, dst, 2, 2));
		assert(dst[0] == 50 && dst[4] == 110);
		/* An extreme aspect still samples the single centered column. */
		assert(map_pictures_crop_resample(wide, 5, 1, 20, dst, 1, 8));
		for (i = 0; i < 8; i++) assert(dst[i * 4] == 80);
	}
	{
		/* 3x2 to 2:1 rounds 1.5 rows up to 2 rather than dropping a row. */
		uint8_t odd[3 * 2 * 4] = {0};
		for (i = 3; i < 6; i++) odd[i * 4] = 100;
		assert(map_pictures_crop_resample(odd, 3, 2, 12, dst, 2, 1));
		assert(dst[0] == 50 && dst[4] == 50);
	}
}

static void bad_arguments(void)
{
	uint8_t pixel[4] = {0};
	assert(!map_pictures_crop_resample(NULL, 1, 1, 4, pixel, 1, 1));
	assert(!map_pictures_crop_resample(pixel, 1, 1, 4, NULL, 1, 1));
	assert(!map_pictures_crop_resample(pixel, 0, 1, 4, pixel, 1, 1));
	assert(!map_pictures_crop_resample(pixel, 1, 0, 4, pixel, 1, 1));
	assert(!map_pictures_crop_resample(pixel, 1, 1, 4, pixel, 0, 1));
	assert(!map_pictures_crop_resample(pixel, 1, 1, 4, pixel, 1, 0));
	assert(!map_pictures_crop_resample(pixel, 1, 1, 3, pixel, 1, 1));
	assert(!map_pictures_crop_resample(pixel, MAP_PICTURES_MAX_DIMENSION + 1, 1, SIZE_MAX, pixel, 1, 1));
	assert(!map_pictures_crop_resample(pixel, 1, MAP_PICTURES_MAX_DIMENSION + 1, 4, pixel, 1, 1));
	assert(!map_pictures_crop_resample(pixel, 1, 1, 4, pixel, MAP_PICTURES_MAX_DIMENSION + 1, 1));
	assert(!map_pictures_crop_resample(pixel, 1, 1, 4, pixel, 1, MAP_PICTURES_MAX_DIMENSION + 1));
	assert(!map_pictures_crop_resample(pixel, 1, 2, SIZE_MAX, pixel, 1, 1));
}

static void bmp_round_trip(void)
{
	uint8_t src[5 * 3 * 4], out[54 + sizeof(src)];
	uint32_t pixels[15];
	struct bmp_file_picture picture;
	size_t i;
	for (i = 0; i < 15; i++)
	{
		src[i * 4] = (uint8_t)(i * 11);
		src[i * 4 + 1] = (uint8_t)(i * 7);
		src[i * 4 + 2] = (uint8_t)(i * 17);
		src[i * 4 + 3] = 0;
	}
	assert(map_pictures_bmp_size(5, 3) == sizeof(out));
	assert(!map_pictures_bmp_size(0, 1));
	assert(!map_pictures_bmp_size(1, 0));
	assert(!map_pictures_bmp_size(UINT32_MAX, UINT32_MAX));
	assert(!map_pictures_bmp_encode(out, sizeof(out) - 1, src, 5, 3));
	assert(!map_pictures_bmp_encode(NULL, sizeof(out), src, 5, 3));
	assert(!map_pictures_bmp_encode(out, sizeof(out), NULL, 5, 3));
	assert(map_pictures_bmp_encode(out, sizeof(out), src, 5, 3) == sizeof(out));
	assert(bmp_file_open(out, sizeof(out), &picture) == _bmp_file_status_ok);
	assert(!picture.top_down && picture.bits_per_pixel == 32);
	assert(picture.width == 5 && picture.height == 3);
	bmp_file_fit(out, &picture, 0, 0, 5, 3, pixels);
	for (i = 0; i < 15; i++)
		assert(pixels[i] == (0xFF000000u | ((uint32_t)src[i * 4 + 2] << 16) |
			((uint32_t)src[i * 4 + 1] << 8) | src[i * 4]));
}

int main(void)
{
	crop_rows();
	colors_and_split();
	sampling_and_stride();
	bad_arguments();
	bmp_round_trip();
	puts("map picture image tests passed");
	return 0;
}
#endif
