"""Catch safe-area values lost between the Apple host and the guest's HUD anchors."""
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

from harness import function

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("apple_guest", [True, False], ids=["apple-guest", "desktop"])
def test_host_safe_area_reaches_hud_anchors(tmp_path, apple_guest):
    sources = [
        ("port/ios/host/host_external.m", "host_external_safe_insets"),
        ("port/linux/src/sdl_platform.c", "platform_video_safe_insets"),
        ("port/linux/src/d3d8_device.c", "halo_screen_hud_frame"),
        ("source/main/main.c", "compute_subframe_counts"),
        ("source/main/main.c", "compute_window_bounds"),
        ("source/interface/hud_draw.c", "hud_calculate_point"),
    ]
    functions = "\n".join(function((ROOT / path).read_text(), name) for path, name in sources)
    program = r'''
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "check failed: %s\n", #x); return 1; } } while (0)
#define SCREEN_HEIGHT 480
#define TRUE 1
#define FALSE 0
#define STACK_BUFFER_LENGTH 4
#define FLAG(bit) (1 << (bit))
#define TEST_FLAG(value, bit) ((value) & FLAG(bit))
#define _hud_anchor_right_bit 0
#define _hud_anchor_bottom_bit 1
#define csmemset memset
#define match_assert(...) ((void)0)
#define match_assert_stack_frame(...) ((void)0)
typedef int boolean;
typedef float real;
typedef struct { short x, y; } point2d;
typedef struct { real x, y; } real_point2d;
typedef union { struct { short y0, x0, y1, x1; }; short v[4]; } rectangle2d;
enum { _hud_anchor_top_left, _hud_anchor_top_right, _hud_anchor_bottom_left,
       _hud_anchor_bottom_right, _hud_anchor_center };
struct hud_absolute_placement_definition { short corner; };
struct hud_placement_definition { point2d offset; };
struct bitmap_data { point2d registration_point; short width, height; };
static struct { short local_player_index; struct { rectangle2d viewport_bounds, window_bounds; } camera; } render;
static struct { struct { rectangle2d screen_bounds, frame_bounds; } reserved04; } rasterizer_globals;
static float safe_insets[4];
static pthread_mutex_t safe_insets_lock = PTHREAD_MUTEX_INITIALIZER;
static int log_count;
#define platform_log(...) (++log_count)
static long get_return_eip(void) { return 0; }
static long fast_ftol(real x) { return (long)x; }
static real hud_globals_get_scale(boolean multiplayer) { (void)multiplayer; return 1; }
static void halo_hud_group_corner(short corner) { (void)corner; }
static boolean hud_split_screen_layout(void) { return TRUE; }
/* FUNCTIONS */
static void layout(void) {
    halo_screen_hud_frame(698, &rasterizer_globals.reserved04.frame_bounds.x0,
        &rasterizer_globals.reserved04.frame_bounds.y0, &rasterizer_globals.reserved04.frame_bounds.x1,
        &rasterizer_globals.reserved04.frame_bounds.y1);
    compute_window_bounds(0, 1, &render.camera.viewport_bounds, &render.camera.window_bounds);
}
static point2d anchor(short corner) {
    struct hud_absolute_placement_definition absolute = {corner};
    struct hud_placement_definition placement = {{14, 10}};
    point2d point;
    hud_calculate_point(0, &absolute, &placement, NULL, FALSE, 0, &point);
    return point;
}
int main(void) {
    rasterizer_globals.reserved04.screen_bounds = (rectangle2d){.x0=0, .y0=0, .x1=698, .y1=480};
    layout();
    rectangle2d original = render.camera.window_bounds;
    const rectangle2d expected = {.x0=52, .y0=36, .x1=646, .y1=444};
    CHECK(memcmp(&original, &expected, sizeof(expected)) == 0);
    point2d before[5];
    for (int corner = 0; corner < 5; corner++) before[corner] = anchor(corner);
    CHECK(before[0].x == 66 && before[0].y == 46);
    CHECK(log_count == 1);
    layout();
    CHECK(log_count == 1);
    safe_insets[0] = 0.12f; safe_insets[3] = 0.07f;
    layout();
#ifndef HALO_IOS
    CHECK(memcmp(&render.camera.window_bounds, &original, sizeof(original)) == 0);
    CHECK(log_count == 1);
    return 0;
#endif
    CHECK(render.camera.window_bounds.x0 == 84);
    CHECK(anchor(_hud_anchor_top_left).x == 98);
    CHECK(anchor(_hud_anchor_top_left).x > 0.12f * 698);
    CHECK(anchor(_hud_anchor_bottom_left).x == 98);
    CHECK(anchor(_hud_anchor_top_right).x == before[1].x);
    CHECK(anchor(_hud_anchor_bottom_right).y == before[3].y);
    CHECK(log_count == 2);
    safe_insets[0] = 0.1239f; safe_insets[3] = 0.0730f;
    layout();
    CHECK(render.camera.window_bounds.x0 == 87);
    CHECK(anchor(_hud_anchor_top_left).x == 101);
    /* A rotated cutout can exceed the title-safe margin on any edge. */
    safe_insets[0] = 0; safe_insets[1] = 0.13f;
    safe_insets[2] = 0.10f; safe_insets[3] = 0.11f;
    layout();
    CHECK(anchor(_hud_anchor_top_right).x == 593);
    CHECK(anchor(_hud_anchor_top_left).y == 58);
    CHECK(anchor(_hud_anchor_bottom_left).y == 417);
    /* Moving to an external display restores the original bytes. */
    memset(safe_insets, 0, sizeof(safe_insets));
    layout();
    CHECK(memcmp(&render.camera.window_bounds, &original, sizeof(original)) == 0);
    for (int corner = 0; corner < 5; corner++) {
        point2d point = anchor(corner);
        CHECK(memcmp(&point, &before[corner], sizeof(point)) == 0);
    }
    return 0;
}
'''
    source = tmp_path / "probe.c"
    binary = tmp_path / "probe"
    source.write_text(program.replace("/* FUNCTIONS */", functions))
    defines = ["-DHALO_IOS=1"] if apple_guest else []
    subprocess.run([shutil.which("clang") or "cc", "-std=gnu11", *defines, str(source),
                    "-lm", "-pthread", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_safe_insets_host_import_resolves(tmp_path):
    """Catch a missing import even when both platform functions compile."""
    assembly = tmp_path / "imports.s"
    table = tmp_path / "imports.c"
    subprocess.run([sys.executable, str(ROOT / "tools/guest_imports.py"), "--host-table",
                    str(table), str(assembly), str(ROOT / "port/runtime/host_imports.list")], check=True)
    names = [line.split("#", 1)[0].strip()
             for line in (ROOT / "port/runtime/host_imports.list").read_text().splitlines()]
    stubs = []
    for name in dict.fromkeys(names):
        if name and name != "host_external_safe_insets":
            symbol = "posix_" + name[len("hostposix_"):] if name.startswith("hostposix_") else name
            stubs.append(f"void {symbol}(void) {{}}")
    getter = function((ROOT / "port/ios/host/host_external.m").read_text(), "host_external_safe_insets")
    source = tmp_path / "probe.c"
    source.write_text(r'''
#include <pthread.h>
#include <string.h>
static float safe_insets[4] = {0.12f, 0, 0, 0.07f};
static pthread_mutex_t safe_insets_lock = PTHREAD_MUTEX_INITIALIZER;
void *host_resolve_import(const char *name);
''' + getter + "\n" + "\n".join(stubs) + r'''
int main(void) {
    void (*read_insets)(float *) = host_resolve_import("host_external_safe_insets");
    float actual[4] = {0};
    if (!read_insets) return 1;
    read_insets(actual);
    return memcmp(actual, safe_insets, sizeof(actual)) != 0;
}
''')
    binary = tmp_path / "probe"
    subprocess.run([shutil.which("clang") or "cc", str(source), str(table),
                    "-pthread", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
