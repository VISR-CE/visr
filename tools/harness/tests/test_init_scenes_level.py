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


def test_deferred_lines_wait_for_a_level_not_the_main_menu():
    """The main menu is a game in progress too: with debug.init_after, map_name can run over it, and the lines after it must
    wait for the level, not run in the menu's game (where the level's AI, flags and scripts are not valid names)."""
    body = CONSOLE[CONSOLE.index("void console_startup_update("):CONSOLE.index("static void console_startup_run(\n")]
    condition = body[body.index("else if (console_startup_rest"):body.index("FILE *rest")]
    assert "game_in_progress()" in condition
    assert "global_scenario->type != _scenario_type_main_menu" in condition


def test_overlong_lines_are_skipped_whole_not_run_in_pieces():
    """fgets cuts a line longer than the buffer; every piece used to run as its own command. The class: any line over the
    buffer. It is skipped whole (the rest of it read and dropped) and logged, before anything compiles it."""
    body = CONSOLE[CONSOLE.index("static void console_startup_run_lines(\n"):CONSOLE.index("void console_dispose")]
    check = body.index("if (!strchr(buffer, '\\n') && !feof(file))")
    assert check < body.index("hs_compile_and_evaluate(buffer)")
    skip = body[check:body.index("strtok(buffer")]
    assert "fgetc(file)" in skip and "continue;" in skip and "was skipped" in skip
