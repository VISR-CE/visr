/* Names only: Apple apps rasterize SF Symbols at runtime, never in assets. */
#ifndef HALO_KEYBOARD_SYMBOLS_H
#define HALO_KEYBOARD_SYMBOLS_H
#include <stdio.h>
#include <string.h>

struct halo_keyboard_symbol { const char *key, *symbol; };
static const struct halo_keyboard_symbol halo_keyboard_symbols[] = {
    {"Enter", "return"}, {"Return", "return"}, {"Keypad Enter", "return"},
    {"Escape", "escape"}, {"Esc", "escape"}, {"Tab", "arrow.right.to.line"},
    {"Shift", "shift"}, {"Left Shift", "shift"}, {"Right Shift", "shift"},
    {"Command", "command"}, {"Left GUI", "command"}, {"Right GUI", "command"},
    {"Alt", "option"}, {"Left Alt", "option"}, {"Right Alt", "option"},
    {"Ctrl", "control"}, {"Left Ctrl", "control"}, {"Right Ctrl", "control"},
    {"Backspace", "delete.left"}, {"Delete", "delete.right"}, {"Space", "space"},
    {"Up", "arrow.up"}, {"Down", "arrow.down"}, {"Left", "arrow.left"}, {"Right", "arrow.right"},
    {"CapsLock", "capslock"}, {"Home", "arrow.up.to.line"}, {"End", "arrow.down.to.line"},
    {"PageUp", "arrow.up"}, {"PageDown", "arrow.down"},
    {"Mouse", "computermouse"}, {"Mouse Left", "computermouse"}, {"Mouse Right", "computermouse"},
    {"Mouse Middle", "computermouse"}, {"WASD", "arrow.up.and.down.and.arrow.left.and.right"},
    {"=", "equal"}, {"-", "minus"}, {"+", "plus"}, {"/", "slash"},
    {".", "period"}, {",", "comma"}, {";", "semicolon"}, {"'", "apostrophe"},
    {"[", "leftbracket"}, {"]", "rightbracket"}, {"\\", "backslash"}, {"`", "grave"},
};

/* Try the key's own square first. If the SDK lacks it, use square plus
   the returned plain symbol. Availability is checked by the native loader. */
static inline int halo_keyboard_symbol_plan(const char *key, char *square, unsigned int capacity,
    const char **overlay)
{
    unsigned int index;
    const char *symbol = NULL;
    char letter[2] = {0};
    if (!key || !*key || !square || capacity < 2 || !overlay) return 0;
    if (!key[1] && ((key[0] >= 'A' && key[0] <= 'Z') ||
        (key[0] >= 'a' && key[0] <= 'z') || (key[0] >= '0' && key[0] <= '9')))
    {
        letter[0] = key[0] >= 'A' && key[0] <= 'Z' ? key[0] + ('a' - 'A') : key[0];
        symbol = letter;
        *overlay = NULL;
    }
    else
    {
        for (index = 0; index < sizeof(halo_keyboard_symbols) / sizeof(halo_keyboard_symbols[0]); index++)
            if (!strcmp(key, halo_keyboard_symbols[index].key)) symbol = halo_keyboard_symbols[index].symbol;
        /* Function keys have their own square symbols on supporting SDKs. */
        if (!symbol && key[0] == 'F' && key[1] >= '1' && key[1] <= '9' &&
            (!key[2] || (key[2] >= '0' && key[2] <= '9' && !key[3])))
        {
            int written = snprintf(square, capacity, "f%s.square", key + 1);
            *overlay = NULL;
            return written > 0 && (unsigned int)written < capacity;
        }
        if (!symbol) symbol = "keyboard";
        *overlay = symbol;
    }
    int written = snprintf(square, capacity, "%s.square", symbol);
    return written > 0 && (unsigned int)written < capacity;
}
#endif
