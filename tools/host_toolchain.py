"""Which Mac can build the arm64 targets, and where its Homebrew toolchain lives.

The builds target arm64 (the guest is cross-compiled with Homebrew LLVM, and the apps are built with
`-arch arm64`), so any Mac with full Xcode can build them, Intel included. Only running arm64 code
(the guest probe runner, arm64 simulators, the Mac runner app) needs an Apple Silicon host.
"""
from __future__ import annotations

import platform
import subprocess
from pathlib import Path
from typing import Callable, Iterable

HOMEBREW_PREFIXES = ('/opt/homebrew', '/usr/local')


def check_build_host(system: str | None = None) -> str | None:
    """Return why this host cannot build, or None. Any Mac can: the targets are arm64 cross-builds."""
    system = platform.system() if system is None else system
    if system != 'Darwin':
        return 'a Mac with full Xcode is required'
    return None


def check_run_host(what: str, system: str | None = None, machine: str | None = None) -> str | None:
    """Return why this host cannot run arm64 code (`what` names it), or None."""
    system = platform.system() if system is None else system
    machine = platform.machine() if machine is None else machine
    if system != 'Darwin' or machine != 'arm64':
        return f'{what} runs arm64 code and needs an Apple Silicon Mac; this host can build but not run it'
    return None


def _brew_prefix(formula: str) -> str | None:
    try:
        out = subprocess.run(['brew', '--prefix', formula], capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.SubprocessError):
        return None
    prefix = out.stdout.strip()
    return prefix if out.returncode == 0 and prefix else None


def find_homebrew_tool(
    formula: str,
    relative: str,
    *,
    prefixes: Iterable[str] = HOMEBREW_PREFIXES,
    exists: Callable[[Path], bool] = Path.exists,
    brew_prefix: Callable[[str], str | None] = _brew_prefix,
) -> str:
    """Path of `relative` inside the formula's `opt/<formula>`, searching each Homebrew prefix.

    Falls back to `brew --prefix`, then to the first prefix's path so a missing tool is reported by
    name where it was expected.
    """
    prefixes = list(prefixes)
    for prefix in prefixes:
        candidate = Path(prefix) / 'opt' / formula / relative
        if exists(candidate):
            return str(candidate)
    brewed = brew_prefix(formula)
    if brewed:
        candidate = Path(brewed) / relative
        if exists(candidate):
            return str(candidate)
    return str(Path(prefixes[0]) / 'opt' / formula / relative)


def find_llvm(**kwargs) -> str:
    """LLVM installation root (the directory holding bin/clang)."""
    return str(Path(find_homebrew_tool('llvm', 'bin/clang', **kwargs)).parent.parent)


def find_lld(**kwargs) -> str:
    return find_homebrew_tool('lld', 'bin/ld.lld', **kwargs)


def llvm_tool(name: str, **kwargs) -> str:
    return str(Path(find_llvm(**kwargs)) / 'bin' / name)
