#!/usr/bin/env python3
"""Build and run an isolated UIKit simulator regression app.

The script creates and removes only its own simulator. Every simctl operation
names that simulator's UDID.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import plistlib
import shutil
import signal
import subprocess
import tempfile
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
BUNDLE = "org.visr.tests.import-layout"


def run(*args: str, capture: bool = False) -> str:
    result = subprocess.run(args, check=True, text=True, stdout=subprocess.PIPE if capture else None)
    return result.stdout.strip() if capture else ""


def interrupted(signum: int, frame: object) -> None:
    raise SystemExit(128 + signum)


def main() -> int:
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGHUP, interrupted)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--runtime", help="Available iOS simulator runtime identifier")
    parser.add_argument("--device-type", default="com.apple.CoreSimulator.SimDeviceType.iPhone-17-Pro")
    parser.add_argument("--timeout", default=180, type=int)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    runtimes = json.loads(run("xcrun", "simctl", "list", "runtimes", "--json", capture=True))["runtimes"]
    available = [item for item in runtimes if item.get("isAvailable") and
                 item["identifier"].startswith("com.apple.CoreSimulator.SimRuntime.iOS-")]
    if not available and not args.runtime:
        parser.error("No available iOS simulator runtime")
    runtime = args.runtime or max(available, key=lambda item: tuple(map(int, item["version"].split("."))))["identifier"]
    udid = None
    with tempfile.TemporaryDirectory(prefix="visr-import-layout-") as temporary:
        app = Path(temporary) / "ImportLayout.app"
        app.mkdir()
        info = {
            "CFBundleIdentifier": BUNDLE, "CFBundleExecutable": "ImportLayout",
            "CFBundleName": "Import Layout", "CFBundleVersion": "1", "CFBundleShortVersionString": "1.0",
            "CFBundlePackageType": "APPL", "LSRequiresIPhoneOS": True, "MinimumOSVersion": "16.0",
            "UIDeviceFamily": [1, 2], "UILaunchScreen": {},
            "UIApplicationSceneManifest": {"UIApplicationSupportsMultipleScenes": False,
                "UISceneConfigurations": {"UIWindowSceneSessionRoleApplication": [
                    {"UISceneConfigurationName": "Default", "UISceneDelegateClassName": "ProbeScene"}]}},
            "UISupportedInterfaceOrientations": ["UIInterfaceOrientationPortrait", "UIInterfaceOrientationLandscapeLeft",
                                                  "UIInterfaceOrientationLandscapeRight"],
        }
        (app / "Info.plist").write_bytes(plistlib.dumps(info))
        icon = ROOT / "port/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-120.png"
        if icon.exists():
            shutil.copyfile(icon, app / "AppIcon60x60.png")
        sdk = run("xcrun", "--sdk", "iphonesimulator", "--show-sdk-path", capture=True)
        run("xcrun", "--sdk", "iphonesimulator", "clang", "-target", "arm64-apple-ios16.0-simulator",
            "-isysroot", sdk, "-fobjc-arc", "-fmodules", "-Wno-incompatible-pointer-types",
            "-I", str(ROOT / "port/ios/host"), "-I", str(ROOT / "port/runtime"),
            "-I", str(ROOT / "port/runtime/include"),
            str(ROOT / "port/ios/tests/import_layout_probe.m"),
            str(ROOT / "port/ios/host/host_orientation.m"), str(ROOT / "port/ios/host/host_upscale_tick.m"),
            "-framework", "UIKit", "-framework", "Foundation", "-framework", "QuartzCore",
            "-framework", "UniformTypeIdentifiers", "-o", str(app / "ImportLayout"))
        run("codesign", "--force", "--sign", "-", str(app))
        try:
            udid = run("xcrun", "simctl", "create", f"VISR Import Layout {uuid.uuid4().hex[:8]}",
                       args.device_type, runtime, capture=True)
            # The job runner can clean up the owned simulator after interruption.
            if cleanup_file := os.environ.get("HEAVY_JOB_CLEANUP_FILE"):
                with open(cleanup_file, "a", encoding="utf-8") as cleanup:
                    cleanup.write(f"xcrun simctl shutdown {udid} >/dev/null 2>&1 || true\n")
                    cleanup.write(f"xcrun simctl delete {udid} >/dev/null 2>&1 || true\n")
            run("xcrun", "simctl", "boot", udid)
            run("xcrun", "simctl", "bootstatus", udid, "-b")
            run("xcrun", "simctl", "install", udid, str(app))
            data = Path(run("xcrun", "simctl", "get_app_container", udid, BUNDLE, "data", capture=True))
            report = {"cases": [], "failures": [], "simulatorRuntime": runtime,
                      "simulatorDeviceType": args.device_type}
            for category, size in [("default", "large"), ("accessibility", "accessibility-extra-extra-extra-large")]:
                run("xcrun", "simctl", "ui", udid, "content_size", size)
                stdout, stderr = args.output / f"{category}-stdout.log", args.output / f"{category}-stderr.log"
                launched = run("xcrun", "simctl", "launch", f"--stdout={stdout}", f"--stderr={stderr}",
                    "--terminate-running-process", udid, BUNDLE, category, capture=True)
                print(launched, flush=True)
                process_id = int(launched.rsplit(":", 1)[1])
                unified = args.output / f"{category}-unified.log"

                def collect_log() -> str:
                    logs = run("xcrun", "simctl", "spawn", udid, "log", "show", "--style", "compact",
                               "--last", "5m", "--predicate", f"processIdentifier == {process_id}", capture=True)
                    unified.write_text(logs)
                    return logs
                source = data / "Documents/layout" / category
                deadline = time.monotonic() + args.timeout
                while not (source / "report.json").exists():
                    try:
                        os.kill(process_id, 0)
                    except ProcessLookupError:
                        collect_log()
                        raise RuntimeError(f"Probe exited before report; see {unified}") from None
                    if time.monotonic() >= deadline:
                        collect_log()
                        raise TimeoutError(f"UIKit probe did not produce {category}/report.json; see {stderr}")
                    time.sleep(0.25)
                shutil.copytree(source, args.output / category, dirs_exist_ok=True)
                partial = json.loads((source / "report.json").read_text())
                report["cases"].extend(partial["cases"])
                report["failures"].extend(partial["failures"])
                for key in ("viewportMethod", "systemVersion", "simulatorModel"):
                    if key in partial:
                        report[key] = partial[key]
                diagnostics = stdout.read_text() + stderr.read_text() + collect_log()
                if "Unable to simultaneously satisfy constraints" in diagnostics or "uncaught exception" in diagnostics.lower():
                    report["failures"].append(f"{category}: UIKit constraint or exception diagnostics; see {stderr}")
            report["passed"] = not report["failures"]
            (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
            print(f"{len(report['cases'])} cases; {len(report['failures'])} failures; artifacts: {args.output}")
            for failure in report["failures"]:
                print(f"FAIL: {failure}")
            return 0 if report["passed"] else 1
        finally:
            if udid:
                subprocess.run(["xcrun", "simctl", "shutdown", udid], check=False)
                subprocess.run(["xcrun", "simctl", "delete", udid], check=False)


if __name__ == "__main__":
    raise SystemExit(main())
