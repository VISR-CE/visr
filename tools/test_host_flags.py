"""The Apple host's flags to the game go through port/linux/src/halo_host_flags.h.

Bug class: the game asked whether it runs on Apple Vision Pro in seven places, each with its own
getenv("HALO_HOST_VISIONOS") and its own test of the value ("1" in most, atoi in one), so a change to
how the host says so had seven places to miss. halo_host_visionos() is the one answer; this test
fails on any other read of the flag in the game's sources. The host (port/ios/host) sets it, and
the probes in port/ios/tests set it to exercise both answers.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / 'port/linux/src/halo_host_flags.h'
SOURCES = ('*.c', '*.h', '*.cpp', '*.m', '*.mm')
SETS = re.compile(r'\b(?:setenv|unsetenv)\s*\(\s*"HALO_HOST_VISIONOS"')


def flag_reads(text):
    """Lines that name the flag other than to set it."""
    return [number for number, line in enumerate(text.splitlines(), 1)
            if '"HALO_HOST_VISIONOS"' in line and not SETS.search(line)]


def test_the_check_finds_a_read():
    assert flag_reads('\tconst char *vision = getenv("HALO_HOST_VISIONOS");\n') == [1]
    assert flag_reads('\tmenu_input_env("HALO_HOST_VISIONOS");\n') == [1]
    assert flag_reads('\tsetenv("HALO_HOST_VISIONOS", "1", 1);\n') == []


def test_the_game_reads_the_vision_pro_flag_only_through_the_helper():
    listed = subprocess.run(['git', 'ls-files', '--', *SOURCES], cwd=ROOT, capture_output=True, text=True,
                            check=True).stdout.split()
    found = []
    for name in listed:
        path = ROOT / name
        parts = Path(name).parts
        if path == HELPER or parts[:3] in (('port', 'ios', 'tests'), ('port', 'ios', 'host')) or not path.is_file():
            continue
        found += [f'{name}:{number}' for number in flag_reads(path.read_text(errors='replace'))]
    assert listed and not found, 'read HALO_HOST_VISIONOS through halo_host_visionos() (halo_host_flags.h): ' + ', '.join(found)


def test_the_helper_names_the_flag():
    assert 'halo_host_flag("HALO_HOST_VISIONOS")' in HELPER.read_text()
