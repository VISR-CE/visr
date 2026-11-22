#!/usr/bin/env python3
"""Syntax-check every host source for every Apple platform it ships on.

A host file under port/ios/host/ (or port/macos/) that only one platform compiles, through a
`TARGET_OS_*`, `HALO_TVOS` or other platform `#if` block, is invisible to ios_test and ci_test: they
compile for the build host's own configuration only. A call placed above its declaration inside such a
block then breaks the one device build that nobody ran (the `snapshot_output` use on iPhone and the
`frame_repeat_apply` call on visionOS both did).

This tool takes each platform's real compile commands from CMake instead of listing flags by hand:
it configures port/ios (or port/macos) with the Ninja generator for the platform's SDK, reads
`compile_commands.json`, and re-runs every host translation unit with `-fsyntax-only`. The same CMake
options that gate code get a second pass with every optional feature turned on (the shipped
configuration) next to the plain defaults, so both sides of those `#if`s are compiled.

Needs a Mac with Xcode, the platform SDKs, and the guest-side files that tools/ios_build.py makes
(build/ios/embedded, build/ios/host); `--prepare` makes them when they are missing.

    python3 tools/host_syntax_check.py                 # every platform and variant
    python3 tools/host_syntax_check.py --platform xros --variant features
"""
from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

if __package__:
    from . import host_toolchain, ios_build
else:
    import host_toolchain
    import ios_build

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/host-syntax'
# Sources under these folders are the host files this check covers.
HOST_DIRS = (ROOT / 'port/ios/host', ROOT / 'port/macos')
# What the compile of each source must not be allowed to get away with.
STRICT_FLAGS = ('-fsyntax-only', '-Werror=implicit-function-declaration', '-Werror=implicit-int')
SYNTAX_SUFFIXES = ('.c', '.m', '.mm', '.cpp')

# name -> (CMake source folder, CMAKE_SYSTEM_NAME, SDK, deployment target, extra definitions)
PLATFORMS = {
    'iphoneos': ('port/ios', 'iOS', 'iphoneos', '16.0', ('-DHALO_RENDERER=BOTH',)),
    'appletvos': ('port/ios', 'tvOS', 'appletvos', '16.0', ('-DHALO_RENDERER=BOTH',)),
    'xros': ('port/ios', 'visionOS', 'xros', '2.0', ('-DHALO_RENDERER=METAL',)),
    'macosx': ('port/macos', 'Darwin', 'macosx', '14.0', ('-DHALO_RENDERER=METAL',)),
}


def feature_definitions():
    """every option that compiles code in or out, on: the configuration the release builds ship"""
    return [
        *ios_build.cloud_definitions('org.steverice.visr', True, None, True, True, True),
        '-DHALO_TESTER_BUILD=ON', '-DHALO_BENCHMARK_ROUTES=ON', '-DHALO_DEVELOPMENT_HOOKS=ON',
        '-DHALO_RENDER_VALIDATION=ON', '-DHALO_BINK=ON',
    ]


def plain_definitions():
    return [*ios_build.cloud_definitions('org.steverice.visr', False, None),
            '-DHALO_TESTER_BUILD=OFF', '-DHALO_BENCHMARK_ROUTES=OFF', '-DHALO_DEVELOPMENT_HOOKS=OFF',
            '-DHALO_RENDER_VALIDATION=OFF', '-DHALO_BINK=OFF']


VARIANTS = {'plain': plain_definitions, 'features': feature_definitions}


def common_definitions():
    return [
        '-DHALO_BUNDLE_IDENTIFIER=org.steverice.visr', '-DHALO_DEVELOPMENT_TEAM=', '-DHALO_RENDER_HEIGHT=0',
        '-DHALO_EXTENDED_VIRTUAL_ADDRESSING=OFF', '-DHALO_BUILD_NUMBER=1', '-DHALO_MARKETING_VERSION=',
        '-DHALO_DATA_PACK_CHANNEL_NAME=',
        *ios_build.data_pack_definitions(ios_build.DATA_PACK_PUBLIC_KEY,
                                         ios_build.DATA_PACK_FEED.format(channel='stable', kind=ios_build.PROFILE_FEED_DIRECTORY).removesuffix('.json'),
                                         ios_build.DATA_PACK_FEED.format(channel='stable', kind='recordings'),
                                         ios_build.DATA_PACK_FEED.format(channel='stable', kind='shaders')),
    ]


def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], cwd=ROOT, **kwargs)


