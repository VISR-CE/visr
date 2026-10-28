/* the game's tag_groups.h, as menu_tags.c uses it */
#ifndef TAG_GROUPS_H
#define TAG_GROUPS_H

struct tag_block
{
	long count;
	void *address;
	void *definition;
};

struct tag_reference
{
	unsigned long group_tag;
	char *name;
	long name_length;
	long index;
};

struct tag_data
{
	long size;
	unsigned long pad;
	long file_offset;
	void *address;
	void *definition;
};

long tag_loaded(long group_tag, const char *name);
void *tag_get(long group_tag, long tag_index);

#endif
