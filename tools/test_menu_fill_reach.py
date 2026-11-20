"""Full-screen menu fills reach every edge of the screen, for every screen size, safe area and size change.

Bug class: a full-screen fill (a dim, the menus' band, a fade) sized from the wide screen's margins alone,
(width - 640) / 2, stopped short of both edges while the safe-area zoom shrank the menus below one (the iPhone
Duo's outer display in landscape: a 0.1239 camera inset gives a zoom of 0.837, and the band covered 0.837 of the
width). The probe runs the production helpers (`halo_screen_menu_side_columns`, `halo_screen_menu_visible_rows`,
`menu_window_geometry`) and the drawing coefficients from `d3d8_device.c` over a sequence of sizes, the way a
rotation or a move between displays changes them under an open screen, and checks that the fill's extent, drawn
through the menu zoom, covers the whole screen each time.
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
#define CONFIG_SCENE_MENU 1
static int height = 402, width = 1044;
static float insets_now[4];
int config_boolean(const char *name) { (void)name; return 1; }
int halo_scene_class(void) { return CONFIG_SCENE_MENU; }
long halo_screen_width(void) { return width; }
int halo_screen_ui_portrait(void) { return width < 640; }
float halo_screen_pixel_scale(void) { return 3; }
void platform_video_window_size(int *w, int *h) { *w = width; *h = height; }
void platform_video_safe_insets(float *insets) { memcpy(insets, insets_now, sizeof(insets_now)); }
int host_sdl_menu_height_points(void) { return height; }
__HELPERS__
struct viewport { float viewport_scale[1][4], viewport_offset[1][4]; };
/* where a menu-unit point lands on the screen, 0..width by 0..480, through the menu draw's coefficients */
static void drawn(float x, float y, float *out_x, float *out_y) {
    float menu_top = menu_draw_top, menu_zoom = menu_draw_zoom;
    struct { float viewport_offset[4]; } device = { { width * 0.5f, 240 } };
    struct viewport uniforms = { { { width * 0.5f, -240 } }, { { width * 0.5f, 240 } } };
    struct viewport *converted = &uniforms;
    __COEFFICIENTS__
    float screen_x = x + (width - 640) / 2;
    float ndc_x = (screen_x - converted->viewport_offset[0][0]) / converted->viewport_scale[0][0];
    float ndc_y = (y - converted->viewport_offset[0][1]) / converted->viewport_scale[0][1];
    *out_x = (ndc_x + 1) * width / 2;
    *out_y = (1 - ndc_y) * 240;
}
int main(int argc, char **argv) {
    /* each argument: width x points-height : left,right,top,bottom insets, in the order a session meets them */
    for (int i = 1; i < argc; i++) {
        assert(sscanf(argv[i], "%dx%d:%f,%f,%f,%f", &width, &height,
            &insets_now[0], &insets_now[1], &insets_now[2], &insets_now[3]) == 6);
        halo_screen_menu_window(1);
        long side = halo_screen_menu_side_columns(), top_row, bottom_row;
        halo_screen_menu_visible_rows(&top_row, &bottom_row);
        float x0, y0, x1, y1;
        drawn((float)-side, (float)top_row, &x0, &y0);
        drawn((float)(640 + side), (float)bottom_row, &x1, &y1);
        printf("%s zoom %.3f side %ld rows %ld..%ld drawn x %.1f..%.1f y %.1f..%.1f\n", argv[i], menu_draw_zoom,
            side, top_row, bottom_row, x0, x1, y0, y1);
        if (width >= 640) {
            assert(x0 <= 0.5f && x1 >= width - 0.5f);
            assert(y0 <= 0.5f && y1 >= 479.5f);
            /* never narrower than the margins (the wide screen at zoom one) */
            assert(side >= (width - 640) / 2);
        }
        halo_screen_menu_window(0);
        /* outside a menu draw (the fade): the margins alone */
        assert(halo_screen_menu_side_columns() == (width > 640 ? (width - 640 + 1) / 2 : 0));
    }
    return 0;
}
'''


@pytest.fixture(scope="module")
def probe(tmp_path_factory):
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("A C compiler is required for the menu fill probe")
    source = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    helpers = source[source.index("static int menu_window_drawing;"):source.index("/* render_window calls this once")]
    start = source.index("converted->viewport_scale[0][0] /= menu_zoom;")
    end = source.index("\n", source.index("converted->viewport_offset[0][1] = menu_top", start))
    folder = tmp_path_factory.mktemp("menu_fill_reach")
    path, binary = folder / "reach.c", folder / "reach"
    path.write_text(HARNESS.replace("__HELPERS__", helpers).replace("__COEFFICIENTS__", source[start:end]))
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

SEQUENCES = {
    "rotate portrait to landscape on the Duo outer display": [DUO_OUTER_PORTRAIT, DUO_OUTER_LANDSCAPE],
    "turn the Duo over (cutout moves to the other side)": [DUO_OUTER_LANDSCAPE, DUO_OUTER_LANDSCAPE_RIGHT],
    "unfold from the outer to the inner display": [DUO_OUTER_LANDSCAPE, DUO_INNER_LANDSCAPE, DUO_OUTER_LANDSCAPE],
    "move to an external display and back": [IPHONE_17_PRO, EXTERNAL_1080P, IPHONE_17_PRO],
    "4:3 iPad and a deep cutout": [IPAD, DEEP_CUTOUT, IPAD],
}


@pytest.mark.parametrize("sequence", SEQUENCES.values(), ids=SEQUENCES.keys())
def test_full_screen_fill_reaches_every_edge(probe, sequence):
    result = subprocess.run([str(probe), *sequence], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_duo_outer_landscape_zooms_below_one(probe):
    """The case Steve saw: the margins alone would cover only the zoom's share of the width."""
    output = subprocess.check_output([str(probe), DUO_OUTER_LANDSCAPE], text=True)
    zoom = float(output.split("zoom ")[1].split()[0])
    side = int(output.split("side ")[1].split()[0])
    assert zoom == pytest.approx(712 * (1 - 2 * 0.1239) / 640, abs=0.002)
    assert side > (712 - 640) // 2

