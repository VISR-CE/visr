/*
ALPHA_PIN.H

The alpha the Xbox renderer's constants carry (real_alpha_to_pixel32,
bitmaps_inlines.h) is asserted to lie in 0 to 1, and a tag's density ratio,
brightness or tint can leave that range (and a ratio of two zero densities is
NaN). The conversion pins its value here first. Header-only so that
port/ios/tests/alpha_pin_probe.c checks the edges without the game.
*/

#ifndef __ALPHA_PIN_H
#define __ALPHA_PIN_H

/* the value within 0 to 1; NaN and anything below 0 give 0, anything above 1 gives 1 */
static inline float alpha_pin(float alpha)
{
	if (!(alpha >= 0.0f))
		return 0.0f;
	if (alpha > 1.0f)
		return 1.0f;
	return alpha;
}

#endif
