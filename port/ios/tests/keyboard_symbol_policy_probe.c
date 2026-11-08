#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include "halo_keyboard_symbols.h"
#include "menu_legend_strings.h"
int main(void)
{
    char square[96];
    const char *overlay;
    for (int index = 0; index < 36; index++) {
        char key[2] = {index < 26 ? 'A' + index : '0' + index - 26, 0};
        assert(halo_keyboard_symbol_plan(key, square, sizeof(square), &overlay));
        assert(square[0] == (index < 26 ? 'a' + index : '0' + index - 26));
        assert(!strcmp(square + 1, ".square") && !overlay);
    }
    for (unsigned int index = 0; index < sizeof(halo_keyboard_symbols) / sizeof(halo_keyboard_symbols[0]); index++) {
        assert(halo_keyboard_symbol_plan(halo_keyboard_symbols[index].key, square, sizeof(square), &overlay));
        assert(overlay && !strcmp(overlay, halo_keyboard_symbols[index].symbol));
        assert(!strncmp(square, overlay, strlen(overlay)));
        assert(!strcmp(square + strlen(overlay), ".square"));
    }
    assert(halo_keyboard_symbol_plan("Escape", square, sizeof(square), &overlay));
    assert(!strcmp(square, "escape.square") && !strcmp(overlay, "escape"));
    assert(halo_keyboard_symbol_plan("F1", square, sizeof(square), &overlay));
    assert(!strcmp(square, "f1.square") && !overlay);
    assert(!halo_keyboard_symbol_plan("A", square, 2, &overlay));
    assert(!halo_keyboard_symbol_plan("", square, sizeof(square), &overlay));
    assert(halo_keyboard_symbol_plan("Unknown", square, sizeof(square), &overlay));
    assert(!strcmp(square, "keyboard.square") && !strcmp(overlay, "keyboard"));
    wchar_t pause[] = L"%b-button =CANCEL %a-button =SELECT";
    wchar_t keyboard[] = L"=";
    halo_menu_legend_plain(pause);
    assert(!wcscmp(pause, L"%b-button CANCEL %a-button SELECT"));
    halo_menu_legend_plain(keyboard);
    assert(!wcscmp(keyboard, L"="));
    halo_menu_legend_plain(pause);
    assert(!wcschr(pause, L'='));
    assert(!wcschr(halo_menu_legend_fallback(72), L'='));
    assert(!wcschr(halo_menu_legend_fallback(73), L'='));
    assert(!halo_menu_legend_fallback(71));
    return 0;
}
