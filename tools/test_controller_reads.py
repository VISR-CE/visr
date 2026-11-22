"""The game's controller reads start from current state: on the Apple hosts' game thread SDL updates its controllers
only when asked, so every read must call controllers_update (the host's SDL_UpdateJoysticks, skipped when another
read of the poll just made it) before it lists the pads. Order matters: an update after sdl_gamepads is a read of
state one poll old."""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sdl_event_callbacks as callbacks  # noqa: E402

SOURCE = (Path(__file__).resolve().parent.parent / "port/linux/src/xinput_sdl.c").read_text()


def body(name: str) -> str:
    start = SOURCE.index(name + "(")
    start = SOURCE.index("{", start)
    depth, index = 1, start + 1
    while depth:
        depth += {"{": 1, "}": -1}.get(SOURCE[index], 0)
        index += 1
    return SOURCE[start:index]


def test_every_read_updates_before_it_lists_the_pads():
    for name in ("XInputGetState", "halo_look_stick_latest"):
        text = body(name)
        update, listing = text.find("controllers_update()"), text.find("sdl_gamepads(")
        assert 0 <= update < listing, f"{name} lists the pads before (or without) controllers_update"


def test_only_the_helper_reaches_sdl_update_joysticks():
    assert len(re.findall(r"\bSDL_UpdateJoysticks\(\)", SOURCE)) == 1
    assert "static void controllers_update(void)" in SOURCE


def test_the_readers_of_gamepad_state_are_the_known_ones():
    """A new reader of SDL_GetGamepadAxis or SDL_GetGamepadButton is a new read that must call controllers_update
    first (or be added here on purpose: gamepad_idle only decides which pad is idle, for port assignment)."""
    functions = callbacks.functions(callbacks.strip_comments(SOURCE))
    readers = {name for name, text in functions.items() if re.search(r"\bSDL_GetGamepad(?:Axis|Button)\(", text)}
    assert readers == {"gamepad_idle", "sdl_gamepad_state", "halo_look_stick_latest"}
    callers = {name for name, text in functions.items() if name != "sdl_gamepad_state" and re.search(r"\bsdl_gamepad_state\(", text)}
    assert callers == {"XInputGetState"}
