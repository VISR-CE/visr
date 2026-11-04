"""The iOS and Mac guest holds the mouse while the game is played and lets it go in the menus.

The guest's `halo_ui_pointer_update` (the ILP32 build, d3d8_device.c) asks the host to hold the mouse
(`platform_mouse_menus_changed`) when no menu is up, and to release it when one is, once per change.
"""
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from harness import function, mutated  # noqa: E402

SOURCE = (ROOT / "port/linux/src/d3d8_device.c").read_text()
PRELUDE = r'''
#include <stdio.h>
typedef int BOOL;
#define CONFIG_SCENE_MENU 1
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "check failed: %s\n", #x); return 1; } } while (0)
struct halo_ui_pointer { int unused; };
static int scene = 0, captures, held = -1;
static int halo_scene_class(void) { return scene; }
static void host_sdl_set_hint(const char *name, const char *value) { (void)name; (void)value; }
static void platform_menus_set_active(BOOL active) { (void)active; }
static void platform_mouse_menus_changed(BOOL up) { captures++; held = !up; }
'''
MAIN = r'''
int main(void) {
    struct halo_ui_pointer pointer;
    halo_ui_pointer_update(0, &pointer);
    CHECK(held == 1 && captures == 1);
    halo_ui_pointer_update(0, &pointer);
    CHECK(captures == 1);
    halo_ui_pointer_update(1, &pointer);
    CHECK(held == 0 && captures == 2);
    halo_ui_pointer_update(1, &pointer);
    CHECK(captures == 2);
    halo_ui_pointer_update(0, &pointer);
    CHECK(held == 1 && captures == 3);
    return 0;
}
'''


def run_probe(tmp_path, *, fault=False):
    compiler = shutil.which("clang") or shutil.which("cc")
    assert compiler, "A C compiler is required for the mouse capture probe"
    tmp_path.mkdir(parents=True, exist_ok=True)
    stub = function(SOURCE, "halo_ui_pointer_update")
    assert "platform_mouse_menus_changed(pointer_free)" in stub
    if fault:
        stub = mutated(stub, "platform_mouse_menus_changed(pointer_free)", "(void)pointer_free")
    source = tmp_path / "probe.c"
    source.write_text(PRELUDE + "\n" + stub + "\n" + MAIN)
    binary = tmp_path / "probe"
    compiled = subprocess.run([compiler, "-std=c99", "-Wall", "-Werror", "-Wno-unused-function",
                               str(source), "-o", str(binary)], capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stderr
    return subprocess.run([str(binary)], capture_output=True, text=True)


def test_mouse_is_held_in_play_and_released_in_menus(tmp_path):
    observed = run_probe(tmp_path)
    assert observed.returncode == 0, observed.stderr


def test_negative_control_detects_a_missing_capture_request(tmp_path):
    assert run_probe(tmp_path / "fault", fault=True).returncode != 0
