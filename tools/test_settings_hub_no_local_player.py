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


def test_editable_profile_label_handles_no_local_player():
    text = SOURCE.read_text()
    start = text.index("static void get_editable_player_profile_display_name(\n\tstruct widget_instance *widget)\n{")
    body = text[start:]
    body = body[: body.index("\n}\n")]
    guard = re.search(r"if \(widget->local_player_index < 0\)", body)
    assertion = body.index("profile display name requires a valid local player index")
    assert guard and guard.start() < assertion, "negative index must return before the assertion"
    assert "return;" in body[guard.start():assertion], "the guard must return"


def test_hub_data_inputs_do_not_need_a_local_player():
    """The hub's other data inputs (hub status, graphics state, the list's description update) read no local player."""
    menus = Path(__file__).resolve().parents[1] / "port/linux/game/menu_functions.c"
    text = menus.read_text()
    for name in ("static void graphics_state_show(", "static void hub_status_show("):
        start = text.index(name)
        body = text[start: text.index("\n}\n", start)]
        assert "local_player_index" not in body, name
