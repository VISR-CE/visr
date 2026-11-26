"""The settings-default agreement checks (tools/default_settings_check.py) pass on the repository and fail on a broken copy.

tools/ios_test.py ran them after its native probes, so ci_test never did and a mismatch reached the landing check;
they run here under ci_test, and ios_test calls the same functions."""

import re
from pathlib import Path

import pytest

import default_settings_check as check

ROOT = Path(__file__).resolve().parents[1]
KEYS = ["display.foveation", "display.output_height", "a.b"]
DEFAULTS = {"display.foveation": "true", "debug.foveation_eye_passes": "true", "a.b": "1"}
SETTINGS = {"display.foveation": {"default": True}, "input.turn": {"default": "snap"},
            "network.online": {"default": False, "x-defaults": {"apple": False, "desktop": True}},
            "network.map_download": {"default": False, "x-defaults": {"apple": False, "desktop": True}}}
THEATER = 'host_config_string("display.foveation", "true", 1)'


def with_default(settings, name, key, value):
    copy = {setting: dict(entry) for setting, entry in settings.items()}
    copy[name][key] = value
    return copy


def test_the_repository_agrees():
    check.check_mac_run_defaults()
    check.check_foveation_defaults()
    check.check_config_defaults()


def test_a_profile_key_mac_run_does_not_set_is_found():
    assert check.unpinned_profile_keys(KEYS, DEFAULTS) == []
    assert check.unpinned_profile_keys(KEYS + ["new.key"], DEFAULTS) == ["new.key"]


def test_a_key_named_only_as_a_value_is_not_pinned():
    assert check.unpinned_profile_keys(KEYS + ["x.y"], {**DEFAULTS, "other": "x.y"}) == ["x.y"]


def test_no_profile_keys_raises_rather_than_passing_empty():
    with pytest.raises(ValueError, match="no profile keys"):
        check.unpinned_profile_keys([], DEFAULTS)


def test_prepare_must_mark_what_it_writes_chosen():
    assert check.mac_run_marks_chosen("def prepare(x):\n    chosen_origins(x)\n")
    assert not check.mac_run_marks_chosen("def prepare(x):\n    pass\n")


def test_foveation_defaults_agree_or_are_named():
    assert check.foveation_default_errors(SETTINGS, THEATER, DEFAULTS) == []
    assert check.foveation_default_errors(with_default(SETTINGS, "display.foveation", "default", False), THEATER, DEFAULTS)
    assert check.foveation_default_errors(SETTINGS, THEATER.replace('"true"', '"false"'), DEFAULTS)
    assert check.foveation_default_errors(SETTINGS, THEATER, {**DEFAULTS, "display.foveation": "false"})
    assert check.foveation_default_errors(SETTINGS, THEATER, {"display.foveation": "true"})


def test_config_defaults_agree_or_are_named():
    assert check.config_default_errors(SETTINGS) == []
    assert check.config_default_errors(with_default(SETTINGS, "input.turn", "default", "smooth"))
    assert check.config_default_errors(with_default(SETTINGS, "network.online", "x-defaults", {"apple": True, "desktop": True}))
    assert check.config_default_errors(with_default(SETTINGS, "network.map_download", "x-defaults", {"apple": True, "desktop": True}))


def test_the_wrapper_exits_with_the_message(monkeypatch):
    monkeypatch.setattr(check.mac_run, "DEFAULTS", {})
    with pytest.raises(SystemExit, match="missing"):
        check.check_mac_run_defaults()


def test_ios_test_keeps_no_port_config_default_check_of_its_own():
    """A pure-Python agreement check on port_config.c lives in default_settings_check.py, where ci_test runs it, not
    inline in the native job: a mismatch there only reached the landing check."""
    text = (ROOT / "tools/ios_test.py").read_text()
    code = "\n".join(line for line in text.split("\n") if not line.lstrip().startswith("#"))
    assert not re.search(r"read_text\(\)[^\n]*", "\n".join(
        line for line in code.split("\n") if "port_config.c" in line and "read_text" in line)), \
        "ios_test.py reads port_config.c itself: move the check into tools/default_settings_check.py"
