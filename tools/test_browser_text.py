"""Exercise browser region text and measured single-line name truncation."""
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

from test_network_discovery import function

ROOT = Path(__file__).resolve().parents[1]


def test_browser_text_fit_and_regions(tmp_path):
    ui = (ROOT / 'source/interface/ui_widget.c').read_text()
    menu = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    production = function(ui, 'static short ui_widget_port_text_width(')
    production += '\n' + function(ui, 'void ui_widget_port_text_ellipsize(')
    for signature in ('static long player_capacity(void)', 'static boolean player_capacity_exceeded(',
                      'static boolean player_capacity_marked(', 'static void browser_listing_region('):
        production += '\n' + function(menu, signature)
    fixture = r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include "p2p.h"
#define NONE -1
typedef int boolean;
static long capacity = 8;
static long config_integer(char const *name) { (void)name; return capacity; }
#define SHORT_MAX 32767
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
typedef struct { short y0, x0, y1, x1; } rectangle2d;
struct widget_instance { long definition_tag_index; };
struct ui_widget_definition { rectangle2d bounds; struct { long index; } text_font; short horizontal_offset; };
struct font_header { int ellipsis; };
static struct font_header fonts[2] = {{0}, {1}};
static struct ui_widget_definition cell_definition = {{0,0,18,30}, {0}, 2};
static long current_font;
#define ui_widget_definition_get(index) (&cell_definition)
#define font_definition_get(index) (&fonts[index])
static long draw_string_get_font(void) { return current_font; }
static void draw_string_set_font(long font) { current_font=font; }
static void draw_string_set_format(short style, short justification, unsigned long flags) {
    assert(style == NONE && justification == 0 && flags == 0);
}
static void draw_string_set_indents(short initial, short paragraph) { assert(!initial && !paragraph); }
static void *font_get_character_by_ascii_code(struct font_header *font, unsigned short character) {
    assert(character == 0x2026); return font->ellipsis ? font : NULL;
}
/* Variable advances and bitmap overhang make character-count truncation fail. */
static void draw_unicode_string_compute_bounds(rectangle2d const *bounds, wchar_t const *text,
    rectangle2d *ink, rectangle2d *cursor) {
    short width = 1, overhang = 0;
    assert(bounds->x1 == SHORT_MAX);
    for (; *text; text++) {
        width += *text == L'W' ? 9 : *text == L'.' ? 2 : *text == 0x2026 ? 5 : 3;
        overhang = *text == L'W' ? 2 : 0;
    }
    *cursor = *ink = *bounds; cursor->x0 = width; ink->x1 = width + overhang;
}
/* stand-ins for the region column's helpers (menu_functions.c) */
#include <stdarg.h>
#include <stdio.h>
static int halo_network_kind_reason(int kind) { (void)kind; return 0; }
enum p2p_server_kind p2p_listing_server_kind(const struct p2p_listing *l) { return l->kind == 9 ? P2P_SERVER_LAN : P2P_SERVER_UNKNOWN; }
static void usnprintf(wchar_t *t, int size, wchar_t const *format, ...) { va_list a; va_start(a, format); vswprintf(t, size + 1, format, a); va_end(a); }
static void text_to_wide(char const *s, wchar_t *w, short size) { int i = 0; for (; s[i] && i < size - 1; i++) w[i] = s[i]; w[i] = 0; }
/* PRODUCTION */
int main(void) {
    struct widget_instance box = {0};
    struct p2p_listing listing = {0};
    wchar_t text[32];
    listing.joinable = 1; listing.open = 1; listing.maximum_player_count = 16;
    browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"-"));
    strcpy(listing.region, "na-west");
    browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"na-west"));
    listing.kind = 9; browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"LAN"));
    listing.kind = 0; listing.player_count = 4; listing.maximum_player_count = 16;
    browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"BIG"));
    /* Master provenance and region must never hide the same BIG mark that gates joining. */
    for (int master = 0; master <= 1; master++) {
        listing.from_master = master;
        for (int dedicated = 0; dedicated <= 1; dedicated++) {
            listing.dedicated = dedicated;
            browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"BIG"));
            listing.player_count = 3;
            browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"na-west"));
            listing.player_count = 4; capacity = 0;
            browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"na-west"));
            capacity = 16;
            browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"na-west"));
            capacity = 8;
        }
    }
    listing.open = 0; browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"CLOSED"));
    listing.failed = 1; browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"FAILED"));
    listing.failed = 0; listing.joinable = 0; listing.network_version = 38;
    browser_listing_region(&listing, text, 32); assert(!wcscmp(text, L"V38"));
    wcscpy(text, L"WWWW"); current_font=1;
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"WW...")); assert(current_font == 1);
    cell_definition.text_font.index=1;
    wcscpy(text, L"WWWW");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"WW\x2026"));
    wcscpy(text, L"iiiiiiii");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"iiiiiiii"));
    cell_definition.bounds.x1=29; cell_definition.horizontal_offset=0;
    wcscpy(text, L"WWW");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"WW\x2026"));
    cell_definition.bounds.x1=25;
    wcscpy(text, L"WWWW");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"WW\x2026"));
    cell_definition.bounds.x1=3;
    wcscpy(text, L"WWWW");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(text[0] == 0);
    wcscpy(text, L"WWWW");
    ui_widget_port_text_ellipsize(&box, text, 2);
    assert(text[0] == 0);
    wcscpy(text, L"");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(text[0] == 0);
    return 0;
}
'''
    probe, binary = tmp_path / 'text.c', tmp_path / 'text'
    probe.write_text(fixture.replace('/* PRODUCTION */', production))
    subprocess.run(['clang', '-std=c11', '-Wall', '-Werror', '-iquote',
                    str(ROOT / 'port/linux/src'), str(probe), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_pc_browser_region_header_matches_generator():
    import sys
    sys.path.insert(0, str(ROOT / 'tools'))
    import port_settings
    name = 'main_menu/multiplayer_type_select/join_game/join_game_column_headers'
    strings = ET.parse(ROOT / 'port/assets/menus/ce/strings.xml').find(f"strings[@name='{name}']")
    assert strings[-1].get('text') == 'Region'
    assert port_settings.STRING_OVERRIDES[name][-1] == 'Region'
