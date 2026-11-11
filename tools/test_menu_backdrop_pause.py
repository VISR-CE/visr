"""Main menu child screens must not pause game time (the 3D backdrop runs on it)."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_main_menu_widgets_do_not_pause_game_time():
    source = (ROOT / "source/interface/ui_widget.c").read_text()
    start = source.index("widget->pause_game_time = TEST_FLAG(definition->flags, _widget_pause_game_time_bit)")
    statement = source[start:source.index(";", start)]
    assert "!we_are_at_the_main_menu" in statement
