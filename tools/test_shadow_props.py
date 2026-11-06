"""Exercise display.shadow_props' whitelist and debug.shadow_scenery in the real C header (source/render/shadow_props.h)."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

PROGRAM = r'''
#include <assert.h>
#include "source/render/shadow_props.h"
int main(void) {
    float d[6] = {9.f, 1.f, 4.f, 1.f, 16.f, 0.25f};
    unsigned char keep[6];
    /* the setting off (and no debug bits): nothing the game made shadowless casts, whatever it is */
    assert(!shadow_prop_allowed(1, 0, 0, 0, 0, 1, 1));
    assert(!shadow_prop_allowed(0, 1, 0, 0, 0, 1, 1));
    assert(!shadow_prop_allowed(0, 0, 1, 0, 0, 1, 1));
    /* on: only what is eligible and among the view's nearest */
    assert(shadow_prop_allowed(0, 1, 0, 0, 1, 1, 1));
    assert(!shadow_prop_allowed(0, 1, 0, 0, 1, 1, 0));
    assert(!shadow_prop_allowed(0, 1, 0, 0, 1, 0, 1));
    /* eligible: a device, or scenery by name, at or under the radius cap (3 units) */
    assert(shadow_prop_eligible(0, 1, 1.f, 0));
    assert(shadow_prop_eligible(1, 0, 1.f, 1));
    assert(!shadow_prop_eligible(1, 0, 1.f, 0));
    assert(!shadow_prop_eligible(0, 0, 1.f, 1));
    assert(shadow_prop_eligible(0, 1, SHADOW_PROP_MAXIMUM_RADIUS, 0));
    assert(!shadow_prop_eligible(0, 1, SHADOW_PROP_MAXIMUM_RADIUS + 0.01f, 0));
    assert(!shadow_prop_eligible(1, 0, 50.f, 1));
    /* the debug bits work without the setting, one class each, and ignore the size, the names and the limit */
    assert(shadow_prop_allowed(1, 0, 0, SHADOW_PROP_DEBUG_SCENERY, 0, 0, 0));
    assert(!shadow_prop_allowed(0, 1, 0, SHADOW_PROP_DEBUG_SCENERY, 0, 1, 1));
    assert(shadow_prop_allowed(0, 1, 0, SHADOW_PROP_DEBUG_DEVICES, 0, 0, 0));
    assert(shadow_prop_allowed(0, 0, 1, SHADOW_PROP_DEBUG_ITEMS, 0, 0, 0));
    /* the nearest four of six, a tie keeping the earlier; fewer candidates than the limit keep them all */
    assert(shadow_prop_select_nearest(d, 6, 4, keep) == 4);
    assert(!keep[0] && keep[1] && keep[2] && keep[3] && !keep[4] && keep[5]);
    assert(shadow_prop_select_nearest(d, 6, 1, keep) == 1 && keep[5] && !keep[1] && !keep[3]);
    assert(shadow_prop_select_nearest(d, 2, 4, keep) == 2 && keep[0] && keep[1]);
    assert(shadow_prop_select_nearest(d, 0, 4, keep) == 0);
    assert(SHADOW_PROP_MAXIMUM_PER_VIEW == 4);
    /* the names: the Covenant props of b30 and c10, and not the vegetation, rocks or the game's weapons */
    assert(shadow_prop_scenery_name_matches("scenery\\c_storage\\c_storage"));
    assert(shadow_prop_scenery_name_matches("scenery\\c_storage_large\\c_storage_large"));
    assert(shadow_prop_scenery_name_matches("scenery\\c_field_generator\\c_field_generator"));
    assert(shadow_prop_scenery_name_matches("scenery\\c_uplink\\c_uplink"));
    assert(shadow_prop_scenery_name_matches("levels\\b40\\scenery\\b40_metal_small\\metal_small"));
    assert(!shadow_prop_scenery_name_matches("scenery\\trees\\tree_leafy\\tree_leafy"));
    assert(!shadow_prop_scenery_name_matches("scenery\\rocks\\boulder\\boulder"));
    assert(!shadow_prop_scenery_name_matches("scenery\\vehicles\\c_dropship_crashed\\c_dropship_crashed"));
    assert(!shadow_prop_scenery_name_matches("scenery\\emitters\\steam jets\\steam jets"));
    assert(!shadow_prop_scenery_name_matches(0));
    return 0;
}
'''


def build_and_run(tmp_path, flags):
    source = tmp_path / 'shadow_props.c'
    binary = tmp_path / ('shadow_props.exe' if os.name == 'nt' else 'shadow_props')
    source.write_text(PROGRAM)
    subprocess.run([shutil.which('clang') or 'cc', *flags, '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                    str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_shadow_props_as_the_game_compiles_it(tmp_path):
    build_and_run(tmp_path, ['-std=gnu89'])


def test_header_is_c89(tmp_path):
    build_and_run(tmp_path, ['-std=c89', '-pedantic-errors', '-D__STRICT_ANSI__'])


def test_shadow_props_under_the_sanitizers(tmp_path):
    build_and_run(tmp_path, ['-std=c99', '-fsanitize=address,undefined'])


def test_shadow_props_setting_is_on_by_default_behind_the_graphics_mode_and_documented():
    config = (ROOT / 'port/linux/src/port_config.c').read_text()
    assert '{ "display.shadow_props", _config_boolean, "true"' in config
    # Enhanced graphics only: Classic and Hardcore turn it off
    assert 'X("display.shadow_props", "false", "false")' in (ROOT / 'port/linux/src/port_graphics_modes.h').read_text()
    assert '{ "debug.shadow_scenery", _config_integer, "0"' in config
    readme = (ROOT / 'port/linux/README.md').read_text()
    assert '| `display.shadow_props` | boolean | `true` |' in readme
    assert '| `debug.shadow_scenery` | integer | `0` |' in readme
