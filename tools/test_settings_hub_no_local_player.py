"""The settings hub's profile label must not halt when no local player exists."""
import re
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / "source/interface/ui_widget_game_data_input_functions.c"


def test_active_profile_label_handles_no_local_player():
    text = SOURCE.read_text()
    start = text.index("static void get_active_player_profile_display_name(\n\tstruct widget_instance *widget)\n{")
    body = text[start:]
    body = body[: body.index("\n}\n")]
    guard = re.search(r"if \(widget->local_player_index < 0\)", body)
    assertion = body.index("profile display name requires a valid local player index")
    assert guard and guard.start() < assertion, "negative index must return before the assertion"
