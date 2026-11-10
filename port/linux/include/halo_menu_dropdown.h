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

/* The panel hangs under the value area when at least two options (or the
whole list) fit there, otherwise above it, and shows as many rows as fit on
that side; the rest scroll. */
static void halo_menu_dropdown_open(struct halo_menu_dropdown *d,
	int left, int top, int bottom, int count, int selected)
{
	int below = (480 - bottom - 3) / HALO_DROPDOWN_PITCH;
	int above = (top - 3) / HALO_DROPDOWN_PITCH;
	int wanted = count < HALO_DROPDOWN_MAX_ROWS ? count : HALO_DROPDOWN_MAX_ROWS;
	int under = below >= (wanted < 2 ? wanted : 2) || below >= above;
	int fit = under ? below : above;
	d->open = count > 0;
	d->count = count;
	d->rows = wanted < fit ? wanted : fit;
	if (d->rows < 1) d->rows = 1;
	d->x = left < 0 ? 0 : left > 640 - HALO_DROPDOWN_WIDTH ? 640 - HALO_DROPDOWN_WIDTH : left;
	d->y = under ? bottom : top - (d->rows * HALO_DROPDOWN_PITCH + 3);
	if (d->y < 0) d->y = 0;
	if (d->y > 480 - (d->rows * HALO_DROPDOWN_PITCH + 3)) d->y = 480 - (d->rows * HALO_DROPDOWN_PITCH + 3);
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
