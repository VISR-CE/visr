"""Exercise measured single-line name truncation."""
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

from test_network_discovery import function

ROOT = Path(__file__).resolve().parents[1]


def test_browser_text_fit(tmp_path):
    ui = (ROOT / 'source/interface/ui_widget.c').read_text()
    production = function(ui, 'static short ui_widget_port_text_width(')
    production += '\n' + function(ui, 'void ui_widget_port_text_ellipsize(')
    production += '\n' + function(ui, 'short ui_widget_port_font_text_width(')
    fixture = r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include "p2p.h"
#define NONE -1
typedef int boolean;
#define SHORT_MAX 32767
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
typedef struct { short y0, x0, y1, x1; } rectangle2d;
struct widget_instance { long definition_tag_index; };
struct ui_widget_definition { rectangle2d bounds; struct { long index; } text_font; short horizontal_offset; };
struct font_header { int ellipsis; };
__attribute__((unused)) static struct font_header fonts[2] = {{0}, {1}};
static struct ui_widget_definition cell_definition = {{0,0,18,33}, {0}, 2};
static long current_font;
#define ui_widget_definition_get(index) (&cell_definition)
#define font_definition_get(index) (&fonts[index])
static long draw_string_get_font(void) { return current_font; }
/* set_font looks the index up as one of this map's fonts; restore must not,
because the saved index can be stale (another map's, NONE or the zeroed 0) */
static void draw_string_set_font(long font) { assert(font == 0 || font == 1); current_font=font; }
static void draw_string_restore_font(long font) { current_font=font; }
static void draw_string_set_format(short style, short justification, unsigned long flags) {
    assert(style == NONE && justification == 0 && flags == 0);
}
static void draw_string_set_indents(short initial, short paragraph) { assert(!initial && !paragraph); }
__attribute__((unused)) static void *font_get_character_by_ascii_code(struct font_header *font, unsigned short character) {
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
/* PRODUCTION */
int main(void) {
    struct widget_instance box = {0};
    wchar_t text[32];
    wcscpy(text, L"WWWW"); current_font=1;
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"WW...")); assert(current_font == 1);
    cell_definition.text_font.index=1;
    wcscpy(text, L"WWWW");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"WW..."));
    wcscpy(text, L"iiiiiiii");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"iiiiiiii"));
    cell_definition.bounds.x1=32; cell_definition.horizontal_offset=0;
    wcscpy(text, L"WWW");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"WW..."));
    cell_definition.bounds.x1=28;
    wcscpy(text, L"WWWW");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(!wcscmp(text, L"W..."));
    cell_definition.bounds.x1=6;
    wcscpy(text, L"WWWW");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(text[0] == 0);
    wcscpy(text, L"WWWW");
    ui_widget_port_text_ellipsize(&box, text, 2);
    assert(text[0] == 0);
    wcscpy(text, L"");
    ui_widget_port_text_ellipsize(&box, text, 32);
    assert(text[0] == 0);
    /* a previous map's font index survives into a new map's menu building */
    current_font=227;
    assert(ui_widget_port_font_text_width(1, L"WW") > 0); assert(current_font == 227);
    current_font=NONE;
    assert(ui_widget_port_font_text_width(1, L"WW") > 0); assert(current_font == NONE);
    current_font=227; cell_definition.bounds.x1=30;
    wcscpy(text, L"WWWW");
    ui_widget_port_text_ellipsize(&box, text, 32); assert(current_font == 227);
    return 0;
}
'''
    probe, binary = tmp_path / 'text.c', tmp_path / 'text'
    probe.write_text(fixture.replace('/* PRODUCTION */', production))
    subprocess.run(['clang', '-std=c11', '-Wall', '-Werror', '-iquote',
                    str(ROOT / 'port/linux/src'), str(probe), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
