"""Pure-Python checks that the settings defaults in separate files agree, shared by tools/ios_test.py (which runs them
after the native probes) and tools/test_default_settings_check.py (which ci_test collects, so a mismatch fails the
first CI round instead of the landing check). Each check is a function of parsed data, so a test can feed it a broken
copy; the check_* wrappers read the repository and exit with the message. The defaults come from the parsers the
repository already has: profile_check.profile_keys (port_profile_keys.h), settings_schema.config_settings
(port_config.c) and mac_run itself (DEFAULTS, prepare)."""

import inspect
from pathlib import Path

import mac_run
import profile_check
import settings_schema

ROOT = Path(__file__).resolve().parents[1]
HOST_THEATER = ROOT / "port/ios/host/host_theater.m"
# external profile policy, never a config.toml setting, so a runner does not pin it
NOT_PINNED = ("display.output_height",)


def unpinned_profile_keys(keys, defaults):
    """Profile keys that mac_run's DEFAULTS does not set (so a device profile could move a check-metal run)."""
    if not keys:
        raise ValueError("no profile keys parsed from port_profile_keys.h")
    return [key for key in keys if key not in NOT_PINNED and key not in defaults]


def mac_run_marks_chosen(prepare_source):
    """Whether prepare() marks the settings it writes chosen in config.origins."""
    return "chosen_origins(" in prepare_source


def foveation_default_errors(settings, host_theater_text, defaults):
    """HEAD mode renders through foveation unless the player turns it off: the game's default, the host's fallback
    (host_theater.m, before the game has written config.toml) and mac_run's defaults agree."""
    errors = []
    if settings["display.foveation"]["default"] is not True \
            or 'host_config_string("display.foveation", "true"' not in host_theater_text:
        errors.append("port_config.c and host_theater.m: display.foveation must default to true")
    if defaults.get("display.foveation") != "true" or defaults.get("debug.foveation_eye_passes") != "true":
        errors.append("mac_run.py: DEFAULTS must follow port_config.c's foveation defaults")
    return errors


def config_default_errors(settings):
    """Defaults port_config.c must keep: what a player is asked before anything reaches the network, and how HEAD mode turns."""
    errors = []
    if settings["input.turn"]["default"] != "snap":
        errors.append('port_config.c: input.turn must default to "snap"')
    if settings["network.online"]["x-defaults"]["apple"] is not False:
        errors.append("port_config.c: network.online stays off by default on Apple builds; internet play on by default "
                      'is DEFAULT_ONLINE_ASK "true", which asks before any server is reached')
    if settings["network.map_download"]["x-defaults"]["apple"] is not False:
        errors.append("port_config.c: network.map_download stays off by default on Apple builds until a download asks first")
    return errors


def check_mac_run_defaults():
    unpinned = unpinned_profile_keys(profile_check.profile_keys(), mac_run.DEFAULTS)
    if unpinned:
        raise SystemExit(f"mac_run.py: DEFAULTS must set every setting a device profile may set; missing {unpinned}")
    if not mac_run_marks_chosen(inspect.getsource(mac_run.prepare)):
        raise SystemExit("mac_run.py: prepare must mark the settings it writes chosen in config.origins")


def check_foveation_defaults():
    errors = foveation_default_errors(settings_schema.config_settings(), HOST_THEATER.read_text(), mac_run.DEFAULTS)
    if errors:
        raise SystemExit("; ".join(errors))


def check_config_defaults():
    errors = config_default_errors(settings_schema.config_settings())
    if errors:
        raise SystemExit("; ".join(errors))
