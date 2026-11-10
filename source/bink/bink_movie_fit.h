/* Where a Bink movie is drawn on the virtual screen. Pure arithmetic with no game
headers, so a probe can test it on its own. A movie keeps its native size, centered,
unless that leaves only thin bars (at most BINK_MOVIE_THIN_BAR_PERCENT of the screen
on an axis, all axes thin or zero): then it scales up to cover the screen and the
overflow is cropped, centered. Wider bars keep the whole picture. */
#ifndef BINK_MOVIE_FIT_H
#define BINK_MOVIE_FIT_H

#define BINK_MOVIE_THIN_BAR_PERCENT 10

struct bink_movie_fit
{
	float x0, y0, x1, y1; /* screen rectangle */
	float u0, v0, u1, v1; /* movie pixels shown */
	int covers;           /* nonzero: scaled up and cropped to fill the screen */
	int has_bars;         /* nonzero: part of the screen is left uncovered */
};

static void bink_movie_fit_compute(
	int movie_width,
	int movie_height,
	int screen_width,
	int screen_height,
	struct bink_movie_fit *fit)
{
	int bar_x = screen_width - movie_width;
	int bar_y = screen_height - movie_height;
	int thin_x = bar_x <= 0 || bar_x * 100 <= screen_width * BINK_MOVIE_THIN_BAR_PERCENT;
	int thin_y = bar_y <= 0 || bar_y * 100 <= screen_height * BINK_MOVIE_THIN_BAR_PERCENT;

	fit->u0 = 0.0f;
	fit->v0 = 0.0f;
	fit->u1 = (float)movie_width;
	fit->v1 = (float)movie_height;
	fit->covers = 0;

	if ((bar_x > 0 || bar_y > 0) && bar_x >= 0 && bar_y >= 0 && thin_x && thin_y)
	{
		/* cover: scale by the larger ratio, crop what overflows */
		float scale_x = (float)screen_width / (float)movie_width;
		float scale_y = (float)screen_height / (float)movie_height;
		float scale = scale_x > scale_y ? scale_x : scale_y;
		float visible_width = (float)screen_width / scale;
		float visible_height = (float)screen_height / scale;

		fit->x0 = 0.0f;
		fit->y0 = 0.0f;
		fit->x1 = (float)screen_width;
		fit->y1 = (float)screen_height;
		fit->u0 = ((float)movie_width - visible_width) * 0.5f;
		fit->u1 = fit->u0 + visible_width;
		fit->v0 = ((float)movie_height - visible_height) * 0.5f;
		fit->v1 = fit->v0 + visible_height;
		fit->covers = 1;
		fit->has_bars = 0;
		return;
	}

	fit->x0 = (float)((short)((screen_width - movie_width) / 2));
	fit->x1 = (float)((short)((screen_width + movie_width) / 2));
	fit->y0 = (float)((short)((screen_height - movie_height) / 2));
	fit->y1 = (float)((short)((screen_height + movie_height) / 2));
	fit->has_bars = bar_x > 0 || bar_y > 0;
}

#endif
