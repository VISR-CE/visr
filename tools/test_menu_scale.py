"""The production C window preserves padded content at the largest fitting scale."""
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def probe(tmp_path_factory):
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("A C compiler is required for the menu scale probe")
    binary = tmp_path_factory.mktemp("menu_scale") / "probe"
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "port/linux/src"), "-idirafter", str(ROOT / "port/linux/include"),
                    str(ROOT / "port/ios/tests/menu_scale_probe.c"),
                    str(ROOT / "port/linux/src/menu_scale.c"), "-o", str(binary)], check=True)
    return binary


@pytest.mark.parametrize("height", [375, 402, 440, 834, 1080, 720, 900, 414, 480])
def test_menu_window(probe, height):
    top, bottom, scale = map(float, subprocess.check_output([str(probe), str(height)], text=True).split())
    padded_top, padded_bottom = 41 - 18, 443 + 4
    span = 480 if height >= 480 else max(height / 1.0, padded_bottom - padded_top)
    center = (padded_top + padded_bottom) / 2
    expected_top = min(max(center - span / 2, 0), 480 - span)
    assert top == pytest.approx(expected_top)
    assert bottom == pytest.approx(expected_top + span)
    assert scale == pytest.approx(height / span)
    assert 0 <= top <= padded_top
    assert padded_bottom <= bottom <= 480
    if height >= 480:
        assert (top, bottom) == (0, 480)
    elif height == 440:
        assert 480 - (bottom - top) == pytest.approx(40)
        assert scale == 1
    elif height in (375, 402):
        assert scale == pytest.approx(height / 424)
        assert scale < 1


@pytest.mark.parametrize("apple", [False, True])
def test_widget_viewport_and_pointer_round_trip(tmp_path, apple):
    """Execute the actual drawing coefficients and input inverse together."""
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("A C compiler is required for the menu transform probe")
    source = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    helpers = source[source.index("static int menu_window_drawing;"):source.index("/* render_window calls this once")]
    start = source.index("converted->viewport_scale[0][0] /= menu_zoom;")
    end = source.index("\n", source.index("converted->viewport_offset[0][1] = menu_top", start))
    coefficients = source[start:end]
    harness = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "menu_scale.h"
