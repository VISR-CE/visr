"""The game's controller reads start from current state: on the Apple hosts' game thread SDL updates its controllers
only when asked, so every read must call controllers_update (the host's SDL_UpdateJoysticks, skipped when another
read of the poll just made it) before it lists the pads. Order matters: an update after sdl_gamepads is a read of
state one poll old."""

import re
from pathlib import Path

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
