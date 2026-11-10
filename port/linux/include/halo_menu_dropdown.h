/* Pointer dropdown geometry in the game's 640 by 480 menu space. Option rows
are 52 units, at least 44 pt on the iPhone 17 Pro in landscape at the minimum
menu scale. */
#ifndef HALO_MENU_DROPDOWN_H
#define HALO_MENU_DROPDOWN_H

#define HALO_DROPDOWN_WIDTH 165
#define HALO_DROPDOWN_PITCH 52
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

/* Prefer a full panel below, then above. If neither side fits, shift it
within the visible band above the legend, reducing rows only when necessary. */
static void halo_menu_dropdown_open(struct halo_menu_dropdown *d,
	int left, int top, int bottom, int count, int selected,
	int limit_top, int limit_bottom)
{
	int wanted = count < HALO_DROPDOWN_MAX_ROWS ? count : HALO_DROPDOWN_MAX_ROWS;
	int fit = (limit_bottom - limit_top - 3) / HALO_DROPDOWN_PITCH;
	int height;

	d->count = count;
	d->rows = wanted < fit ? wanted : fit;
	if (d->rows < 0) d->rows = 0;
	d->open = count > 0 && d->rows > 0;
	height = d->rows * HALO_DROPDOWN_PITCH + 3;

	d->x = left < 0 ? 0 : left > 640 - HALO_DROPDOWN_WIDTH ? 640 - HALO_DROPDOWN_WIDTH : left;
	if (bottom >= limit_top && bottom + height <= limit_bottom)
		d->y = bottom;
	else if (top <= limit_bottom && top - height >= limit_top)
		d->y = top - height;
	else
		d->y = limit_bottom - height;
	if (d->y < limit_top) d->y = limit_top;

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
