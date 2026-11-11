/* Bar fill for Bink movies. Pure arithmetic with no game headers, so a probe can test it on
its own. The bars around a movie continue the picture's background: samples taken from the
frame's outer edge feed a per-channel linear fit of color against distance from the frame
center (as a fraction of the frame's half-diagonal), and bar pixels evaluate that line at
their own distance. A frame that is not a smooth radial background (the fit's error is above
BINK_GRADIENT_MAXIMUM_RMS, or the samples cover too little of the radius) falls back to the
flat average of its edge samples. */
#ifndef BINK_EDGE_GRADIENT_H
#define BINK_EDGE_GRADIENT_H

#include <math.h>

#define BINK_GRADIENT_MAXIMUM_RMS 6.0
#define BINK_GRADIENT_MINIMUM_SPREAD 0.08 /* least spread of r among the samples to trust a slope */
#define BINK_GRADIENT_RANGE_MARGIN 24.0f  /* a fill may not leave the sampled values by more than this */

struct bink_gradient_sums
{
	double n, sr, srr;
	double sc[3], src[3], scc[3];
	double rmin, rmax;
	float cmin[3], cmax[3];
};

struct bink_gradient
{
	float a[3], b[3];    /* color = a + b*r, in 0..255 */
	float lo[3], hi[3];  /* clamp for the evaluated color */
	int radial;          /* nonzero: the radial fit was accepted, zero: flat fallback */
};

static void bink_gradient_sums_reset(
	struct bink_gradient_sums *sums)
{
	int channel;

	sums->n = sums->sr = sums->srr = 0.0;
	sums->rmin = 1e30;
	sums->rmax = -1e30;
	for (channel = 0; channel < 3; channel++)
	{
		sums->sc[channel] = sums->src[channel] = sums->scc[channel] = 0.0;
		sums->cmin[channel] = 255.0f;
		sums->cmax[channel] = 0.0f;
	}
}

static void bink_gradient_sums_add(
	struct bink_gradient_sums *sums,
	double r,
	const float *rgb)
{
	int channel;

	sums->n += 1.0;
	sums->sr += r;
	sums->srr += r * r;
	if (r < sums->rmin) sums->rmin = r;
	if (r > sums->rmax) sums->rmax = r;
	for (channel = 0; channel < 3; channel++)
	{
		double c = rgb[channel];

		sums->sc[channel] += c;
		sums->src[channel] += r * c;
		sums->scc[channel] += c * c;
		if (rgb[channel] < sums->cmin[channel]) sums->cmin[channel] = rgb[channel];
		if (rgb[channel] > sums->cmax[channel]) sums->cmax[channel] = rgb[channel];
	}
}

/* returns nonzero when the radial fit was accepted */
static int bink_gradient_solve(
	const struct bink_gradient_sums *sums,
	struct bink_gradient *gradient)
{
	double n = sums->n;
	double var_r;
	int channel;
	int radial = 0;

	for (channel = 0; channel < 3; channel++)
	{
		gradient->a[channel] = 0.0f;
		gradient->b[channel] = 0.0f;
		gradient->lo[channel] = 0.0f;
		gradient->hi[channel] = 255.0f;
	}
	gradient->radial = 0;
	if (n < 2.0)
		return 0;

	var_r = sums->srr - sums->sr * sums->sr / n;
	if (var_r > 0.0 && (sums->rmax - sums->rmin) >= BINK_GRADIENT_MINIMUM_SPREAD)
	{
		double worst_mse = 0.0;
		float a[3], b[3];

		for (channel = 0; channel < 3; channel++)
		{
			double cov = sums->src[channel] - sums->sr * sums->sc[channel] / n;
			double slope = cov / var_r;
			double intercept = (sums->sc[channel] - slope * sums->sr) / n;
			/* sum of squared error of the line: Σc² - aΣc - bΣrc */
			double sse = sums->scc[channel] - intercept * sums->sc[channel] - slope * sums->src[channel];
			double mse = sse > 0.0 ? sse / n : 0.0;

			a[channel] = (float)intercept;
			b[channel] = (float)slope;
			if (mse > worst_mse) worst_mse = mse;
		}
		if (sqrt(worst_mse) <= BINK_GRADIENT_MAXIMUM_RMS)
		{
			for (channel = 0; channel < 3; channel++)
			{
				gradient->a[channel] = a[channel];
				gradient->b[channel] = b[channel];
			}
			radial = 1;
		}
	}

	if (!radial)
	{
		for (channel = 0; channel < 3; channel++)
			gradient->a[channel] = (float)(sums->sc[channel] / n);
	}
	for (channel = 0; channel < 3; channel++)
	{
		float lo = sums->cmin[channel] - BINK_GRADIENT_RANGE_MARGIN;
		float hi = sums->cmax[channel] + BINK_GRADIENT_RANGE_MARGIN;

		gradient->lo[channel] = lo < 0.0f ? 0.0f : lo;
		gradient->hi[channel] = hi > 255.0f ? 255.0f : hi;
	}
	gradient->radial = radial;
	return radial;
}

/* moves dst toward src by fraction k (smoothing over frames) */
static void bink_gradient_blend(
	struct bink_gradient *dst,
	const struct bink_gradient *src,
	float k)
{
	int channel;

	for (channel = 0; channel < 3; channel++)
	{
		dst->a[channel] += (src->a[channel] - dst->a[channel]) * k;
		dst->b[channel] += (src->b[channel] - dst->b[channel]) * k;
		dst->lo[channel] += (src->lo[channel] - dst->lo[channel]) * k;
		dst->hi[channel] += (src->hi[channel] - dst->hi[channel]) * k;
	}
	dst->radial = src->radial;
}

static void bink_gradient_eval(
	const struct bink_gradient *gradient,
	float r,
	float *rgb)
{
	int channel;

	for (channel = 0; channel < 3; channel++)
	{
		float c = gradient->a[channel] + gradient->b[channel] * r;

		rgb[channel] = c < gradient->lo[channel] ? gradient->lo[channel] :
			(c > gradient->hi[channel] ? gradient->hi[channel] : c);
	}
}

#endif
