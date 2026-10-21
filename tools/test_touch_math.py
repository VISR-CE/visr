"""Portable probes for the touch overlay's geometry and stick response."""
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def probe(tmp_path_factory):
    compiler = shutil.which("cc") or shutil.which("clang")
    if not compiler:
        pytest.skip("A C compiler is required")
    folder = tmp_path_factory.mktemp("touch-math")
    source = folder / "probe.c"
    source.write_text(r'''
#include "touch_math.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    const char *name = argv[1];
    if (!strcmp(name, "dead_zone")) {
        touch_point p = touch_stick(0, 0, 50, .1, 1.5);
        assert(p.x == 0 && p.y == 0);
        p = touch_stick(3, 4, 50, .1, 1.5);
        assert(p.x == 0 && p.y == 0);
        p = touch_stick(5.001, 0, 50, .1, 1.5);
        assert(p.x > 0 && p.x < .001);
    } else if (!strcmp(name, "curve")) {
        double previous = 0;
        for (int i = 6; i <= 45; ++i) {
            touch_point p = touch_stick(i, 0, 50, .1, 1.5);
            assert(p.x > previous && p.y == 0);
            previous = p.x;
        }
        touch_point linear = touch_stick(25, 0, 50, .1, 1);
        touch_point curved = touch_stick(25, 0, 50, .1, 1.5);
        assert(fabs(linear.x - .5) < 1e-9);
        assert(fabs(curved.x - pow(.5, 1.5)) < 1e-9);
    } else if (!strcmp(name, "saturation")) {
        for (int i = 45; i <= 100; ++i) {
            touch_point p = touch_stick(-.6*i, .8*i, 50, .1, 1.5);
            assert(fabs(p.x + .6) < 1e-9 && fabs(p.y - .8) < 1e-9);
        }
    } else if (!strcmp(name, "hit")) {
        assert(touch_hit_test((touch_point){0, 0}, NULL, 0) == -1);
        touch_target targets[] = {{{10, 10, 20, 20}, 7}, {{40, 10, 20, 20}, 7}};
        assert(touch_hit_test((touch_point){33, 20}, targets, 2) == 0);
        assert(touch_hit_test((touch_point){37, 20}, targets, 2) == 1);
        assert(touch_hit_test((touch_point){35, 20}, targets, 2) == 0);
        assert(touch_hit_test((touch_point){3, 20}, targets, 2) == 0);
        assert(touch_hit_test((touch_point){2, 20}, targets, 2) == -1);
        touch_target stick = {{100, 100, 136, 136}, 18};
        assert(touch_hit_test((touch_point){82, 168}, &stick, 1) == 0);
        assert(touch_hit_test((touch_point){81, 168}, &stick, 1) == -1);
        touch_target mixed[] = {{{0, 0, 136, 136}, 18}, {{140, 50, 44, 44}, 7}};
        assert(touch_hit_test((touch_point){145, 72}, mixed, 2) == 1);
    } else if (!strcmp(name, "scale")) {
        assert(touch_scale(100, 100) == .7);
        assert(touch_scale(2000, 1000) == 1.4);
        assert(touch_scale(760, 360) == 1);
        assert(touch_scale(1520, 360) == 1);
    } else if (!strcmp(name, "floating")) {
        touch_rect zone = touch_move_zone((touch_rect){0, 0, 1000, 500},
                                          (touch_rect){20, 10, 950, 470});
        assert(zone.x == 20 && zone.y == 140 && zone.width == 400 && zone.height == 340);
        touch_point p = touch_floating_base((touch_point){0, 900}, zone, 50);
        assert(p.x == 70 && p.y == 430);
        p = touch_floating_base((touch_point){300, 250}, zone, 50);
        assert(p.x == 300 && p.y == 250);
        p = touch_floating_base((touch_point){100, 100}, (touch_rect){10, 20, 30, 40}, 50);
        assert(p.x == 25 && p.y == 40);
    } else return 1;
    return 0;
}
''')
    binary = folder / "probe"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "port/ios/host"), str(source),
                    str(ROOT / "port/ios/host/touch_math.c"), "-lm", "-o", str(binary)], check=True)
    return binary


@pytest.mark.parametrize("case", ["dead_zone", "curve", "saturation", "hit", "scale", "floating"])
def test_touch_math(probe, case):
    subprocess.run([str(probe), case], check=True, capture_output=True, text=True)
