/* SPDX-License-Identifier: Apache-2.0 */
#include "map_pictures_image.h"
#include "map_pictures.h"

#include <math.h>
#include <string.h>

/* Clamp within the cropped span before choosing neighboring pixel centers. */
static double sample_position(uint32_t index, uint32_t count, uint32_t length)
{
	double position = ((double)index + 0.5) * length / count - 0.5;
	if (position < 0.0) return 0.0;
	if (position > length - 1.0) return length - 1.0;
	return position;
}

int map_pictures_crop_resample(const uint8_t *src, uint32_t src_width, uint32_t src_height,
	size_t src_stride, uint8_t *dst, uint32_t dst_width, uint32_t dst_height)
{
	uint32_t crop_width = src_width, crop_height = src_height;
	uint32_t crop_x, crop_y, x, y;
	size_t row_bytes = (size_t)src_width * 4;
	if (!src || !dst || !src_width || !src_height || !dst_width || !dst_height ||
		src_width > MAP_PICTURES_MAX_DIMENSION || src_height > MAP_PICTURES_MAX_DIMENSION ||
		dst_width > MAP_PICTURES_MAX_DIMENSION || dst_height > MAP_PICTURES_MAX_DIMENSION ||
		src_stride < row_bytes ||
		(src_height > 1 && src_stride > (SIZE_MAX - row_bytes) / (src_height - 1)) ||
		(size_t)dst_width * 4 > SIZE_MAX / dst_height)
		return 0;

	if ((uint64_t)src_width * dst_height > (uint64_t)src_height * dst_width)
		crop_width = (uint32_t)(((uint64_t)src_height * dst_width + dst_height / 2) / dst_height);
	else
		crop_height = (uint32_t)(((uint64_t)src_width * dst_height + dst_width / 2) / dst_width);
	/* Extreme aspect ratios still need one source pixel to sample. */
	if (!crop_width) crop_width = 1;
	if (!crop_height) crop_height = 1;
	crop_x = (src_width - crop_width) / 2;
	crop_y = (src_height - crop_height) / 2;
	for (y = 0; y < dst_height; y++)
	{
		double sy = sample_position(y, dst_height, crop_height);
		uint32_t y0 = (uint32_t)sy;
		uint32_t y1 = y0 + (y0 + 1 < crop_height);
		double fy = sy - y0;
		for (x = 0; x < dst_width; x++)
		{
			double sx = sample_position(x, dst_width, crop_width);
			uint32_t x0 = (uint32_t)sx;
			uint32_t x1 = x0 + (x0 + 1 < crop_width);
			double fx = sx - x0;
			const uint8_t *a = src + (crop_y + y0) * src_stride + (size_t)(crop_x + x0) * 4;
			const uint8_t *b = src + (crop_y + y0) * src_stride + (size_t)(crop_x + x1) * 4;
			const uint8_t *c = src + (crop_y + y1) * src_stride + (size_t)(crop_x + x0) * 4;
			const uint8_t *d = src + (crop_y + y1) * src_stride + (size_t)(crop_x + x1) * 4;
			uint8_t *pixel = dst + ((size_t)y * dst_width + x) * 4;
			unsigned channel;
			for (channel = 0; channel < 3; channel++)
			{
				double top = a[channel] * (1.0 - fx) + b[channel] * fx;
				double bottom = c[channel] * (1.0 - fx) + d[channel] * fx;
				pixel[channel] = (uint8_t)floor(top * (1.0 - fy) + bottom * fy + 0.5);
			}
			pixel[3] = 255;
		}
	}
	return 1;
}

size_t map_pictures_bmp_size(uint32_t width, uint32_t height)
{
	uint64_t bytes;
	/* BITMAPINFOHEADER dimensions are signed; the file size is unsigned. */
	if (!width || !height || width > INT32_MAX || height > INT32_MAX) return 0;
	bytes = 54 + (uint64_t)width * height * 4;
	if (bytes > UINT32_MAX || bytes > SIZE_MAX) return 0;
	return (size_t)bytes;
}

static void write_u32(uint8_t *out, uint32_t value)
{
	out[0] = (uint8_t)value;
	out[1] = (uint8_t)(value >> 8);
	out[2] = (uint8_t)(value >> 16);
	out[3] = (uint8_t)(value >> 24);
}

size_t map_pictures_bmp_encode(uint8_t *out, size_t size, const uint8_t *bgra,
	uint32_t width, uint32_t height)
{
	size_t bytes = map_pictures_bmp_size(width, height);
	size_t row_bytes = (size_t)width * 4;
	uint32_t y;
	if (!bytes || !out || !bgra || size < bytes) return 0;
	memset(out, 0, 54);
	out[0] = 'B'; out[1] = 'M';
	write_u32(out + 2, (uint32_t)bytes);
	write_u32(out + 10, 54);
	write_u32(out + 14, 40);
	write_u32(out + 18, width);
	write_u32(out + 22, height);
	out[26] = 1;
	out[28] = 32;
	write_u32(out + 34, (uint32_t)(bytes - 54));
	for (y = 0; y < height; y++)
		memcpy(out + 54 + (size_t)y * row_bytes,
			bgra + (size_t)(height - 1 - y) * row_bytes, row_bytes);
	return bytes;
}
