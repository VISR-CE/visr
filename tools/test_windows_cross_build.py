"""Check the Windows cross-toolchain directory and case-bridge setup."""

from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from types import SimpleNamespace

import pytest

from tools import windows_cross_build as cross


def test_cross_toolchain_prefix_builds_case_bridge(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    vc = tmp_path / "VC" / "Tools" / "MSVC" / "14.51"
    sdk = tmp_path / "Windows Kits" / "10"
    version = "10.0.26100.0"
    for folder in (
        vc / "Include",
        vc / "Lib" / "x86",
        sdk / "Include" / version / "ucrt",
        sdk / "Include" / version / "um",
        sdk / "Include" / version / "shared",
        sdk / "Include" / version / "winrt",
        sdk / "Lib" / version / "ucrt" / "x86",
        sdk / "Lib" / version / "um" / "x86",
    ):
        folder.mkdir(parents=True)
    (sdk / "Include" / version / "um" / "Windows.h").write_text(
        "#include <DriverSpecs.h>\n", encoding="utf-8"
    )
    (sdk / "Include" / version / "um" / "driverspecs.h").write_text("", encoding="utf-8")
    (sdk / "Lib" / version / "um" / "x86" / "WS2_32.Lib").write_text("", encoding="utf-8")

    source = tmp_path / "port" / "linux" / "include"
    source.mkdir(parents=True)
    (source / "StdDef.h").write_text("", encoding="utf-8")
    monkeypatch.setattr(cross, "BUILD", tmp_path / "build" / "windows")
    monkeypatch.setattr(cross, "LINUX_DIR", tmp_path / "port" / "linux")
    settings = SimpleNamespace(windows_vctools_dir=vc, windows_sdk_dir=sdk)

    prefix = cross.cross_toolchain_prefix(settings)
    assert prefix.startswith("env INCLUDE=")
    assert " LIB=" in prefix
    include_mirror = cross.BUILD / "case_bridge" / "include" / "2"
    library_mirror = cross.BUILD / "case_bridge" / "lib" / "2"
    assert (include_mirror / "windows.h").samefile(sdk / "Include" / version / "um" / "Windows.h")
    assert (include_mirror / "DriverSpecs.h").samefile(sdk / "Include" / version / "um" / "driverspecs.h")
    assert (library_mirror / "ws2_32.lib").samefile(sdk / "Lib" / version / "um" / "x86" / "WS2_32.Lib")
    assert (cross.BUILD / "include_case" / "StdDef.h").samefile(source / "StdDef.h")

    stamp = cross.BUILD / "case_bridge" / "include.stamp"
    first = stamp.stat().st_mtime_ns
    assert cross.cross_toolchain_prefix(settings) == prefix
    assert stamp.stat().st_mtime_ns == first

    if sys.platform == "linux":
        compiler = shutil.which("clang")
        if compiler is None:
            pytest.skip("Clang is required for the Linux-to-Windows compile probe")
        probe = tmp_path / "probe.c"
        probe.write_text("#include <windows.h>\nint cross_probe(void) { return 42; }\n", encoding="utf-8")
        output = tmp_path / "probe.obj"
        result = subprocess.run(
            [*shlex.split(prefix), compiler, "--target=i686-pc-windows-msvc", "-c", str(probe), "-o", str(output)],
            text=True,
            capture_output=True,
            check=False,
        )
        assert result.returncode == 0, result.stderr
        assert output.read_bytes()[:2] == b"\x4c\x01", "expected an x86 COFF object"


def test_cross_toolchain_requires_sdk(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(cross, "BUILD", tmp_path / "build" / "windows")
    settings = SimpleNamespace(windows_vctools_dir=tmp_path / "missing-vc", windows_sdk_dir=tmp_path / "missing-sdk")
    with pytest.raises(SystemExit, match="--windows-cross needs"):
        cross.cross_toolchain_prefix(settings)
