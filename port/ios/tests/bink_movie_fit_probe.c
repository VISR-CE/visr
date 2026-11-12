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

	/* a movie larger than the screen is never scaled up */
	bink_movie_fit_compute(640, 480, 600, 480, &fit);
	assert(!fit.covers && !fit.has_bars);

	/* thin bars above and below cover too, cropping the sides of the movie */
	bink_movie_fit_compute(640, 480, 640, 515, &fit);
	assert(fit.covers && near(fit.v0, 0) && near(fit.v1, 480) && fit.u0 > 0);

	/* thin on one axis and wide on the other keeps the fit */
	bink_movie_fit_compute(640, 480, 682, 600, &fit);
	assert(!fit.covers && fit.has_bars);

	return 0;
}