def prepare():
    """the guest-side files the host sources include, made the way tools/ios_build.py makes them"""
    for name, (registry, revision, source) in ios_build.HEADERS.items():
        target = ROOT / 'build/ios/gl_include' / name
        if not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            url = f'https://raw.githubusercontent.com/KhronosGroup/{registry}/{revision}/{source}'
            target.write_bytes(urllib.request.urlopen(url, timeout=30).read())
    run(sys.executable, 'configure.py', '--ios', '--pgo=off', '--lto=off', '--ios-llvm', host_toolchain.find_llvm(),
        '--ios-lld', host_toolchain.find_lld(), check=True)
    run('ninja', 'ios_guest', check=True)
    run(sys.executable, 'tools/ios_bridges.py', check=True)
    run(sys.executable, 'tools/ios_embed_guest.py', 'build/ios/halo_guest.elf', 'build/ios/embedded', check=True)
    notices = ROOT / 'build/ios/licenses'
    notices.mkdir(parents=True, exist_ok=True)


def missing_inputs():
    return [str(path.relative_to(ROOT)) for path in (
        ROOT / 'build/third_party/SDL3', ROOT / 'build/ios/embedded/guest_image.h',
        ROOT / 'build/ios/host/bridges.c', ROOT / 'build/ios/gl_include')
        if not path.exists()]


def configure(platform, variant):
    """CMake-configure one platform/variant with Ninja; returns its compile_commands.json path"""
    source, system, sdk, deployment, extra = PLATFORMS[platform]
    folder = BUILD / f'{platform}-{variant}'
    command = ['cmake', '-S', source, '-B', folder, '-G', 'Ninja', f'-DCMAKE_SYSTEM_NAME={system}',
               f'-DCMAKE_OSX_SYSROOT={sdk}', '-DCMAKE_OSX_ARCHITECTURES=arm64',
               f'-DCMAKE_OSX_DEPLOYMENT_TARGET={deployment}', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
               *common_definitions(), *VARIANTS[variant](), *extra]
    if platform == 'macosx':
        command.append('-DHALO_NATIVE_MAC=ON')
    result = run(*command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if result.returncode:
        raise SystemExit(f'{platform}/{variant}: cmake configure failed\n{result.stdout[-3000:]}')
    return folder / 'compile_commands.json'


def host_entries(compile_commands):
    """the compile commands for host sources, one per file"""
    entries = {}
    for entry in json.loads(Path(compile_commands).read_text()):
        path = Path(entry['file'])
        if path.suffix in SYNTAX_SUFFIXES and any(folder in path.parents for folder in HOST_DIRS):
            entries[str(path)] = entry
    return entries


def syntax_command(command_line):
    """a compile command with its object output dropped and the strict syntax-only flags added"""
    args = shlex.split(command_line)
    result = []
    skip = False
    for arg in args:
        if skip:
            skip = False
        elif arg == '-o':
            skip = True
        elif arg in ('-c', '-MD', '-MMD') or arg.startswith('-MF') or arg in ('-MT', '-MQ'):
            continue
        else:
            result.append(arg)
    # keep the compiler and its flags, add ours before the source file (last argument)
    return result[:-1] + list(STRICT_FLAGS) + result[-1:]


def check_one(entry):
    """(file, ok, diagnostics) for one compile-command entry, run from its own directory"""
    command = syntax_command(entry['command'])
    result = subprocess.run(command, cwd=entry.get('directory') or ROOT, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    return entry['file'], result.returncode == 0, result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--platform', action='append', choices=sorted(PLATFORMS), help='only these (default: all)')
    parser.add_argument('--variant', action='append', choices=sorted(VARIANTS), help='only these (default: all)')
    parser.add_argument('--prepare', action='store_true', help='make the guest-side inputs when they are missing')
    parser.add_argument('--jobs', type=int, default=min(8, os.cpu_count() or 4))
    parser.add_argument('--keep', action='store_true', help='keep the CMake folders under build/host-syntax')
    args = parser.parse_args()
    problem = host_toolchain.check_build_host()
    if problem:
        raise SystemExit(problem)
    if missing_inputs():
        if not args.prepare:
            raise SystemExit('missing build inputs (run with --prepare, or tools/ios_build.py once): ' + ', '.join(missing_inputs()))
        prepare()
    platforms = args.platform or list(PLATFORMS)
    variants = args.variant or list(VARIANTS)
    combos = [(p, v) for p in platforms for v in variants]
    BUILD.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=min(4, len(combos))) as pool:
        databases = list(pool.map(lambda pv: configure(*pv), combos))
    jobs = []
    covered = set()
    for (platform, variant), database in zip(combos, databases):
        entries = host_entries(database)
        covered.update(entries)
        jobs.extend((f'{platform}/{variant}', entry) for entry in entries.values())
    failures = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for (label, _), (file, ok, output) in zip(jobs, pool.map(lambda job: check_one(job[1]), jobs)):
            if not ok:
                failures.append((label, file, output))
    for label, file, output in failures:
        print(f'FAIL [{label}] {Path(file).relative_to(ROOT)}\n{output.rstrip()}\n', file=sys.stderr)
    print(f'host syntax check: {len(jobs)} compiles, {len(covered)} distinct host files, '
          f'{len(combos)} configurations, {len(failures)} failed')
    if not args.keep:
        shutil.rmtree(BUILD, ignore_errors=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
