/* Exercise the production menu window without a renderer or game data. */
#include <stdio.h>
#include <stdlib.h>
#include "menu_scale.h"

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 4) return 1;
    float top, bottom, scale;
    menu_scale_window(strtof(argv[1], NULL),
        argc == 4 ? strtof(argv[2], NULL) : MENU_CONTENT_TOP,
        argc == 4 ? strtof(argv[3], NULL) : MENU_CONTENT_BOTTOM, &top, &bottom, &scale);
    printf("%.9g %.9g %.9g\n", top, bottom, scale);
    return 0;
}
