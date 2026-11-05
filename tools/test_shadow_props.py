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
    /* the setting off (and no debug bits): nothing the game made shadowless casts, whatever it is */
    assert(!shadow_prop_allowed(1, 0, 0, 1.f, 0, 0, 1));
    assert(!shadow_prop_allowed(0, 1, 0, 1.f, 0, 0, 0));
    assert(!shadow_prop_allowed(0, 0, 1, 1.f, 0, 0, 0));
    /* on: devices, and scenery only by name */
    assert(shadow_prop_allowed(0, 1, 0, 1.f, 1, 0, 0));
    assert(shadow_prop_allowed(1, 0, 0, 1.f, 1, 0, 1));
    assert(!shadow_prop_allowed(1, 0, 0, 1.f, 1, 0, 0));
    assert(!shadow_prop_allowed(0, 0, 1, 1.f, 1, 0, 0));
    /* too large (a planet, a ship): none, even a device or a whitelisted name */
    assert(!shadow_prop_allowed(0, 1, 0, 5.5f, 1, 0, 0));
    assert(!shadow_prop_allowed(1, 0, 0, 5.5f, 1, 0, 1));
    assert(shadow_prop_allowed(1, 0, 0, 5.f, 1, 0, 1));
    /* the debug bits work without the setting, one class each, and ignore the size and the names */
    assert(shadow_prop_allowed(1, 0, 0, 50.f, 0, SHADOW_PROP_DEBUG_SCENERY, 0));
    assert(!shadow_prop_allowed(0, 1, 0, 1.f, 0, SHADOW_PROP_DEBUG_SCENERY, 0));
    assert(shadow_prop_allowed(0, 1, 0, 1.f, 0, SHADOW_PROP_DEBUG_DEVICES, 0));
    assert(shadow_prop_allowed(0, 0, 1, 1.f, 0, SHADOW_PROP_DEBUG_ITEMS, 0));
    assert(!shadow_prop_allowed(1, 0, 0, 1.f, 0, SHADOW_PROP_DEBUG_ITEMS, 1));
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


def test_shadow_props_setting_is_off_by_default_and_documented():
    config = (ROOT / 'port/linux/src/port_config.c').read_text()
    assert '{ "display.shadow_props", _config_boolean, "false"' in config
    assert '{ "debug.shadow_scenery", _config_integer, "0"' in config
    readme = (ROOT / 'port/linux/README.md').read_text()
    assert '| `display.shadow_props` | boolean | `false` |' in readme
    assert '| `debug.shadow_scenery` | integer | `0` |' in readme
