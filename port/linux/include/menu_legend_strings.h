/* Port overrides for menu legends loaded from original string lists. */
#ifndef MENU_LEGEND_STRINGS_H
#define MENU_LEGEND_STRINGS_H
#include <stddef.h>

/* String-list entries are writable tag data. Keep glyph tokens, whitespace,
   and captions intact while dropping the old "means" separator. A lone
   equals sign is a virtual keyboard character, so leave it available. */
static inline void halo_menu_legend_plain(wchar_t *text)
{
    wchar_t *write = text;
    if (!text || !text[0] || !text[1]) return;
    for (; *text; text++)
        if (*text != L'=') *write++ = *text;
    *write = 0;
}

/* NTSC fallback text is immutable; override these two port strings instead
   of editing the original fallback table in text_group.c. */
static inline wchar_t *halo_menu_legend_fallback(short index)
{
    if (index == 72) return L"\t%b-button quit    %a-button pick game";
    if (index == 73) return L"\t%b-button quit";
    return 0;
}
#endif
