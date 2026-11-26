"""debug.menu_backdrop_freeze (sdl_platform.c halo_menu_backdrop_seconds): the main menu's backdrop world advances no
time while the setting holds it, a level's time is never held, and the setting off plays the backdrop."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run  # noqa: E402

CASES = ["held-in-menu", "level-plays", "plays-by-default"]

# a fault in the function and the case that must catch it
NEGATIVE_CONTROLS = {
    "never-held": (("menu_scenario_loaded && config_boolean(\"debug.menu_backdrop_freeze\") ? 0.0f : seconds",
                    "seconds"), "held-in-menu"),
    "levels-held-too": (("menu_scenario_loaded && config_boolean(\"debug.menu_backdrop_freeze\")",
                         "config_boolean(\"debug.menu_backdrop_freeze\")"), "level-plays"),
    "held-without-the-setting": (("menu_scenario_loaded && config_boolean(\"debug.menu_backdrop_freeze\")",
                                  "menu_scenario_loaded"), "plays-by-default"),
}


def generated(fault=None):
    code = function(read("port/linux/src/sdl_platform.c"), "halo_menu_backdrop_seconds")
    if fault:
        code = mutated(code, *fault)
    return (("under_test.inc", code + "\n"),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("menu_backdrop_freeze", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("menu_backdrop_freeze", generated(fault)), case)
    assert status == CHECK_FAILED, f"sdl_platform.c with a fault ({control}) passed '{case}': the test cannot see it"
