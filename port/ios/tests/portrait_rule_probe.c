/* Portrait only on a screen whose menus get at least a point per menu unit (host_portrait_rule.h). */
#include <assert.h>
#include <stdio.h>
#include "host_portrait_rule.h"

int main(void)
{
    /* the iPhone Duo's inner display (669x951 points) qualifies in either orientation; its outer display
       (about 457x678) does not, nor a phone (402x874) */
    assert(host_portrait_fits(1, 669, 951) && host_portrait_fits(1, 951, 669));
    assert(!host_portrait_fits(1, 457, 678) && !host_portrait_fits(1, 678, 457));
    assert(!host_portrait_fits(1, 402, 874));
    /* an iPad qualifies by size but only where its device opts in */
    assert(host_portrait_fits(1, 834, 1194) && !host_portrait_fits(0, 834, 1194));
    /* the boundary: exactly a point per unit */
    assert(host_portrait_fits(1, 640, 900) && !host_portrait_fits(1, 639.5, 900));
    /* a scene moving between screens takes the answer of the screen it is on: inner, outer, inner */
    const double screens[][2] = {{669, 951}, {457, 678}, {669, 951}};
    const int expected[] = {1, 0, 1};
    for (int i = 0; i < 3; i++) assert(host_portrait_fits(1, screens[i][0], screens[i][1]) == expected[i]);
    puts("PASS: portrait only where a menu unit is at least a point");
    return 0;
}
