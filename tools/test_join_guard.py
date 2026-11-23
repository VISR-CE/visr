"""A join request while a join is under way is refused before the assertion."""
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANAGER = (ROOT / "source/networking/network_client_manager.c").read_text()
UI = (ROOT / "source/interface/ui_widget_event_handler_functions.c").read_text()


def test_probe_of_the_guard_verdicts():
    with tempfile.TemporaryDirectory() as directory:
        executable = Path(directory) / "network_join_guard_probe"
        subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-o", str(executable),
                        str(ROOT / "port/ios/tests/network_join_guard_probe.c")], check=True)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr


def test_the_assertion_names_the_failing_clause():
    start = MANAGER.index("boolean network_game_client_initiate_join_game(")
    body = MANAGER[start:MANAGER.index("client->join_in_progress = TRUE;", start)]
    assert "halo_network_join_refusal_name(" in body
    assert "match_vassert(" in body and "match_assert(" not in body


def test_every_call_site_asks_the_guard_first():
    # each caller that is not the guard's own backstop checks it before starting a join
    available = MANAGER[MANAGER.index("network_game_client_join_available_game(\n"):]
    for source, name in ((UI, "ui_widget_event_handler_functions.c"), (available, "join_available_game")):
        calls = list(re.finditer(r"(?<!boolean )network_game_client_initiate_join_game\(", source))
        assert calls, name
        for call in calls:
            before = source[max(0, call.start() - 2500):call.start()]
            assert "network_game_client_can_initiate_join(" in before, name
    # a refused request is ignored: it must not abort the join under way
    refused = UI.index("network_game_client_can_initiate_join(", UI.index("ui_widget_port_join_at_port("))
    abort = UI.index("network_game_abort();", refused)
    assert UI.index("return FALSE;", refused) < abort
