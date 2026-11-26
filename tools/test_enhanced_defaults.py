"""Enhanced graphics ships its whole feature list on by default.

Bug class: an Enhanced-only feature (display.ambient_occlusion) sat default-off, so the mode that is
supposed to draw it never did. Enhanced is the mode that leaves a setting alone (the graphics-mode table
has no Enhanced value), so the setting's shipped default is what Enhanced draws. Every feature Enhanced
adds must default on, and Classic and Hardcore must still force the additions off.
"""
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import settings_schema as schema

ROOT = Path(__file__).resolve().parents[1]

# The Enhanced feature list (SSAO, prop shadows, per-pixel object lighting, soft particles).
ENHANCED_FEATURES = (
    'display.ambient_occlusion',
    'display.shadow_props',
    'display.per_pixel_lighting',
    'display.soft_particles',
)


def mode_rows():
    text = (ROOT / 'port/linux/src/port_graphics_modes.h').read_text()
    rows = {}
    for name, hardcore, classic in re.findall(
            r'X\("([\w.]+)",\s*("[^"]*"|NULL),\s*("[^"]*"|NULL)\)', text):
        rows[name] = (hardcore, classic)
    return rows


def test_enhanced_features_default_on():
    settings = schema.config_settings()
    off = [name for name in ENHANCED_FEATURES if settings[name]['default'] is not True]
    assert not off, f'Enhanced features that ship default-off: {off}'


def test_enhanced_leaves_its_features_alone():
    rows = mode_rows()
    for name in ENHANCED_FEATURES:
        # the mode table has no Enhanced column: a feature is on in Enhanced exactly when its default is
        assert name in rows, f'{name} is missing from the graphics-mode table'


def test_classic_and_hardcore_force_the_additions_off():
    rows = mode_rows()
    for name in ('display.ambient_occlusion', 'display.per_pixel_lighting', 'display.shadow_props'):
        assert rows[name] == ('"false"', '"false"'), (name, rows[name])
    assert rows['display.soft_particles'][0] == '"false"'
