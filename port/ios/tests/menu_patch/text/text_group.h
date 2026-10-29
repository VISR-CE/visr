/* the game's text_group.h, as menu_tags.c uses it */
#ifndef TEXT_GROUP_H
#define TEXT_GROUP_H

struct string_list
{
	struct tag_block strings;
};

struct string_list_entry
{
	struct tag_data string;
};

#endif
