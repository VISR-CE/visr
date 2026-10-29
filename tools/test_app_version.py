"""Exercise configure-time version resolution with real Git repositories and CMake."""
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


def run(*args, cwd):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


@pytest.fixture
def version_repo(tmp_path):
    repo = tmp_path / 'source'
    repo.mkdir()
    run('git', 'init', cwd=repo)
    run('git', 'config', 'user.name', 'Version Test', cwd=repo)
    run('git', 'config', 'user.email', 'version@example.invalid', cwd=repo)
    (repo / 'tracked').write_text('original\n')
    run('git', 'add', 'tracked', cwd=repo)
    run('git', 'commit', '--message', 'fixture', cwd=repo)
    run('git', 'tag', 'visr-v2.4.6', cwd=repo)
    run('git', 'tag', 'unrelated-v9.9.9', cwd=repo)
    (repo / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.24)
project(VersionTest NONE)
set(ROOT "${{CMAKE_CURRENT_SOURCE_DIR}}")
include("{ROOT / 'port/ios/AppVersion.cmake'}")
file(WRITE "${{CMAKE_BINARY_DIR}}/version.txt" "${{HALO_MARKETING_VERSION}}\n${{HALO_BUILD_DESCRIPTION}}\n")
''')
    run('git', 'add', 'CMakeLists.txt', cwd=repo)
    run('git', 'commit', '--message', 'configure fixture', cwd=repo)
    return repo


def configure(repo, *options):
    build = repo.parent / 'build'
    run('cmake', '-S', str(repo), '-B', str(build), *options, cwd=repo)
    return (build / 'version.txt').read_text().splitlines()


def test_dev_version_uses_reachable_tag_and_refreshes_dirty_state(version_repo):
    repo = version_repo
    sha = run('git', 'rev-parse', 'HEAD', cwd=repo)[:9]
    assert configure(repo) == ['2.4.6', f'2.4.6-dev ({sha})']
    (repo / 'tracked').write_text('modified\n')
    assert configure(repo) == ['2.4.6', f'2.4.6-dev ({sha}+)']
    run('git', 'checkout', '--', 'tracked', cwd=repo)
    (repo / 'untracked').write_text('new\n')
    assert configure(repo) == ['2.4.6', f'2.4.6-dev ({sha}+)']
    (repo / 'untracked').unlink()
    run('git', 'tag', 'visr-v2.5.0', cwd=repo)
    assert configure(repo) == ['2.5.0', f'2.5.0-dev ({sha})']


def test_explicit_release_version_has_no_dev_suffix(version_repo):
    (version_repo / 'tracked').write_text('dirty release\n')
    assert configure(version_repo, '-DHALO_MARKETING_VERSION=0.6.3') == ['0.6.3', '0.6.3']


def test_no_git_falls_back_to_old_numeric_default(version_repo):
    assert configure(version_repo, '-DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE') == ['0.1.2', '0.1.2']


def test_source_archive_falls_back_to_old_numeric_default(version_repo):
    shutil.rmtree(version_repo / '.git')
    assert configure(version_repo) == ['0.1.2', '0.1.2']


def test_unreachable_release_tag_is_ignored(version_repo):
    repo = version_repo
    original = run('git', 'rev-parse', 'HEAD', cwd=repo)
    run('git', 'checkout', '--orphan', 'unrelated', cwd=repo)
    run('git', 'commit', '--message', 'unrelated history', cwd=repo)
    run('git', 'tag', 'visr-v99.0.0', cwd=repo)
    run('git', 'checkout', original, cwd=repo)
    sha = run('git', 'rev-parse', 'HEAD', cwd=repo)[:9]
    assert configure(repo) == ['2.4.6', f'2.4.6-dev ({sha})']


@pytest.mark.skipif(__import__('sys').platform != 'darwin', reason='CoreFoundation requires macOS')
@pytest.mark.parametrize('description,expected', [
    ('0.6.3-dev (123456789+)', '0.6.3-dev (123456789+)'),
    (None, '0.6.3'), ('', '0.6.3'), (123, '0.6.3'),
])
def test_runtime_version_prefers_nonempty_description(tmp_path, description, expected):
    import plistlib

    bundle = tmp_path / 'Version.app'
    (bundle / 'Contents').mkdir(parents=True)
    info = {'CFBundleIdentifier': 'org.example.version', 'CFBundleShortVersionString': '0.6.3'}
    if description is not None:
        info['VISRBuildDescription'] = description
    (bundle / 'Contents/Info.plist').write_bytes(plistlib.dumps(info))
    probe = tmp_path / 'version.c'
    probe.write_text('''#include "host_app_version.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)argv[1], strlen(argv[1]), true);
    CFBundleRef bundle = CFBundleCreate(NULL, url);
    char text[128];
    if (!bundle || !CFStringGetCString(host_app_version_string(bundle), text, sizeof(text), kCFStringEncodingUTF8)) return 1;
    puts(text);
    CFRelease(bundle);
    CFRelease(url);
    return 0;
}
''')
    binary = tmp_path / 'version'
    run('xcrun', 'clang', '-I', str(ROOT / 'port/ios/host'), str(probe),
        '-framework', 'CoreFoundation', '-o', str(binary), cwd=tmp_path)
    assert run(str(binary), str(bundle), cwd=tmp_path) == expected


def test_clearing_release_override_restores_dev_identity(version_repo):
    repo = version_repo
    assert configure(repo, '-DHALO_MARKETING_VERSION=0.6.3') == ['0.6.3', '0.6.3']
    sha = run('git', 'rev-parse', 'HEAD', cwd=repo)[:9]
    assert configure(repo, '-DHALO_MARKETING_VERSION=') == ['2.4.6', f'2.4.6-dev ({sha})']
