#!/usr/bin/env python3
"""Build and run port/ios/tests/frame_workgroup_uikit_probe.m as a small Mac Catalyst app (no game data).

"fixed" must finish without UIKit's "Failed to create work interval" exception. "unfixed" is the control: UIKit is
expected to end it with that exception, which shows the probe still reaches the crash; if a UIKit update stops
raising, the control only warns, since the fix is then untested here rather than broken."""
import os
import plistlib
import subprocess
import sys
from pathlib import Path

REASON = 'Failed to create work interval'


def main():
    root = Path(__file__).resolve().parents[1]
    build = root / 'build/ios/frame-workgroup-uikit-probe'
    app = build / 'FrameWorkgroupProbe.app'
    macos = app / 'Contents/MacOS'
    macos.mkdir(parents=True, exist_ok=True)
    sdk = subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()
    subprocess.run([
        'xcrun', 'clang', '-fobjc-arc', '-Wall', '-Werror', '-target', 'arm64-apple-ios15.0-macabi', '-isysroot', sdk,
        '-iframework', f'{sdk}/System/iOSSupport/System/Library/Frameworks', '-Iport/ios/host',
        'port/ios/tests/frame_workgroup_uikit_probe.m', '-framework', 'UIKit', '-framework', 'Foundation',
        '-framework', 'AudioToolbox', '-o', str(macos / 'FrameWorkgroupProbe'),
    ], cwd=root, check=True)
    with (app / 'Contents/Info.plist').open('wb') as output:
        plistlib.dump(dict(CFBundleExecutable='FrameWorkgroupProbe', CFBundleIdentifier='org.visr.tests.frame-workgroup',
                           CFBundleName='FrameWorkgroupProbe', CFBundlePackageType='APPL', LSRequiresIPhoneOS=True,
                           UIApplicationSceneManifest=dict(UIApplicationSupportsMultipleScenes=False,
                               UISceneConfigurations={'UIWindowSceneSessionRoleApplication': [dict(
                                   UISceneConfigurationName='Default', UISceneDelegateClassName='ProbeDelegate')]})),
                      output)
    subprocess.run(['codesign', '--force', '--sign', '-', str(app)], check=True)
    env = {**os.environ, 'DYLD_FRAMEWORK_PATH': '/System/iOSSupport/System/Library/Frameworks'}

    def run(mode):
        result = subprocess.run([str(macos / 'FrameWorkgroupProbe'), mode], cwd=root, capture_output=True, text=True,
                                timeout=60, env=env)
        print(result.stdout, end='')
        return result

    fixed = run('fixed')
    if fixed.returncode != 0 or REASON in fixed.stderr:
        sys.stderr.write(fixed.stderr[-4000:])
        sys.exit('frame workgroup probe: the dialog held out of the workgroup still failed')
    control = run('unfixed')
    if control.returncode != 0 and REASON in control.stderr:
        print(f'frame workgroup probe (unfixed): UIKit raised "{REASON}", as expected')
    else:
        print(f'frame workgroup probe: WARNING, the control (thread kept in the workgroup) did not raise "{REASON}" '
              f'(exit {control.returncode}); this probe no longer reaches the crash')


if __name__ == '__main__':
    main()
