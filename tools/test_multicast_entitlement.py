"""Every iOS and visionOS entitlements file that port/ios/CMakeLists.txt can pick must carry the multicast
entitlement. CMake defines `HALO_MULTICAST_ENTITLED` only when the chosen file has the key, and without that
define the LAN broadcast code is compiled out (host/host_bonjour.m), so a missing key is silent at runtime."""

import plistlib
import re
from pathlib import Path

import pytest

IOS_DIR = Path(__file__).resolve().parent.parent / "port" / "ios"
KEY = "com.apple.developer.networking.multicast"

# the files the iOS and visionOS branches of CMakeLists.txt choose, plain and extended
BROADCASTING = ["VISR-iOS.entitlements", "VISR-iOS-extended.entitlements",
                "VISR-visionOS.entitlements", "VISR-visionOS-extended.entitlements"]


def _entitlements(name: str) -> dict:
    return plistlib.loads((IOS_DIR / name).read_bytes())


@pytest.mark.parametrize("name", BROADCASTING)
def test_broadcasting_platform_entitlements_include_multicast(name: str) -> None:
    assert _entitlements(name).get(KEY) is True


def test_tvos_entitlements_do_not_claim_multicast() -> None:
    # the tvOS profiles do not carry it, and a claim without the profile fails signing
    assert KEY not in _entitlements("VISR-tvOS-extended.entitlements")


def test_every_entitlements_file_the_ios_and_visionos_branches_pick_is_covered() -> None:
    cmake = (IOS_DIR / "CMakeLists.txt").read_text()
    picked = set(re.findall(r"VISR-(?:iOS|visionOS)[A-Za-z-]*\.entitlements", cmake))
    assert picked == set(BROADCASTING)
