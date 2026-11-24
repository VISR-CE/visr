/* the alpha pin of the Xbox renderer's constants (alpha_pin.h): inside 0 to 1 unchanged; below 0, above 1, the
   infinities and NaN (zero over zero) land on 0 or 1, never outside, so the 0-to-1 assertion of
   real_alpha_to_pixel32 cannot fire (the Mac halt in render_window) */
#include "alpha_pin.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    assert(alpha_pin(0.0f) == 0.0f && alpha_pin(1.0f) == 1.0f && alpha_pin(0.25f) == 0.25f);
    assert(alpha_pin(-0.0001f) == 0.0f && alpha_pin(-1e30f) == 0.0f && alpha_pin(-INFINITY) == 0.0f);
    assert(alpha_pin(1.0001f) == 1.0f && alpha_pin(1e30f) == 1.0f && alpha_pin(INFINITY) == 1.0f);
    assert(alpha_pin(NAN) == 0.0f);
    assert(alpha_pin(0.0f / 0.0f * 1.0f) == 0.0f);
    /* a density ratio with the near density past the far one, and a negative one */
    assert(alpha_pin(3.0f / 2.0f) == 1.0f && alpha_pin(-1.0f / 2.0f) == 0.0f);
    printf("alpha pin probe ok\n");
    return 0;
}
