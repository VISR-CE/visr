"""Asset-free Xbox tag inspection and menu checks; HALO_MAPS enables the real-map check."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import zlib

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))

ROOT = Path(__file__).resolve().parents[1]
SCREEN = r"ui\test\screen"
LABEL = r"ui\test\label"
STRINGS = r"ui\test\strings"
BITMAP = r"ui\test\bitmap"
FONT = r"ui\test\font"


def synthetic_map(compressed=False):
    """Two widgets sharing a string list; all content and names are invented."""
    data = bytearray(0x2000)
    data[:4], data[0x7FC:0x800] = b"daeh", b"toof"
    struct.pack_into("<II", data, 4, 5, len(data))
    struct.pack_into("<II", data, 0x10, 0x900, 0x1700)

    def address(offset):
        return 0x803A6000 + offset - 0x900

    struct.pack_into("<IIII", data, 0x900, address(0x940), 0x12340000, 0, 5)
    names = 0xA00
    for index, (group, name, base) in enumerate([
        (b"aLeD", SCREEN, 0xC00), (b"aLeD", LABEL, 0x1000),
        (b"rtsu", STRINGS, 0x1400), (b"mtib", BITMAP, 0x1500),
        (b"tnof", FONT, 0x1600),
    ]):
        encoded = name.encode("ascii") + b"\0"
        data[names:names + len(encoded)] = encoded
        struct.pack_into("<4s4s4sIIIII", data, 0x940 + index * 32,
                         group, b"\xff" * 4, b"\xff" * 4, 0x12340000 + index,
                         address(names), address(base), 0, 0)
        names += len(encoded)

    def reference(offset, index):
        struct.pack_into("<I", data, offset + 12, 0x12340000 + index)

    for base in (0xC00, 0x1000):
        for offset in (0x38, 0xEC, 0xFC, 0x154, 0x164, 0x1A4):
            struct.pack_into("<I", data, base + offset + 12, 0xFFFFFFFF)
        struct.pack_into("<h", data, base + 0x12E, -1)
    struct.pack_into("<hh", data, 0xC00, 3, 1)
    struct.pack_into("<hhhhI", data, 0xC24, 10, -2, 200, 400, 0x21)
    struct.pack_into("<iII", data, 0xFE0, 1, address(0x1700), 0)
    reference(0x1700, 1)
    struct.pack_into("<hh", data, 0x1736, -3, 7)
    # A cycle through a conditional must not require recursive widget decoding.
    struct.pack_into("<iII", data, 0x12D4, 1, address(0x1750), 0)
    reference(0x1750, 0)
    struct.pack_into("<hh", data, 0x1000, 1, 0)
    struct.pack_into("<hhhh", data, 0x1024, 0, 1, 30, 101)
    reference(0x1038, 3)
    reference(0x10EC, 2)
    reference(0x10FC, 4)
    struct.pack_into("<h", data, 0x112E, 1)
    struct.pack_into("<iII", data, 0x1054, 1, address(0x17A0), 0)
    struct.pack_into("<Ihh", data, 0x17A0, 0x88, 0, 23)
    reference(0x17A8, 0)
    data[0x17C8:0x17CD] = b"demo\0"
    struct.pack_into("<iII", data, 0x1048, 1, address(0x1800), 0)
    struct.pack_into("<h", data, 0x1800, 42)
    struct.pack_into("<iII", data, 0x1400, 2, address(0x1840), 0)
    for index, (text, offset) in enumerate([("Unused", 0x1900), ("Test café", 0x1940)]):
        encoded = (text + "\0").encode("utf-16-le")
        data[offset:offset + len(encoded)] = encoded
        struct.pack_into("<iIIII", data, 0x1840 + index * 20,
                         len(encoded), 0, 0, address(offset), 0)
    return bytes(data[:0x800] + zlib.compress(data[0x800:])) if compressed else bytes(data)


@pytest.mark.parametrize("compressed", [False, True])
def test_cli_inventory_decodes_widgets_and_all_tag_paths(tmp_path, compressed):
    map_path = tmp_path / "ui.map"
    map_path.write_bytes(synthetic_map(compressed))
    result = subprocess.run([sys.executable, str(ROOT / "tools/ui_map_tags.py"), str(map_path)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    inventory = json.loads(result.stdout)
    assert {(tag["group"], tag["path"]) for tag in inventory["tags"]} == {
        ("DeLa", SCREEN), ("DeLa", LABEL), ("ustr", STRINGS), ("bitm", BITMAP), ("font", FONT)}
    screen, label = inventory["widgets"][SCREEN], inventory["widgets"][LABEL]
    assert screen["type"] == "column_list"
    assert screen["bounds"] == [-2, 10, 400, 200]
    assert screen["flags"] == "0x21"
    assert screen["children"] == [{"widget": LABEL, "x": 7, "y": -3}]
    assert label["bounds"] == [1, 0, 101, 30]
    assert label["bitmap"] == BITMAP
    assert label["font"] == FONT
    assert label["text_list"] == STRINGS
    assert label["string_index"] == 1
    assert label["text"] == "Test café"
    assert label["conditionals"] == [SCREEN]
    assert label["game_data_inputs"] == [42]
    assert label["handlers"] == [{"event": "a", "function": 23,
                                    "flags": ["open", "run_function"],
                                    "widget": SCREEN, "script": "demo"}]


@pytest.mark.parametrize("offset,fmt,value", [
    (4, "<I", 7), (0x10, "<I", 0xFFFF), (0x900, "<I", 0x803A5FFF),
    (0x90C, "<I", 0xFFFFFFFF), (0xFE0, "<i", -1),
    (0xFE4, "<I", 0), (0xFE4, "<I", 0x803AFFFF),
    (0x1840, "<i", -2), (0x1854, "<i", 3),
    (0x170C, "<I", 0xDEADBEEF),
])
def test_rejects_invalid_version_pointers_counts_and_strings(tmp_path, offset, fmt, value):
    import ui_map_tags
    data = bytearray(synthetic_map())
    struct.pack_into(fmt, data, offset, value)
    map_path = tmp_path / "bad.map"
    map_path.write_bytes(data)
    with pytest.raises(ValueError):
        ui_map_tags.read_map(map_path)


def menus(tmp_path, body):
    folder = tmp_path / "xbox"
    folder.mkdir(exist_ok=True)
    (folder / "test.xml").write_text(f'<menus>{body}</menus>', encoding="utf-8")
    return folder


def test_menu_checks_allow_xml_widgets_map_art_and_patch_rows(tmp_path):
    import ui_map_tags
    (tmp_path / "fallback.png").write_bytes(b"synthetic placeholder")
    folder = menus(tmp_path, f'''
        <widget name="xbox/test"><on open="{SCREEN}"/><child widget="xbox/other"/></widget>
        <widget name="xbox/other" font="{FONT}" bitmap="art"/>
        <bitmap name="art"><frame map="{BITMAP}" png="fallback.png"/></bitmap>
        <patch list="{SCREEN}" screen="{SCREEN}">
          <row after="{LABEL}" model="{LABEL}" name="extra" open="xbox/test"/>
          <row after="extra" name="last" open="xbox/other"/>
        </patch>''')
    map_path = tmp_path / "ui.map"
    map_path.write_bytes(synthetic_map())
    inventory = ui_map_tags.read_map(map_path)
    assert ui_map_tags.check_menus(folder, tmp_path, inventory) == []


@pytest.mark.parametrize("body,problem", [
    (f'<bitmap name="art"><frame map="{BITMAP}"/></bitmap>', "png"),
    (f'<bitmap name="art"><frame map="{BITMAP}" png="absent.png"/></bitmap>', "absent.png"),
    (f'<widget name="xbox/test"><child widget="{LABEL}"/></widget>', "map widget"),
    (f'<widget name="xbox/test" description="{LABEL}"/>', "map widget"),
    ('<widget name="xbox/test"><on open="xbox/missing"/></widget>', "xbox/missing"),
    ('<patch list="ui\\missing"><row model="ui\\missing" name="new"/></patch>', "ui\\missing"),
    (f'<widget name="xbox/test" bitmap="{FONT}"/>', "bitm"),
    (f'<widget name="xbox/test"><on open="{BITMAP}"/></widget>', "DeLa"),
    ('<widget name="xbox/test"/><patch list="xbox/test"/>', "map tag"),
    (f'<widget name="{LABEL}"/>', "map widget"),
    ('<bitmap name="art"><frame map="art" png="missing.png"/></bitmap>', "bitm map tag"),
])
def test_menu_checks_report_missing_or_wrong_kind_references(tmp_path, body, problem):
    import ui_map_tags
    map_path = tmp_path / "ui.map"
    map_path.write_bytes(synthetic_map())
    errors = ui_map_tags.check_menus(menus(tmp_path, body), tmp_path, ui_map_tags.read_map(map_path))
    assert any(problem in error for error in errors), errors


def test_fallback_checks_run_without_game_data(tmp_path):
    import ui_map_tags
    errors = ui_map_tags.check_menus(menus(tmp_path,
        f'<bitmap name="art"><frame map="{BITMAP}"/></bitmap>'), tmp_path)
    assert any("png" in error for error in errors)


def test_repository_xbox_menu_fallbacks():
    import ui_map_tags
    folder = ROOT / "port/assets/menus/xbox"
    if not folder.is_dir():
        pytest.skip("Xbox menu files have not landed yet")
    assert ui_map_tags.check_menus(folder, folder.parent) == []


def test_real_ui_map_when_available():
    import ui_map_tags
    maps = os.environ.get("HALO_MAPS")
    if not maps:
        pytest.skip("set HALO_MAPS to a folder containing an Xbox ui.map")
    path = Path(maps) / "ui.map"
    inventory = ui_map_tags.read_map(path)
    assert inventory["widgets"]
    assert r"ui\shell\main_menu\main_menu" in inventory["widgets"]
    folder = ROOT / "port/assets/menus/xbox"
    if folder.is_dir():
        assert ui_map_tags.check_menus(folder, folder.parent, inventory) == []


def test_cli_output_file_and_errors(tmp_path):
    tool = [sys.executable, str(ROOT / "tools/ui_map_tags.py")]
    map_path, output = tmp_path / "ui.map", tmp_path / "tags.json"
    map_path.write_bytes(synthetic_map(True))
    result = subprocess.run([*tool, str(map_path), "--output", str(output)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout == ""
    assert json.loads(output.read_text(encoding="utf-8"))["widgets"][LABEL]["text"] == "Test café"
    map_path.write_bytes(b"bad cache")
    result = subprocess.run([*tool, str(map_path)], capture_output=True, text=True)
    assert result.returncode == 1
    assert "Xbox cache header" in result.stderr
    assert "Traceback" not in result.stderr


def test_cli_menu_check_without_map_reports_bad_fallback(tmp_path):
    folder = menus(tmp_path, f'<bitmap name="art"><frame map="{BITMAP}"/></bitmap>')
    result = subprocess.run([sys.executable, str(ROOT / "tools/ui_map_tags.py"),
                             "--check-menus", str(folder)], capture_output=True, text=True)
    assert result.returncode == 1
    assert "png fallback" in result.stderr
    (tmp_path / "fallback.png").write_bytes(b"synthetic placeholder")
    menus(tmp_path, f'<bitmap name="art"><frame map="{BITMAP}" png="fallback.png"/></bitmap>')
    result = subprocess.run([sys.executable, str(ROOT / "tools/ui_map_tags.py"),
                             "--check-menus", str(folder)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize("data", [
    b"", synthetic_map()[:0x800], synthetic_map(True)[:-3],
    synthetic_map()[:0x800] + b"invalid zlib stream",
])
def test_rejects_truncated_or_corrupt_compressed_cache(tmp_path, data):
    import ui_map_tags
    map_path = tmp_path / "bad.map"
    map_path.write_bytes(data)
    with pytest.raises(ValueError):
        ui_map_tags.read_map(map_path)
