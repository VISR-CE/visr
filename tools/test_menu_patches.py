"""The menus' patch table (port/linux/game/menu_tags.c, read by port/linux/src/menu_files.c).

Builds the real reader and tag builder for this Mac, with the game's and the platform layer's headers stood in for
by port/ios/tests/menu_patch, and runs port/ios/tests/menu_patch_probe.c: a small ui.map with the Xbox's
multiplayer type, profile edit and main menu screens, patched as the Xbox look patches them. It checks the rows'
places (the pitch from the rows' own bounds), which screen children moved, the copies' names, handlers and texts,
the descriptions shown by a patched list's rows, the descriptions' strings and frames, the tag budget and the
trimmed table, and that patches that do not fit or do not resolve are not made. Run as the Apple guest with a
keyboard, as the Apple TV, and with a malformed patch.
"""
from pathlib import Path
import shutil
import subprocess
import sys

# the probes build with the Apple toolchain (xcrun clang), as tools/ios_test.py runs them; tools/ios_test.py runs
# this file as a script, where pytest may not be installed
try:
    import pytest
except ImportError:
    pytest = None
else:
    pytestmark = pytest.mark.skipif(sys.platform != "darwin" or not shutil.which("xcrun"),
                                    reason="needs the Apple toolchain (xcrun)")

ROOT = Path(__file__).resolve().parents[1]
EXPAT = ROOT / "port/third_party/expat"


def build(directory: Path) -> Path:
    """The probe, built in directory (the sources copied beside the stand-in headers, which their own
    directories' headers would otherwise win over)."""
    shutil.copytree(ROOT / "port/ios/tests/menu_patch", directory, dirs_exist_ok=True)
    for source in ("port/linux/game/menu_tags.c", "port/linux/src/menu_files.c", "port/linux/src/menu_files.h",
                   "port/linux/src/menu_platform.h", "port/linux/include/halo_menus.h",
                   "port/linux/include/menu_equivalence.h",
                   "port/ios/tests/menu_patch_probe.c"):
        shutil.copy2(ROOT / source, directory)
    binary = directory / "menu_patch_probe"
    subprocess.run(["xcrun", "clang", "-std=gnu11", "-g", "-Wall", "-Werror", "-Wno-multichar",
                    "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-but-set-variable",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                    "-DHALO_ILP32=1", "-DHALO_IOS=1", "-DMENU_TAGS_PROBE=1",
                    f"--include-directory={directory}", f"--include-directory={EXPAT}",
                    str(directory / "menu_patch_probe.c"), str(directory / "menu_files.c"),
                    *(str(EXPAT / name) for name in ("xmlparse.c", "xmlrole.c", "xmltok.c")),
                    f"--output={binary}"], check=True)
    return binary


def check_saved_picture(path: Path) -> None:
    """The PNG the probe's extraction wrote (halo_menus_cache_write_png): 300 by 300 RGBA that Python's zlib reads,
    in more than one stored block, with no partial file left beside it."""
    import struct
    import zlib

    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    chunks, offset = {}, 8
    while offset < len(data):
        length, kind = struct.unpack(">I4s", data[offset:offset + 8])
        body = data[offset + 8:offset + 8 + length]
        assert struct.unpack(">I", data[offset + 8 + length:offset + 12 + length])[0] == zlib.crc32(kind + body)
        chunks[kind] = body
        offset += 12 + length
    assert struct.unpack(">IIBB", chunks[b"IHDR"][:10]) == (300, 300, 8, 6)
    raw = zlib.decompress(chunks[b"IDAT"])
    assert len(raw) == 300 * (300 * 4 + 1) and len(chunks[b"IDAT"]) > 65535
    for y in (0, 1, 150, 299):
        row = raw[y * 1201:(y + 1) * 1201]
        assert row[0] == 0
        for x in (0, 7, 299):
            assert tuple(row[1 + x * 4:5 + x * 4]) == (x % 256, y % 256, (x ^ y) % 256, 255)
    assert not list(path.parent.glob("*.part"))


def test_menu_patches(tmp_path):
    binary = build(tmp_path)
    (tmp_path / "menus/xbox").mkdir(parents=True)
    for mode in ("apple", "tvos", "bad"):
        result = subprocess.run([str(binary), mode, str(tmp_path)], capture_output=True, text=True)
        print(result.stdout + result.stderr)
        assert result.returncode == 0, f"menu patch probe failed ({mode})"
        if mode != "bad":
            check_saved_picture(tmp_path / "menus/cache/xbox/v1/extracted.png")
            # the picture that was saved from the original is saved again, over it
            check_saved_picture(tmp_path / "menus/cache/xbox/v1/upgrade.png")


if __name__ == "__main__":
    import tempfile

    with tempfile.TemporaryDirectory() as directory:
        test_menu_patches(Path(directory))
