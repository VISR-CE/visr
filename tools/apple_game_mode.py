#!/usr/bin/env python3
"""The Game Mode and App Store games-category keys every Apple app target's Info.plist must declare.

This table is the one list of the app plists and of which keys each platform gets;
`tools/test_apple_game_mode.py` fails when a plist drifts from it or when the plists named by
`HALO_INFO_PLIST` in `port/ios/CMakeLists.txt` differ from it, and `tools/test_ios_build.py` reads
`PLISTS` for its per-plist checks. Apple's rules (developer.apple.com Information Property List
reference, and the Developer Forums "How to Enable Game Mode" thread):
  * `LSApplicationCategoryType` names a games category (`public.app-category.action-games` here).
  * `LSSupportsGameMode` (Boolean, iOS and iPadOS 18.6+, macOS 26+): without it Game Mode might not turn on.
  * `GCSupportsGameMode` (Boolean) is its predecessor, deprecated at 18.6, kept for iOS and iPadOS 18.0-18.5
    and macOS 14-15; the forum thread says the two can be used together.
  * tvOS and visionOS have no documented Game Mode, so they get the category only.
Enabling the keys only makes the app eligible; the system decides at runtime.
"""
from __future__ import annotations

from pathlib import Path
import plistlib
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
CATEGORY = {"LSApplicationCategoryType": "public.app-category.action-games"}
GAME_MODE = {"GCSupportsGameMode": True, "LSSupportsGameMode": True}

# plist (relative to the repository root) -> the keys it must declare
PLISTS: dict[str, dict[str, object]] = {
    "port/ios/Info.plist": {**CATEGORY, **GAME_MODE},          # iOS and iPadOS
    "port/ios/Info-macOS.plist": {**CATEGORY, **GAME_MODE},    # Mac Catalyst
    "port/macos/Info.plist": {**CATEGORY, **GAME_MODE},        # AppKit Mac app
    "port/ios/Info-tvOS.plist": dict(CATEGORY),
    "port/ios/Info-visionOS.plist": dict(CATEGORY),
}


def build_plists() -> set[str]:
    """The plists `port/ios/CMakeLists.txt` can pick for the app, as repository-relative paths."""
    text = (ROOT / "port/ios/CMakeLists.txt").read_text()
    found = set()
    for value in re.findall(r"set\(HALO_INFO_PLIST\s+\"?([^\s\")]+)\"?\)", text):
        if "${HALO_INFO_PLIST}" in value:  # the later step that makes a relative path absolute
            continue
        if value.startswith("${HALO_NATIVE_DIR}/"):
            found.add("port/macos/" + value.split("/", 1)[1])
        else:
            found.add("port/ios/" + value)
    return found


def problems() -> list[str]:
    found = []
    for name, required in PLISTS.items():
        with open(ROOT / name, "rb") as handle:
            info = plistlib.load(handle)
        for key, value in required.items():
            actual = info.get(key)
            if type(actual) is not type(value) or actual != value:
                found.append(f"{name}: {key} is {actual!r}, expected {value!r}")
    return found


if __name__ == "__main__":
    issues = problems()
    print("\n".join(issues) or "ok")
    sys.exit(1 if issues else 0)
