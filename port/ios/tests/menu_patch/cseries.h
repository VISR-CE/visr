/* the game's cseries.h, as menu_tags.c uses it (tools/test_menu_patches.py) */
#ifndef CSERIES_H
#define CSERIES_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;
typedef unsigned short word;
typedef float real;
typedef byte boolean;

enum
{
	UNSIGNED_SHORT_MAX = 65535,
};

#define TRUE 1
#define FALSE 0
#define NONE -1
#define CEILING(n, ceiling) ((n) > (ceiling) ? (ceiling) : (n))
#define PIN(n, floor, ceiling) ((n) < (floor) ? (floor) : CEILING((n), (ceiling)))
#define FLAG(b) (1 << (b))
#define TEST_FLAG(flags, bit) (((flags) & (unsigned)FLAG(bit)) != 0)
#define SET_FLAG(f, b, v) ((v) ? ((f) |= (unsigned)FLAG(b)) : ((f) &= (unsigned)~FLAG(b)))
#define NUMBEROF(array) (sizeof(array) / sizeof(array[0]))
#define DATUM_INDEX_TO_ABSOLUTE_INDEX(datum_index) ((datum_index) & UNSIGNED_SHORT_MAX)

union rectangle2d
{
	short v[4];
	struct { short y0, x0, y1, x1; };
};
typedef union rectangle2d rectangle2d;

union real_argb_color
{
	real n[4];
	struct { real alpha, red, green, blue; };
};
typedef union real_argb_color real_argb_color;

#endif
