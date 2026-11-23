"""Bug class: a launch's HALO_ environment variable that never reaches the game. The guest gets only the variables the
host names (port/ios/host/host_main.m), so a setting given at launch (devicectl --environment-variables, an Xcode scheme)
was dropped without a word unless a benchmark suite was set; a device session's settings silently did nothing (found on
Nov 23, 2026: an Apple TV session's HALO_SCENE_PRECISION=16 left the scene 8-bit). The host must pass on every HALO_
variable of its own environment, whatever else the launch is."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_every_halo_launch_variable_reaches_the_guest():
    text = (ROOT / "port/ios/host/host_main.m").read_text()
    loop = re.search(r"\n(?P<indent>[ \t]*)\{\s*\n\s*extern char \*\*environ;\s*\n\s*for\(char \*\*launch=environ;.*?"
                     r"strncmp\(\*launch,\"HALO_\",5\)", text, re.S)
    assert loop, "host_main.m must copy every HALO_ variable of the launch's environment into the guest's"
    opening = text[:loop.start() + 1].rstrip().splitlines()[-1]
    assert not re.search(r"\bif\s*\(", opening), f"the HALO_ pass-on is conditional: {opening.strip()}"
