"""A frame visionOS's Compositor paces counts as a benchmark frame whichever present path showed it.

Class: a measurement fed by one presentation path while a platform presents through another. The device benchmark
counted only the flat screen's presents, so every run in the immersive space ended with 0 frames. The probe runs
the arithmetic the Compositor present paths share; the source checks hold every such path in gpu_metal.m to the
measurement call (the theater code itself cannot run here)."""
from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / "port/ios/host"
# every function that hands a picture to the Compositor
PRESENTS = {"host_stereo_present", "host_stereo_present_ui", "host_theater_present", "host_theater_present_eyes"}


def test_probe(tmp_path):
    compiler = shutil.which("cc") or shutil.which("clang")
    if not compiler:
        pytest.skip("no C compiler")
    program = tmp_path / "probe"
    subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(HOST), "-I", str(ROOT / "port/runtime"),
                    str(ROOT / "port/ios/tests/stereo_benchmark_frames_probe.c"),
                    str(ROOT / "port/runtime/benchmark_metrics.c"), "-lm", "-o", str(program)], check=True)
    result = subprocess.run([str(program)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def code_lines(path):
    """the file's lines with block comments blanked, so a name in a comment is not a call"""
    text = re.sub(r"/\*.*?\*/", lambda found: re.sub(r"[^\n]", " ", found.group()), path.read_text(), flags=re.S)
    return text.splitlines()


def present_branches(lines):
    """(name, line number, the branch's lines) for each Compositor present call: from the branch's commit to its return"""
    call = re.compile(r"\b(%s)\(" % "|".join(sorted(PRESENTS, key=len, reverse=True)))
    for index, line in enumerate(lines):
        found = call.search(line)
        if not found:
            continue
        start = max(at for at in range(index + 1) if "commit(YES)" in lines[at])
        end = min(at for at in range(index, len(lines)) if re.search(r"\breturn 0;", lines[at]))
        yield found.group(1), index + 1, lines[start:end + 1]


def test_every_compositor_present_is_measured():
    branches = list(present_branches(code_lines(HOST / "gpu_metal.m")))
    assert {name for name, _, _ in branches} == PRESENTS
    # the eyes, the eyes on the screen, the theater's mono picture, and the UI's quad from both present entries
    assert len(branches) == 5
    for name, number, branch in branches:
        counted = [line for line in branch if "stereo_pacing_count(" in line]
        assert len(counted) == 1, f"gpu_metal.m:{number}: {name} is shown and its frame is measured {len(counted)} times"


def test_no_other_compositor_present():
    """a new present function in the theater's or stereo's header must join PRESENTS, and so the check above"""
    declared = set()
    for header in ("host_stereo.h", "host_theater.h"):
        declared |= set(re.findall(r"\b(host_(?:stereo|theater)_present(?:_\w+)?)\(", "\n".join(code_lines(HOST / header))))
    assert declared == PRESENTS


def test_measurement_feeds_the_benchmark():
    lines = code_lines(HOST / "gpu_metal.m")
    start = next(at for at, line in enumerate(lines) if line.startswith("static void stereo_pacing_count(int view") and not line.endswith(";"))
    end = next(at for at in range(start, len(lines)) if lines[at] == "}")
    body = "\n".join(lines[start:end])
    assert "host_stereo_measure(" in body and "host_frame_cpu_split(" in body and "host_theater_take_waited()" in body
    assert body.count("host_benchmark_frame(") == 1 and "host_benchmark_upscaler(" in body
    # host_benchmark_frame feeds the overlay itself: a run frame must not reach it twice
    assert body.count("host_frame_counter_sample(") == 1
    assert re.search(r"else\s+host_frame_counter_sample\(", body)
