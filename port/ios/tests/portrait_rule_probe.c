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
    /* the checks as the scene moves: each row is the screen it is on (0x0: no screen yet), whether it is in
       portrait, and what the check must decide */
    struct { double w, h; int portrait, answer, changed, turn; } rows[] = {
        {0, 0, 0, -1, 0, 0},       /* configure before the scene connects: no answer yet */
        {457, 678, 1, 0, 1, 1},    /* cold launch on the cover display held in portrait: back to landscape */
        {678, 457, 0, 0, 0, 0},    /* the rotation that follows: nothing new */
        {951, 669, 0, 1, 1, 0},    /* unfold to the inner display in landscape: portrait allowed */
        {669, 951, 1, 1, 0, 0},    /* turn it to portrait there */
        {457, 678, 1, 0, 1, 1},    /* fold to the cover display in portrait: back to landscape */
        {669, 951, 0, 1, 1, 0},    /* unfold again */
    };
    int answer = -1;
    for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        struct host_portrait_step step = host_portrait_check(answer, 1, rows[i].w > 0, rows[i].w, rows[i].h,
                                                             rows[i].portrait);
        if (step.answer != rows[i].answer || step.changed != rows[i].changed ||
            step.turn_to_landscape != rows[i].turn) {
            printf("FAIL: row %u: answer %d changed %d turn %d\n", i, step.answer, step.changed,
                   step.turn_to_landscape);
            return 1;
        }
        answer = step.answer;
    }
    /* a device that does not opt in never answers, whatever the screen */
    assert(host_portrait_check(-1, 0, 1, 669, 951, 1).changed == 0);
    puts("PASS: portrait only where a menu unit is at least a point");
    return 0;
}
