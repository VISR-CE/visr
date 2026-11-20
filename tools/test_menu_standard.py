"""The menus' standard elements: one geometry source, tall option rows, wrapped text, buttons and touch strings."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

import menu_geometry  # noqa: E402


def test_geometry_header_is_generated_from_the_geometry_module():
    assert menu_geometry.HEADER.read_text(encoding='utf-8') == menu_geometry.header()


def test_generator_and_engine_take_their_shared_numbers_from_the_geometry_module():
    import xbox_menus
    for name in ('FIRST_ROW', 'ROW_HEIGHT', 'COMFORTABLE_PITCH', 'COMPACT_PITCH', 'LEGEND_Y', 'DROPDOWN_PITCH'):
        assert getattr(xbox_menus, name) == getattr(menu_geometry, name), name
