"""Traced object shadows without Metal: the game's records, skinning and the caster cache
(port/ios/host/traced_shadows.h), compiled and run from port/ios/tests/traced_shadows_probe.c."""
import pathlib
import shutil
import subprocess
import tempfile

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def test_records_skinning_and_caster_cache():
    compiler = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
    if not compiler:
        pytest.skip("no C compiler")
    with tempfile.TemporaryDirectory() as temp:
        executable = pathlib.Path(temp) / "probe"
        subprocess.run([
            compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1",
            "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/linux/src"),
            str(ROOT / "port/ios/tests/traced_shadows_probe.c"), "-lm", "--output", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)


TRACED_KEYS = ("display.traced_shadows", "display.traced_lights")


@pytest.mark.parametrize("key", TRACED_KEYS)
def test_every_traced_setting_is_a_profile_choice_that_only_enhanced_keeps(key):
    """The class: a setting that traces rays is off unless a device profile turns it on (only GPUs with ray tracing
    hardware should), a profile may set it apart for the variable workload, and Classic and Hardcore turn it off
    whatever the profile says. A traced setting missing from one of the three tables would trace where it must not."""
    import re
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    entry = re.search(r'\{ "%s", _config_(boolean|integer), "([^"]*)"' % re.escape(key), config)
    assert entry and entry.group(2) in ("false", "0"), key
    keys = (ROOT / "port/linux/src/port_profile_keys.h").read_text()
    assert re.search(r'^PROFILE_(BOOLEAN|INTEGER)\("%s"' % re.escape(key), keys, re.M), key
    assert 'PROFILE_VARIABLE("%s")' % key in keys, key
    modes = (ROOT / "port/linux/src/port_graphics_modes.h").read_text()
    row = re.search(r'X\("%s", "([^"]*)", "([^"]*)"\)' % re.escape(key), modes)
    assert row and set(row.groups()) <= {"false", "0"}, key


def test_no_other_setting_traces_outside_the_class():
    """Every display.traced_* setting in the settings table is in TRACED_KEYS, so a new one is checked above."""
    import re
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    assert set(re.findall(r'\{ "(display\.traced_[a-z_]+)"', config)) == set(TRACED_KEYS)
