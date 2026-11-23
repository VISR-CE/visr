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
    for name in ('FIRST_ROW', 'ROW_HEIGHT', 'COMFORTABLE_PITCH', 'LEGEND_Y', 'DROPDOWN_PITCH'):
        assert getattr(xbox_menus, name) == getattr(menu_geometry, name), name


def test_font_metrics_table_is_checked_in_and_in_its_written_form():
    import json
    import menu_text_metrics as metrics

    assert metrics.available(), 'tools/menu_font_metrics.json is missing'
    table = json.loads(metrics.TABLE.read_text(encoding='utf-8'))
    assert list(table) == list(metrics.FONTS)
    # the file is exactly what --update writes, so --check against a map compares like with like
    assert metrics.TABLE.read_text(encoding='utf-8') == metrics.dump(table)
    for font in table.values():
        assert all(len(widths) == 2 and all(isinstance(value, int) for value in widths)
                   for widths in font['characters'].values())
    # numbers only: line metrics and per-character widths
    assert set(next(iter(table.values()))) == {'ascending_height', 'descending_height', 'leading_height',
                                               'leading_width', 'characters'}
