/* Apache-2.0. Optional decoded player-source fixture exercises the approved redraw. */
#include "menu_background_redraw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv)
{
    unsigned char source[4 * 512 * 4] = {0};
    unsigned char output[4 * 512 * 4 * 16], before[sizeof(output)];
    memset(output, 73, sizeof(output)); memcpy(before, output, sizeof(output));
    assert(menu_background_redraw_source(source, 4, 512) == -1);
    assert(menu_background_redraw(source, 4, 512, output, 4, NULL, NULL) == 0);
    assert(!memcmp(output, before, sizeof(output)));
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb"); assert(file);
        assert(fread(source, 1, sizeof(source), file) == sizeof(source)); fclose(file);
        assert(menu_background_redraw_source(source, 4, 512) == 0);
        volatile int cancel = 1;
        assert(menu_background_redraw(source, 4, 512, output, 4, &cancel, NULL) == -1);
        assert(!memcmp(output, before, sizeof(output)));
        struct menu_background_redraw_metrics metrics;
        assert(menu_background_redraw(source, 4, 512, output, 4, NULL, &metrics) == 1);
        /* The two bright lines retain their source rows, color, opacity and width. */
        for (unsigned row = 0; row < 2; row++) {
            unsigned y = row ? 409 : 70;
            for (unsigned dy = 0; dy < 4; dy++)
                assert(!memcmp(output + ((y * 4 + dy) * 16) * 4, source + y * 16, 4));
        }
        for (unsigned y = 0; y < 2048; y++)
            for (unsigned x = 1; x < 16; x++)
                assert(!memcmp(output + (y * 16 + x) * 4, output + y * 16 * 4, 4));
        source[0] ^= 1; memcpy(output, before, sizeof(output));
        assert(menu_background_redraw(source, 4, 512, output, 4, NULL, NULL) == 0);
        assert(!memcmp(output, before, sizeof(output)));
        printf("background redraw %.6f seconds, %zu scratch bytes\n", metrics.seconds, metrics.scratch_bytes);
    }
    puts("menu background redraw passed");
    return 0;
}