#include "halo_ui_pointer.h"
#define CONFIG_SCENE_MENU 1
static int enabled = 1, scene = 1, height = 402, width = 1044;
int config_boolean(const char *name) { (void)name; return enabled; }
int halo_scene_class(void) { return scene; }
long halo_screen_width(void) { return width; }
float halo_screen_pixel_scale(void) { return 3; }
void platform_video_window_size(int *w, int *h) { *w = width; *h = height; }
int host_sdl_menu_height_points(void) { return height; }
__HELPERS__
struct viewport { float viewport_scale[1][4], viewport_offset[1][4]; };
static void check(float x, float y) {
    float menu_top, menu_zoom;
    menu_window_geometry(&menu_top, &menu_zoom);
    struct { float viewport_offset[4]; } device = { { width * 0.5f, 240 } };
    struct viewport uniforms = { { { width * 0.5f, -240 } }, { { width * 0.5f, 240 } } };
    struct viewport *converted = &uniforms;
    __COEFFICIENTS__
    float screen_x = x + (width - 640) / 2;
    float ndc_x = (screen_x - converted->viewport_offset[0][0]) / converted->viewport_scale[0][0];
    float ndc_y = (y - converted->viewport_offset[0][1]) / converted->viewport_scale[0][1];
    float drawn_x = (ndc_x + 1) * width / 2;
    float drawn_y = (1 - ndc_y) * 240;
    struct halo_ui_pointer p = {0};
    p.x = p.click_x = (short)floorf(drawn_x - (width - 640) / 2);
    p.y = p.click_y = (short)floorf(drawn_y);
    p.radius = 12;
    menu_window_pointer(&p);
    assert(fabsf(p.x - x) <= 1 && fabsf(p.y - y) <= 1);
    assert(p.click_x == p.x && p.click_y == p.y);
    assert(p.radius == (short)ceilf(12 / menu_zoom));
}
int main(void) {
    const int heights[] = {375, 402, 440, 834, 1080, 720, 900};
    for (unsigned i = 0; i < sizeof(heights) / sizeof(*heights); i++) {
        height = heights[i];
        check(0, 33); check(320, 240); check(640, 447);
        float top, zoom;
        menu_window_geometry(&top, &zoom);
        halo_screen_menu_window(1);
        assert(menu_window_drawing && fabsf(halo_screen_menu_pixel_scale() - 3 * zoom) < 0.001f);
        halo_screen_menu_window(0);
        assert(!menu_window_drawing);
    }
    /* Pause content has its own bounds even though the world scene is live. */
    scene = 0;
    halo_screen_menu_content(120, 370);
    for (unsigned i = 0; i < sizeof(heights) / sizeof(*heights); i++) {
        height = heights[i];
        check(0, 120); check(320, 240); check(640, 370);
        float top, zoom;
        menu_window_geometry(&top, &zoom);
        assert(zoom >= 1);
    }
    /* A width cap must still contain an off-center pause layout. */
    height = 375; width = 700;
    halo_screen_menu_content(300, 460);
    float limited_top, limited_zoom;
    menu_window_geometry(&limited_top, &limited_zoom);
    assert(limited_top <= 292 && limited_top + 480 / limited_zoom >= 464);
    check(320, 460);
    width = 1044;
    halo_screen_menu_content(480, 0);
    height = 375;
    for (int mode = 0; mode < 3; mode++) {
        enabled = mode != 0;
        scene = mode == 1 ? 0 : CONFIG_SCENE_MENU;
        width = mode == 2 ? 640 : 1044;
        float top, zoom;
        menu_window_geometry(&top, &zoom);
        assert(top == 0 && zoom == 1);
        check(320, 240);
    }
#ifdef HALO_ILP32
    enabled = 1; scene = 0; width = 1044;
    halo_screen_menu_content(120, 370);
    setenv("HALO_HOST_VISIONOS", "1", 1);
    float top, zoom;
    menu_window_geometry(&top, &zoom);
    assert(top == 0 && zoom == 1);
#endif
    return 0;
}
'''.replace("__HELPERS__", helpers).replace("__COEFFICIENTS__", coefficients)
    path, binary = tmp_path / "transform.c", tmp_path / "transform"
    path.write_text(harness)
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    *(["-DHALO_ILP32"] if apple else []),
                    "-I", str(ROOT / "port/linux/src"), "-I", str(ROOT / "port/linux/include"),
                    str(path), str(ROOT / "port/linux/src/menu_scale.c"), "-lm", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_host_uses_window_points(tmp_path):
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("A C compiler is required for the host point-height probe")
    source = (ROOT / "port/ios/host/host_sdl.c").read_text()
    start = source.index("int host_sdl_menu_height_points(void)")
    helper = source[start:source.index("\n}\n", start) + 3]
    harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct { int present; } SDL_Window;
static SDL_Window object;
static int present = 1, window_height = 402, external_height, pinned_height;
SDL_Window *host_sdl_window(void) { return present ? &object : NULL; }
int host_external_size(int *w, int *h) { *w = 1920; *h = external_height; return external_height != 0; }
int SDL_GetWindowSize(SDL_Window *window, int *w, int *h) {
    (void)window; *w = 874; *h = window_height; return 1;
}
__HELPER__
int main(void) {
    assert(host_sdl_menu_height_points() == 402);
    window_height = 720;
    assert(host_sdl_menu_height_points() == 720);
    external_height = 1080;
    assert(host_sdl_menu_height_points() == 1080);
    external_height = 0;
    pinned_height = 2048;
    setenv("HALO_HOST_DISPLAY", "1366x1024@2", 1);
    assert(host_sdl_menu_height_points() == 1024);
    present = 0;
    assert(host_sdl_menu_height_points() == 0);
    return 0;
}
'''.replace("__HELPER__", helper)
    path, binary = tmp_path / "host.c", tmp_path / "host"
    path.write_text(harness)
    subprocess.run([compiler, "-Wall", "-Wextra", "-Werror", str(path), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


@pytest.mark.parametrize('content_top,content_bottom', [(120, 370), (85, 405), (41, 443), (4, 478)])
@pytest.mark.parametrize('height', [375, 402, 440, 834, 1080, 720, 900])
def test_pause_window_table(probe, content_top, content_bottom, height):
    """Pause layouts use measured title-to-hint bounds with the same padding."""
    top, bottom, scale = map(float, subprocess.check_output(
        [str(probe), str(height), str(content_top), str(content_bottom)], text=True).split())
    padded_top, padded_bottom = max(0, content_top - 18), min(480, content_bottom + 4)
    span = min(480, max(height, padded_bottom - padded_top))
    expected_top = min(max((padded_top + padded_bottom - span) / 2, 0), 480 - span)
    assert top == pytest.approx(expected_top)
    assert bottom == pytest.approx(top + span)
    assert scale == pytest.approx(height / span)
    assert top <= padded_top <= padded_bottom <= bottom


def test_pause_content_comes_from_the_active_widgets(tmp_path):
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required for the pause bounds probe')
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    start = source.index('static void ui_widget_content_bounds(')
    helper = source[start:source.index('static void ui_widget_menu_content(', start)]
    harness = r'''
#include <assert.h>
#include <string.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define NONE -1
#define WINDOW_MAXIMUM_ROWS 16
#define _ui_widget_type_column_list 1
typedef int boolean;
struct ref { int index; };
struct ui_widget_definition {
    struct ref background_bitmap, text_font;
    struct { short y0, y1; } bounds;
};
struct widget_instance {
    int visible, definition_tag_index, vertical_offset, type;
    struct widget_instance *child, *next;
    struct { struct { struct widget_instance *extended_description; } list; } parameters;
};
static struct ui_widget_definition definitions[] = {
    {{0}, {NONE}, {0,480}}, {{1}, {NONE}, {0,256}},
    {{NONE}, {0}, {0,30}}, {{NONE}, {0}, {0,26}},
    {{NONE}, {0}, {0,30}}
};
static struct ui_widget_definition *ui_widget_definition_get(int i) { return &definitions[i]; }
static const char *tag_get_name(int i) { return i == 1 ? "pause/pausebox_center" : "dim"; }
static int pc_menu_list_window(int i) { (void)i; return 0; }
static short column_list_window_begin(struct widget_instance *w, struct ui_widget_definition *d,
    short n, boolean *saved) { (void)w; (void)d; (void)n; (void)saved; return 0; }
static void column_list_window_end(struct widget_instance *w, short n, boolean *s) {
    (void)w; (void)n; (void)s;
}
__HELPER__
int main(void) {
    struct widget_instance hidden = {0,4,470,0,0,0,{{0}}};
    struct widget_instance hints = {1,3,350,0,0,&hidden,{{0}}};
    struct widget_instance title = {1,2,100,0,0,&hints,{{0}}};
    struct widget_instance box = {1,1,80,0,0,&title,{{0}}};
    struct widget_instance root = {1,0,20,0,&box,0,{{0}}};
    long top = 480, bottom = 0;
    ui_widget_content_bounds(&root, 0, &top, &bottom);
    assert(top == 120 && bottom == 396);
    hints.vertical_offset = 300;
    top = 480; bottom = 0;
    ui_widget_content_bounds(&root, 0, &top, &bottom);
    assert(top == 120 && bottom == 346);
    struct widget_instance description = {1,4,410,0,0,0,{{0}}};
    root.type = _ui_widget_type_column_list;
    root.parameters.list.extended_description = &description;
    top = 480; bottom = 0;
    ui_widget_content_bounds(&root, 0, &top, &bottom);
    assert(top == 120 && bottom == 460);
    return 0;
}
'''.replace('__HELPER__', helper)
    path, binary = tmp_path / 'bounds.c', tmp_path / 'bounds'
    path.write_text(harness)
    subprocess.run([compiler, '-Wall', '-Wextra', '-Werror', str(path), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
