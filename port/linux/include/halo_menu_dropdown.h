/* Pointer dropdown geometry in the game's 640 by 480 menu space. */
#ifndef HALO_MENU_DROPDOWN_H
#define HALO_MENU_DROPDOWN_H

#define HALO_DROPDOWN_WIDTH 165
#define HALO_DROPDOWN_PITCH 26
#define HALO_DROPDOWN_MAX_ROWS 7

struct halo_menu_dropdown
{
	int open, x, y, rows, count, first, highlight;
};

static void halo_menu_dropdown_scroll(struct halo_menu_dropdown *d, int steps)
{
	d->first += steps;
	if (d->first > d->count - d->rows) d->first = d->count - d->rows;
	if (d->first < 0) d->first = 0;
}

static void halo_menu_dropdown_open(struct halo_menu_dropdown *d,
	int left, int top, int bottom, int count, int selected)
{
	int height;
	d->open = count > 0;
	d->count = count;
	d->rows = count < HALO_DROPDOWN_MAX_ROWS ? count : HALO_DROPDOWN_MAX_ROWS;
	d->x = left < 0 ? 0 : left > 640 - HALO_DROPDOWN_WIDTH ? 640 - HALO_DROPDOWN_WIDTH : left;
	height = d->rows * HALO_DROPDOWN_PITCH + 3;
	d->y = bottom + height <= 480 ? bottom : top - height;
	if (d->y < 0) d->y = 0;
	if (d->y > 480 - height) d->y = 480 - height;
	d->highlight = selected;
	d->first = selected - d->rows + 1;
	halo_menu_dropdown_scroll(d, 0);
}

static int halo_menu_dropdown_option(struct halo_menu_dropdown const *d, int x, int y)
{
	if (!d->open || x < d->x + 2 || x >= d->x + HALO_DROPDOWN_WIDTH - 2 ||
		y < d->y + 2 || y >= d->y + 2 + d->rows * HALO_DROPDOWN_PITCH)
		return -1;
	return d->first + (y - d->y - 2) / HALO_DROPDOWN_PITCH;
}

static int halo_menu_dropdown_click(struct halo_menu_dropdown *d, int x, int y)
{
	int option = halo_menu_dropdown_option(d, x, y);
	d->open = 0;
	return option;
}
#endif
