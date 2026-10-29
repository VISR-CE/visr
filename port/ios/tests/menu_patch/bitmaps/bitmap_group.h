/* the game's bitmap_group.h, as menu_tags.c uses it */
#ifndef BITMAP_GROUP_H
#define BITMAP_GROUP_H

#include "tag_files/tag_groups.h"

enum
{
	BITMAP_GROUP_TAG = 'bitm',
};

struct bitmap_data
{
	unsigned long signature;
	short width;
	short height;
	short depth;
	short type;
	short format;
	unsigned short flags;
	short registration_point[2];
	short mipmap_count;
	short mipmap_pad;
	long pixels_offset;
	long pixels_size;
	long tag_index;
	long cache_block_index;
	void *hardware_format;
	void *base_address;
};

struct bitmap_group_sequence
{
	char name[32];
	short first_bitmap_index;
	short bitmap_count;
	long unused[4];
	struct tag_block sprites;
};

struct bitmap_group
{
	short type;
	struct tag_block sequences;
	struct tag_block bitmaps;
};

#define bitmap_group_get(index) ((struct bitmap_group *)tag_get(BITMAP_GROUP_TAG, (index)))

#endif
