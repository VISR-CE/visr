/* Bink bar fill: a smooth radial edge is continued outward; anything else falls back to flat. */
#include "../../../source/bink/bink_edge_gradient.h"
#include <assert.h>
#include <math.h>

static int near(float a, float b, float t) { return fabsf(a - b) < t; }

static void sample(struct bink_gradient_sums *sums, float a0, float b0, float noise)
{
	int i;

	bink_gradient_sums_reset(sums);
	for (i = 0; i < 200; i++)
	{
		double r = 0.5 + 0.5 * i / 199.0;
		float rgb[3];
		float wobble = (i & 1) ? noise : -noise;

		rgb[0] = a0 + b0 * (float)r + wobble;
		rgb[1] = 200.0f + wobble;
		rgb[2] = 230.0f - 10.0f * (float)r + wobble;
		bink_gradient_sums_add(sums, r, rgb);
	}
}

int main(void)
{
	struct bink_gradient_sums sums;
	struct bink_gradient g, h;
	float rgb[3];

	/* a clean radial falloff is recovered and extrapolated past the sampled radius */
	sample(&sums, 290.0f, -109.0f, 0.5f);
	assert(bink_gradient_solve(&sums, &g) && g.radial);
	assert(near(g.a[0], 290.0f, 1.0f) && near(g.b[0], -109.0f, 1.5f));
	bink_gradient_eval(&g, 1.1f, rgb);
	assert(near(rgb[0], 290.0f - 109.0f * 1.1f, 6.0f)); /* about 170 */
	/* far past the samples the value is held inside the sampled range plus the margin */
	bink_gradient_eval(&g, 50.0f, rgb);
	assert(rgb[0] >= 0.0f && rgb[0] >= sums.cmin[0] - BINK_GRADIENT_RANGE_MARGIN - 0.01f);

	/* a noisy or busy edge falls back to the flat mean */
	sample(&sums, 200.0f, -20.0f, 30.0f);
	assert(!bink_gradient_solve(&sums, &g) && !g.radial);
	assert(near(g.b[0], 0.0f, 0.001f) && near(g.a[1], 200.0f, 1.0f));

	/* all samples at one radius: no slope can be trusted */
	bink_gradient_sums_reset(&sums);
	{
		float c[3] = { 100.0f, 110.0f, 120.0f };
		int i;

		for (i = 0; i < 10; i++)
			bink_gradient_sums_add(&sums, 0.7, c);
	}
	assert(!bink_gradient_solve(&sums, &g) && near(g.a[2], 120.0f, 0.01f));

	/* flat white stays white */
	bink_gradient_sums_reset(&sums);
	{
		float c[3] = { 254.0f, 254.0f, 254.0f };
		int i;

		for (i = 0; i < 40; i++)
			bink_gradient_sums_add(&sums, 0.5 + i * 0.0125, c);
	}
	bink_gradient_solve(&sums, &g);
	bink_gradient_eval(&g, 1.2f, rgb);
	assert(near(rgb[0], 254.0f, 0.5f) && near(rgb[2], 254.0f, 0.5f));

	/* blending moves toward the target */
	sample(&sums, 290.0f, -109.0f, 0.5f);
	bink_gradient_solve(&sums, &g);
	h = g;
	h.a[0] = 0.0f;
	bink_gradient_blend(&h, &g, 0.5f);
	assert(near(h.a[0], g.a[0] * 0.5f, 0.01f));
	return 0;
}
