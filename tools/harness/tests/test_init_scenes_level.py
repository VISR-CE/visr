"""init.txt lines after a map_name line wait for the level to load when debug.init_after is set, so script threads
they start (sleep_until and the like) are not killed by the load."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CONSOLE = (ROOT / "source/main/console.c").read_text()


def test_lines_after_map_name_are_deferred():
    body = CONSOLE[CONSOLE.index("static void console_startup_run_lines(\n"):CONSOLE.index("void console_dispose")]
    assert 'strncmp(buffer, "map_name "' in body
    assert "console_startup_rest = file" in body
    assert "debug.init_after" in body


def test_deferred_lines_run_once_the_game_is_in_progress():
    body = CONSOLE[CONSOLE.index("void console_startup_update("):CONSOLE.index("static void console_startup_run(\n")]
    assert "console_startup_rest && game_in_progress()" in body
    assert body.index("console_startup_rest = NULL") < body.index("console_startup_run_lines(rest)")
