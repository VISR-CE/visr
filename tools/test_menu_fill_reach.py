"""The menus' idea of the screen matches where their draws land, for every screen size, safe area and size change.

Bug class: menu code that sized something by the screen from its own arithmetic instead of the menu window's
transform (its safe-area zoom and the wide screen's margins). Seen twice: full-screen fills (a dim, the menus' band,
the in-game keyboard's band) sized from the margins alone, (width - 640) / 2, stopped short of both edges while the
safe-area zoom shrank the menus below one (the iPhone Duo's outer display in landscape: a 0.1239 camera inset gives a
zoom of 0.837, and the band covered 0.837 of the width); and controls kept clear of a camera cutout by the inset
times the menu's 640 columns, which ignored the same margins and zoom (and the in-game keyboard, drawn outside the
menu window, sat under the camera band). Both now go through `halo_screen_menu_screen_rect`.

The probe compiles the production helper and the drawing coefficients from `d3d8_device.c`, and the call sites from
`ui_widget.c` (`ui_widget_screen_fill_kind`, `ui_widget_screen_fill_extend`, `ui_widget_safe_left`) and
`virtual_keyboard.c` (`virtual_keyboard_side_strips`), runs them over a sequence of sizes, the way a rotation or a
move between displays changes them under an open screen, and checks each result where it is drawn: fills cover the
whole screen and edge controls clear the safe-area insets.
"""
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu_scale.h"
#include "halo_ui_pointer.h"
#include "halo_ui_layout.h"
#include "halo_menu_screen.h"
#define CONFIG_SCENE_MENU 1
#define TRUE 1
#define FALSE 0
#define NULL_BITMAP_BLEND 0
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
typedef int boolean;
typedef struct { short y0, x0, y1, x1; } rectangle2d;
struct bitmap_data { short width, height; };
enum { _controller_glyph_blend_menu = 1 };
static int height = 402, width = 1044;
static float insets_now[4];
static int zoom_on = 1;
int config_boolean(const char *name) { (void)name; return zoom_on; }
int halo_scene_class(void) { return CONFIG_SCENE_MENU; }
long halo_screen_width(void) { return width; }
int halo_screen_ui_portrait(void) { return width < 640; }
float halo_screen_pixel_scale(void) { return 3; }
void platform_video_window_size(int *w, int *h) { *w = width; *h = height; }
void platform_video_safe_insets(float *insets) { memcpy(insets, insets_now, sizeof(insets_now)); }
int host_sdl_menu_height_points(void) { return height; }
static rectangle2d blits[2];
static int blit_count;
static void controller_glyph_blit(struct bitmap_data *bitmap, rectangle2d const *bounds, rectangle2d const *source,
    void *unused, unsigned color, int blend) {
    (void)bitmap; (void)source; (void)unused; (void)color; (void)blend;
    assert(blit_count < 2);
    blits[blit_count++] = *bounds;
}
__HELPERS__
__CALL_SITES__
struct viewport { float viewport_scale[1][4], viewport_offset[1][4]; };
/* where a menu-unit point lands on the screen, 0..width by 0..480, through the draw's coefficients (the menu
window's while it is drawing) */
static void drawn(float x, float y, float *out_x, float *out_y) {
    float menu_top = 0, menu_zoom = 1;
    if (menu_window_drawing) { menu_top = menu_draw_top; menu_zoom = menu_draw_zoom; }
    struct { float viewport_offset[4]; } device = { { width * 0.5f, 240 } };
    struct viewport uniforms = { { { width * 0.5f, -240 } }, { { width * 0.5f, 240 } } };
    struct viewport *converted = &uniforms;
    __COEFFICIENTS__
    float screen_x = x + (width - 640) / 2.0f;
    float ndc_x = (screen_x - converted->viewport_offset[0][0]) / converted->viewport_scale[0][0];
    float ndc_y = (y - converted->viewport_offset[0][1]) / converted->viewport_scale[0][1];
    *out_x = (ndc_x + 1) * width / 2;
    *out_y = (1 - ndc_y) * 240;
}
/* a rectangle covers the whole screen where it is drawn */
static void covers(rectangle2d r, int across, int down, const char *what) {
    float x0, y0, x1, y1;
    drawn(r.x0, r.y0, &x0, &y0);
    drawn(r.x1, r.y1, &x1, &y1);
    printf("  %s drawn x %.1f..%.1f y %.1f..%.1f\n", what, x0, x1, y0, y1);
    if (across) assert(x0 <= 0.5f && x1 >= width - 0.5f);
    if (down) assert(y0 <= 0.5f && y1 >= 479.5f);
}
static void full_screen_fills(void) {
    struct halo_menu_screen screen;
    struct bitmap_data dim = {2, 2}, band = {2, 256}, picture = {640, 480};
    rectangle2d design = {0, 0, 480, 640}, rect;
    boolean wide, tall;

    halo_screen_menu_screen_rect(TRUE, &screen);
    /* a dim: across and down */
    ui_widget_screen_fill_kind(&design, dim.width, dim.height, &screen, &wide, &tall);
    rect = design;
    ui_widget_screen_fill_extend(&rect, &screen, wide, tall, FALSE);
    covers(rect, 1, 1, "dim");
    rect = design;
    ui_widget_screen_fill_extend(&rect, &screen, wide, tall, TRUE);
    covers(rect, 1, 1, "dim clip");
    /* the band: across, its rules at their own rows */
    ui_widget_screen_fill_kind(&design, band.width, band.height, &screen, &wide, &tall);
    assert(!tall);
    rect = design;
    ui_widget_screen_fill_extend(&rect, &screen, wide, tall, FALSE);
    covers(rect, 1, 0, "band");
    assert(rect.y0 == 0 && rect.y1 == 480);
    /* a picture is drawn texel for texel: never stretched */
    ui_widget_screen_fill_kind(&design, picture.width, picture.height, &screen, &wide, &tall);
    assert(!wide && !tall);
    /* the in-game keyboard's band strips */
    blit_count = 0;
    virtual_keyboard_side_strips(&band);
    rect = design;
    for (int i = 0; i < blit_count; i++) {
        rect.x0 = MIN(rect.x0, blits[i].x0);
        rect.x1 = MAX(rect.x1, blits[i].x1);
    }
    covers(rect, 1, 0, "keyboard band");
}
/* edge controls (the touch BACK chevron, the title's shift, the hold popup, a dropdown) clear the insets */
static void edge_controls(void) {
    struct halo_menu_screen screen;
    float side = fmaxf(insets_now[0], insets_now[1]), x, y, right, bottom;

    halo_screen_menu_screen_rect(TRUE, &screen);
    drawn(ui_widget_safe_left(0), screen.safe_y0, &x, &y);
    drawn(screen.safe_x1, screen.safe_y1, &right, &bottom);
    printf("  safe left %ld drawn %.1f (inset %.1f) right %ld drawn %.1f\n", ui_widget_safe_left(0), x,
        side * width, screen.safe_x1, right);
    assert(x >= side * width - 0.01f && right <= width * (1 - side) + 0.01f);
    assert(y >= insets_now[2] * 480 - 0.01f && bottom <= 480 * (1 - insets_now[3]) + 0.01f);
    /* and no further in than a column's rounding */
    float zoom = menu_window_drawing ? menu_draw_zoom : 1;
    if (ui_widget_safe_left(0) > 0) assert(x < side * width + zoom + 0.01f);
    /* never left of the clip it was asked for */
    assert(ui_widget_safe_left(100) >= 100);
}
int main(int argc, char **argv) {
    /* each argument: width x points-height : left,right,top,bottom insets, in the order a session meets them */
    for (int i = 1; i < argc; i++) {
        assert(sscanf(argv[i], "%dx%d:%f,%f,%f,%f", &width, &height,
            &insets_now[0], &insets_now[1], &insets_now[2], &insets_now[3]) == 6);
        /* ":fixed": the menu window keeps zoom one (debug.menu_min_scale off), so the cutout crosses the design */
        zoom_on = strstr(argv[i], ":fixed") == NULL;
        halo_screen_menu_window(1);
        printf("%s zoom %.3f\n", argv[i], menu_draw_zoom);
        if (width >= 640) {
            full_screen_fills();
            edge_controls();
        } else {
            /* portrait: the design fits the width, and the screen reaches above and below it */
            struct halo_menu_screen screen;
            halo_screen_menu_screen_rect(TRUE, &screen);
            assert(screen.x0 == 0 && screen.x1 == 640 && screen.y0 < 0 && screen.y1 > 480);
            assert(screen.safe_y0 > screen.y0 && screen.safe_y1 < screen.y1);
        }
        halo_screen_menu_window(0);
        /* outside the menu window (the fade): the screen at zoom one */
        if (width >= 640) {
            struct halo_menu_screen screen;
            halo_screen_menu_screen_rect(FALSE, &screen);
            covers((rectangle2d){0, (short)screen.x0, 480, (short)screen.x1}, 1, 1, "fade");
            assert(screen.x0 == -((width - 640 + 1) / 2) || screen.x0 == -((width - 640) / 2));
        }
    }
    return 0;
}
'''


def function(source, signature):
    """One C function's text, from its signature to the closing brace at the start of a line."""
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]


@pytest.fixture(scope="module")
def probe(tmp_path_factory):
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("A C compiler is required for the menu fill probe")
    source = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    helpers = source[source.index("static int menu_window_drawing;"):source.index("/* render_window calls this once")]
    widget = (ROOT / "source/interface/ui_widget.c").read_text()
    keyboard = (ROOT / "source/interface/virtual_keyboard.c").read_text()
    call_sites = "\n".join([
        function(widget, "static long ui_widget_safe_left("),
        function(widget, "static void ui_widget_screen_fill_kind("),
        function(widget, "static void ui_widget_screen_fill_extend("),
        function(keyboard, "static void virtual_keyboard_side_strips("),
    ])
    start = source.index("converted->viewport_scale[0][0] /= menu_zoom;")
    end = source.index("\n", source.index("converted->viewport_offset[0][1] = menu_top", start))
    folder = tmp_path_factory.mktemp("menu_fill_reach")
    path, binary = folder / "reach.c", folder / "reach"
    path.write_text(HARNESS.replace("__HELPERS__", helpers).replace("__CALL_SITES__", call_sites)
                    .replace("__COEFFICIENTS__", source[start:end]))
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-I", str(ROOT / "port/linux/src"), "-idirafter", str(ROOT / "port/linux/include"),
                    str(path), str(ROOT / "port/linux/src/menu_scale.c"), "-lm", "-o", str(binary)], check=True)
    return binary


# width (menu units at 480 lines) x window height (points) : safe-area insets (fractions: left, right, top, bottom)
DUO_OUTER_PORTRAIT = "323x678:0,0,0.087,0.05"
DUO_OUTER_LANDSCAPE = "712x457:0.1239,0,0,0.073"
DUO_OUTER_LANDSCAPE_RIGHT = "712x457:0,0.1239,0,0.073"
DUO_INNER_LANDSCAPE = "682x669:0,0,0,0.0508"
IPHONE_17_PRO = "1043x402:0.1,0.1,0,0.05"
IPAD = "640x834:0,0,0,0"
EXTERNAL_1080P = "853x1080:0,0,0,0"
DEEP_CUTOUT = "700x375:0.2,0.2,0.1,0.11"
DUO_OUTER_LANDSCAPE_FIXED = DUO_OUTER_LANDSCAPE + ":fixed"

SEQUENCES = {
    "rotate portrait to landscape on the Duo outer display": [DUO_OUTER_PORTRAIT, DUO_OUTER_LANDSCAPE],
    "turn the Duo over (cutout moves to the other side)": [DUO_OUTER_LANDSCAPE, DUO_OUTER_LANDSCAPE_RIGHT],
    "unfold from the outer to the inner display": [DUO_OUTER_LANDSCAPE, DUO_INNER_LANDSCAPE, DUO_OUTER_LANDSCAPE],
    "move to an external display and back": [IPHONE_17_PRO, EXTERNAL_1080P, IPHONE_17_PRO],
    "4:3 iPad and a deep cutout": [IPAD, DEEP_CUTOUT, IPAD],
    "the cutout over the design at zoom one": [DUO_OUTER_LANDSCAPE_FIXED, DUO_OUTER_LANDSCAPE, DEEP_CUTOUT + ":fixed"],
}


@pytest.mark.parametrize("sequence", SEQUENCES.values(), ids=SEQUENCES.keys())
def test_menu_screen_matches_the_drawn_screen(probe, sequence):
    result = subprocess.run([str(probe), *sequence], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_duo_outer_landscape_zooms_below_one(probe):
    """The case Steve saw: the margins alone would cover only the zoom's share of the width."""
    output = subprocess.check_output([str(probe), DUO_OUTER_LANDSCAPE], text=True)
    zoom = float(output.split("zoom ")[1].split()[0])
    assert zoom == pytest.approx(712 * (1 - 2 * 0.1239) / 640, abs=0.002)
    # the band reaches past the margins alone, (712 - 640) / 2 = 36 columns each side
    band = output.split("  band drawn x ")[1].split()[0]
    assert float(band.split("..")[0]) <= 0.5



def test_cutout_over_the_design_moves_edge_controls(probe):
    """At zoom one the Duo's camera band (88 columns from the screen's edge) crosses the 36-column margin:
    edge controls start 52 columns into the design, not the inset times 640 (79) the old arithmetic gave."""
    output = subprocess.check_output([str(probe), DUO_OUTER_LANDSCAPE_FIXED], text=True)
    assert "safe left 53 " in output
