/* Pointer slider geometry in the game's 640 by 480 menu space. Numeric step
lists use equal spacing per selectable step, including nonuniform numbers. */
#ifndef HALO_MENU_SLIDER_H
#define HALO_MENU_SLIDER_H

#define HALO_MENU_SLIDER_WIDTH 165
/* Units from the row's right edge reserved for the value text beside the track. */
#define HALO_MENU_SLIDER_VALUE_ROOM 64
#define HALO_MENU_SLIDER_VALUE_GAP 6

struct halo_menu_slider { int left, right, y; };
struct halo_slider_rect { int x0, y0, x1, y1; };

static void halo_menu_slider_geometry(struct halo_menu_slider *s,
	int left, int top, int right, int bottom)
{
	if (right - left < HALO_MENU_SLIDER_WIDTH) left = right - HALO_MENU_SLIDER_WIDTH;
	s->left = left + 10;
	/* Leave room for the current value (such as 1.5x) and the mouse's right arrow. */
	s->right = right - HALO_MENU_SLIDER_VALUE_ROOM;
	if (s->right <= s->left) s->right = s->left + 1;
	s->y = (top + bottom) / 2;
}

static int halo_menu_slider_value(struct halo_menu_slider const *s, int x, int count)
{
	int width = s->right - s->left;
	if (count < 2 || x <= s->left) return 0;
	if (x >= s->right) return count - 1;
	return ((x - s->left) * (count - 1) + width / 2) / width;
}

static struct halo_slider_rect halo_menu_slider_thumb(struct halo_menu_slider const *s,
	int selected, int count)
{
	struct halo_slider_rect r;
	int x = s->left;
	if (count > 1) x += (s->right - s->left) * selected / (count - 1);
	r.x0 = x - 7; r.x1 = r.x0 + 15;
	r.y0 = s->y - 7; r.y1 = r.y0 + 15;
	return r;
}

static int halo_menu_slider_tap(struct halo_menu_slider const *s, int selected, int count, int x)
{
	struct halo_slider_rect r = halo_menu_slider_thumb(s, selected, count);
	return x < r.x0 ? -1 : x >= r.x1 ? 1 : 0;
}

static int halo_menu_slider_dragged(int down_x, int down_y, int x, int y)
{
	int dx = x - down_x, dy = y - down_y;
	return dx * dx + dy * dy > 16;
}

static int halo_menu_slider_hit(struct halo_menu_slider const *s, int x, int y)
{
	return x >= s->left - 8 && x <= s->right + 8 && y >= s->y - 12 && y <= s->y + 12;
}
#endif
