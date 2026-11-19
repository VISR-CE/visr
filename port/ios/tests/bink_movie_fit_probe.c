/* Bink movie placement: thin bars are covered, wide bars keep the whole picture. */
#include "../../../source/bink/bink_movie_fit.h"
#include <assert.h>
#include <math.h>

static int near(float a, float b) { return fabsf(a - b) < 0.01f; }

int main(void)
{
	struct bink_movie_fit fit;

	/* iPhone Duo inner display: about 682x480 virtual, a 640x480 movie leaves 6% bars */
	bink_movie_fit_compute(640, 480, 682, 480, &fit);
	assert(fit.covers && !fit.has_bars);
	assert(near(fit.x0, 0) && near(fit.y0, 0) && near(fit.x1, 682) && near(fit.y1, 480));
	/* the width fills, so the top and bottom of the movie are cropped evenly */
	assert(near(fit.u0, 0) && near(fit.u1, 640));
	assert(fit.v0 > 14 && fit.v0 < 15.5 && near(fit.v0 + fit.v1, 480));

	/* 4:3 screen: exact fit, nothing changes */
	bink_movie_fit_compute(640, 480, 640, 480, &fit);
	assert(!fit.covers && !fit.has_bars);
	assert(near(fit.x0, 0) && near(fit.x1, 640) && near(fit.u1, 640) && near(fit.v1, 480));

	/* 16:9 and 16:10 keep pillarbox bars and the whole picture */
	bink_movie_fit_compute(640, 480, 853, 480, &fit);
	assert(!fit.covers && fit.has_bars);
	assert(near(fit.x0, 106) && near(fit.x1, 746) && near(fit.u0, 0) && near(fit.u1, 640));
	bink_movie_fit_compute(640, 480, 768, 480, &fit);
	assert(!fit.covers && fit.has_bars && near(fit.x0, 64) && near(fit.x1, 704));

	/* the edge: a bar of 71 px on 711 is just under 10%, 72 on 712 is over */
	bink_movie_fit_compute(640, 480, 712, 480, &fit);
	assert(!fit.covers);
	bink_movie_fit_compute(640, 480, 711, 480, &fit);
	assert(fit.covers);

	/* a movie wider than the screen scales down to fit, centered, with the leftover bars filled */
	bink_movie_fit_compute(640, 480, 600, 480, &fit);
	assert(!fit.covers && fit.has_bars);
	assert(near(fit.x0, 0) && near(fit.x1, 600) && near(fit.y0, 15) && near(fit.y1, 465));
	assert(near(fit.u0, 0) && near(fit.u1, 640) && near(fit.v0, 0) && near(fit.v1, 480));

	/* portrait (a 1080x2340 screen, 466x1010 virtual): the width fills, letterbox bars above and below */
	bink_movie_fit_compute(640, 480, 466, 1010, &fit);
	assert(!fit.covers && fit.has_bars);
	assert(near(fit.x0, 0) && near(fit.x1, 466));
	assert(near(fit.y1 - fit.y0, 349.5f) || near(fit.y1 - fit.y0, 350) || near(fit.y1 - fit.y0, 349));
	assert(near(fit.y0 + fit.y1, 1010) || fabsf(fit.y0 + fit.y1 - 1010) < 1.5f);
	assert(fit.y0 > 300 && fit.u0 == 0 && near(fit.u1, 640) && near(fit.v1, 480));
	bink_movie_fit_compute(640, 480, 1080, 2340, &fit);
	assert(!fit.covers && fit.has_bars && near(fit.x0, 220) && near(fit.x1, 860) && near(fit.y0, 930) && near(fit.y1, 1410));
	/* the Duo portrait screen at a 640-unit width: the fit is exact, bars above and below only */
	bink_movie_fit_compute(640, 480, 640, 960, &fit);
	assert(near(fit.x0, 0) && near(fit.x1, 640) && near(fit.y0, 240) && near(fit.y1, 720));

	/* thin bars above and below cover too, cropping the sides of the movie */
	bink_movie_fit_compute(640, 480, 640, 515, &fit);
	assert(fit.covers && near(fit.v0, 0) && near(fit.v1, 480) && fit.u0 > 0);

	/* thin on one axis and wide on the other keeps the fit */
	bink_movie_fit_compute(640, 480, 682, 600, &fit);
	assert(!fit.covers && fit.has_bars);

	return 0;
}
