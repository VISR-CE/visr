"""Host-side reads of profile keys that a profile may set for the variable workload alone.

`host_config_*` reads the base configuration: it does not see `[variable.display]` or a scene class's table, so a
host-side read of such a key returns the predictable workload's value whatever the game resolved for the map. The
game resolves them at map load and hands the host the value through a guest import (`host_dynamic_resolution_scene`,
`host_frames_in_flight_scene`). The class: a new `PROFILE_VARIABLE` key read with `host_config_*`. Each existing
read below was checked to be about the base configuration on purpose; add to the list only after checking that."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
KEYS = ROOT / "port/linux/src/port_profile_keys.h"
HOST_READ = re.compile(r'\bhost_config_[a-z_]+\s*\(\s*"([^"]+)"')
# (file, key): host reads that predate this test and take the base configuration's value on purpose
ALLOWED = {("port/ios/host/host_benchmark.m", "display.render_scale"),
           ("port/ios/host/host_stereo.m", "display.anti_aliasing"),
           ("port/ios/host/host_stereo.m", "display.shadow_map_size"),
           ("port/ios/host/host_theater.m", "display.render_scale")}


def host_sources():
    for path in sorted((ROOT / "port/ios").rglob("*")):
        if path.suffix in {".m", ".c", ".h", ".mm"} and "tests" not in path.parts and "build" not in path.parts:
            yield path


def test_no_host_read_takes_a_variable_context_key_from_the_base_configuration():
    variable = set(re.findall(r'^PROFILE_VARIABLE\("([^"]+)"', KEYS.read_text(), re.M))
    assert "display.frames_in_flight_at_30" in variable
    found = {(str(path.relative_to(ROOT)), key) for path in host_sources()
             for key in HOST_READ.findall(path.read_text(errors="replace")) if key in variable}
    assert found - ALLOWED == set(), (
        "a host_config_* read of a PROFILE_VARIABLE key sees only the predictable workload's value: push the resolved "
        "value through a guest import (see host_frames_in_flight_scene)")
    assert ALLOWED - found == set(), "an allowed read is gone: remove it from ALLOWED"
