"""init.txt lines after a map_name line wait for the level to load when debug.init_after is set, so script threads
they start (sleep_until and the like) are not killed by the load."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import function  # noqa: E402

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
    wait for the level, not run in the menu's game (where the level's AI, flags and scripts are not valid names), nor in a
    level a pending map change is about to replace."""
    body = CONSOLE[CONSOLE.index("void console_startup_update("):CONSOLE.index("static void console_startup_run(\n")]
    condition = body[body.index("else if (console_startup_rest"):body.index("FILE *rest")]
    assert "game_in_progress()" in condition
    assert "global_scenario_is_level()" in condition
    assert "!main_port_map_change_pending()" in condition
    scenario = (ROOT / "source/scenario/scenario.c").read_text()
    predicate = function(scenario, "global_scenario_is_level")
    assert "global_scenario &&" in predicate and "!= _scenario_type_main_menu" in predicate


def test_every_line_goes_through_the_reader():
    body = function(CONSOLE, "console_startup_run_lines")
    assert "fgets" not in body and "strtok" not in body
    assert body.index("console_startup_read_line(") < body.index("hs_compile_and_evaluate(buffer)")
    assert body.index("if (read < 0)") < body.index("hs_compile_and_evaluate(buffer)")


READER_MAIN = r"""
#include <stdio.h>
#include <string.h>
READER
int main(int argc, char **argv)
{
    FILE *file = fopen(argv[1], "rb");
    char buffer[200];
    int read;

    while ((read = console_startup_read_line(file, buffer, sizeof(buffer) - 1)) != 0)
        printf("%d|%s\n", read, read > 0 ? buffer : "");
    return 0;
}
"""


def read_lines(tmp_path, data):
    """The real console_startup_read_line, compiled natively, run over data: [(result, line), ...]."""
    source = tmp_path / "reader.c"
    source.write_text(READER_MAIN.replace("READER", function(CONSOLE, "console_startup_read_line")))
    executable = tmp_path / "reader"
    subprocess.run([os.environ.get("CC", "clang"), "-std=gnu99", "-Wall", "-Werror", str(source), "-o", str(executable)],
                   check=True, capture_output=True)
    (tmp_path / "init.txt").write_bytes(data)
    output = subprocess.run([str(executable), str(tmp_path / "init.txt")], check=True, capture_output=True).stdout
    return [(int(line.split(b"|", 1)[0]), line.split(b"|", 1)[1].decode()) for line in output.splitlines()]


@pytest.mark.skipif(not shutil.which(os.environ.get("CC", "clang")), reason="needs a C compiler")
@pytest.mark.parametrize("length,fits", [(196, True), (197, True), (198, False), (199, False), (600, False)])
@pytest.mark.parametrize("ending", [b"\n", b"\r\n", b""])
def test_a_line_runs_whole_or_not_at_all(tmp_path, length, fits, ending):
    """The class: a line of any length, with any ending, either runs whole or is skipped whole, and the next line still
    runs intact (no piece of a long line ever runs). 197 bytes is the most that fits (the buffer is 200, fgets is given 199);
    with CRLF the \\r takes one of them."""
    line = b"(" + b"x" * (length - 2) + b")"
    fits = fits and length + len(ending) - (1 if ending else 0) <= 197
    results = read_lines(tmp_path, line + ending + b"(next)\n" if ending else line)
    expected = [(1, line.decode())] if fits else [(-1, "")]
    if ending:
        expected.append((1, "(next)"))
    assert results == expected


@pytest.mark.skipif(not shutil.which(os.environ.get("CC", "clang")), reason="needs a C compiler")
def test_tabs_and_blank_lines_do_not_cut_a_command(tmp_path):
    """The original strtok on tabs ran "(foo<TAB>bar)" as "(foo"; tabs are spaces now, and a blank line is an empty one."""
    assert read_lines(tmp_path, b"(foo\tbar)\n\n(baz)") == [(1, "(foo bar)"), (1, ""), (1, "(baz)")]
