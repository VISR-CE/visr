"""The pre-rendered chart SVGs must have been rendered from the current chart source (a hash stamped into each SVG)."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent / 'perf_compare'))
import charts  # noqa: E402
import refresh  # noqa: E402
import site_page  # noqa: E402

SOURCE = 'xychart-beta horizontal\n    bar [1, 2]\n    bar [3, 4]\n'


def stamped(source):
    return f'<svg {charts.STATIC_STAMP}="{charts.static_source_stamp(source)}"></svg>'


def test_svg_rendered_from_the_source_has_no_problems():
    assert charts.static_svg_problems(SOURCE, stamped(SOURCE)) == []


@pytest.mark.parametrize('changed', [
    'xychart-beta horizontal\n    bar [2, 1]\n    bar [3, 4]\n',      # two builds swapped
    'xychart-beta horizontal\n    bar [3, 4]\n    bar [1, 2]\n',      # the two series swapped
    'xychart-beta horizontal\n    bar [0, 2]\n    bar [3, 4]\n',      # a value dropped to zero
    'xychart-beta horizontal\n    y-axis "x" 0 --> 9\n    bar [1, 2]\n    bar [3, 4]\n',  # a changed axis
])
def test_any_change_of_the_source_is_reported(changed):
    assert charts.static_svg_problems(changed, stamped(SOURCE))


def test_missing_unstamped_and_orphan_svgs_are_reported():
    assert charts.static_svg_problems(SOURCE, None)
    assert charts.static_svg_problems(SOURCE, '<svg/>')
    assert charts.static_svg_problems(None, '<svg/>')
    assert charts.static_svg_problems(None, None) == []


def test_render_check_status_covers_every_checked_in_chart_and_orphan(tmp_path, monkeypatch, capsys):
    rows = charts.load_snapshot(charts.SNAPSHOT)
    specs = charts.load_specs(charts.SPECS, rows)
    assert refresh.static_chart_status(rows, specs, charts, site_page) == 0
    monkeypatch.setattr(refresh, 'ROOT', tmp_path)
    (tmp_path / 'docs/site/benchmarks').mkdir(parents=True)
    (tmp_path / 'docs/site/benchmarks/static-orphan.svg').write_text('<svg/>')
    assert refresh.static_chart_status(rows, specs[:0], charts, site_page) == 1
    assert 'orphan static chart: static-orphan.svg' in capsys.readouterr().err
