"""Tests for tools/host_toolchain.py: the build-host decision and the Homebrew prefix lookup."""
from __future__ import annotations

from pathlib import Path

from tools import host_toolchain as ht


def test_any_mac_can_build_including_intel():
    assert ht.check_build_host('Darwin') is None
    assert 'Mac' in ht.check_build_host('Linux')


def test_running_arm64_code_needs_apple_silicon():
    assert ht.check_run_host('the runner', 'Darwin', 'arm64') is None
    message = ht.check_run_host('the runner', 'Darwin', 'x86_64')
    assert 'the runner' in message and 'Apple Silicon' in message
    assert ht.check_run_host('the runner', 'Linux', 'arm64')


def fake(existing):
    return lambda path: str(path) in existing


def test_finds_llvm_in_the_apple_silicon_prefix():
    exists = fake({'/opt/homebrew/opt/llvm/bin/clang'})
    assert ht.find_llvm(exists=exists, brew_prefix=lambda f: None) == '/opt/homebrew/opt/llvm'


def test_finds_llvm_and_lld_in_the_intel_prefix():
    exists = fake({'/usr/local/opt/llvm/bin/clang', '/usr/local/opt/lld/bin/ld.lld'})
    kwargs = dict(exists=exists, brew_prefix=lambda f: None)
    assert ht.find_llvm(**kwargs) == '/usr/local/opt/llvm'
    assert ht.find_lld(**kwargs) == '/usr/local/opt/lld/bin/ld.lld'
    assert ht.llvm_tool('clang', **kwargs) == '/usr/local/opt/llvm/bin/clang'


def test_prefers_the_first_prefix_when_both_exist():
    exists = fake({'/opt/homebrew/opt/lld/bin/ld.lld', '/usr/local/opt/lld/bin/ld.lld'})
    assert ht.find_lld(exists=exists, brew_prefix=lambda f: None) == '/opt/homebrew/opt/lld/bin/ld.lld'


def test_falls_back_to_brew_prefix_then_to_the_default_path():
    exists = fake({'/custom/llvm/bin/clang'})
    assert ht.find_llvm(exists=exists, brew_prefix=lambda f: '/custom/llvm') == '/custom/llvm'
    assert ht.find_llvm(exists=fake(set()), brew_prefix=lambda f: None) == '/opt/homebrew/opt/llvm'


def test_ios_build_and_configure_use_the_lookup():
    root = Path(__file__).resolve().parents[1]
    for name in ('tools/ios_build.py', 'configure.py', 'tools/ios_test.py'):
        assert '/opt/homebrew' not in (root / name).read_text(), name
