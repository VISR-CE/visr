"""The shipped menus load through the real menu builder, for every platform set.

Bug class: a menu file the builder refuses ("menus: not added; using the game's own menus") takes every VISR menu with it,
and a rule the builder has that Python only mirrors (a widget attribute allowed on one type, a name that must resolve) is
not tested by the mirror. port/ios/tests/menu_load_probe.c includes port/linux/game/menu_tags.c and menu_files.c, embeds
the shipped port/assets/menus/xbox/*.xml (patches.xml left out: tools/test_shipped_menu_patches.py resolves it against
the disc's tags) and loads them against a ui.map of the tags those files name, once for each platform set.
"""
from pathlib import Path
import re
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / "port/assets/menus"
EXPAT = ROOT / "port/third_party/expat"

# (the HALO_HOST_* variables menu_platform.h reads: an iPhone, an iPad with a keyboard, the Apple TV, the Mac app, and
# every capability at once)
PLATFORM_SETS = {
    "iphone": {"HALO_HOST_KEYBOARD": "0", "HALO_HOST_TVOS": "0", "HALO_HOST_MAC": "0",
               "HALO_HOST_CLOUD_SAVES": "0", "HALO_HOST_SEND_LOG": "0"},
    "iphone_cloud": {"HALO_HOST_KEYBOARD": "0", "HALO_HOST_TVOS": "0", "HALO_HOST_MAC": "0",
                     "HALO_HOST_CLOUD_SAVES": "1", "HALO_HOST_SEND_LOG": "1"},
    "ipad_keyboard": {"HALO_HOST_KEYBOARD": "1", "HALO_HOST_TVOS": "0", "HALO_HOST_MAC": "0",
                      "HALO_HOST_CLOUD_SAVES": "1", "HALO_HOST_SEND_LOG": "1"},
    "apple_tv": {"HALO_HOST_KEYBOARD": "0", "HALO_HOST_TVOS": "1", "HALO_HOST_MAC": "0",
                 "HALO_HOST_CLOUD_SAVES": "0", "HALO_HOST_SEND_LOG": "0"},
    "mac": {"HALO_HOST_KEYBOARD": "1", "HALO_HOST_TVOS": "0", "HALO_HOST_MAC": "1",
            "HALO_HOST_CLOUD_SAVES": "1", "HALO_HOST_SEND_LOG": "1"},
}
# the attributes whose value may name a tag of the map (a path with backslashes)
TAG_ATTRIBUTES = ("font", "map", "widget", "open", "child", "sound", "list", "screen", "after", "before", "model")


def tag_names() -> list:
    names = set()
    for path in (MENUS / "xbox").glob("*.xml"):
        for attribute, value in re.findall(r'\b(\w+)="([^"]*\\[^"]*)"', path.read_text(encoding="utf-8")):
            if attribute in TAG_ATTRIBUTES and re.fullmatch(r"[\w ]+(\\[\w ]+)+", value):
                names.add(value)
    return sorted(names)


def function_names() -> list:
    """the engine's event handler functions (ce_menus.py reads them the same way)"""
    text = (ROOT / "source/interface/ui_widget_event_handler_functions.c").read_text(encoding="latin-1")
    text = text[text.index('\t{\n\t\t"NULL",'):]
    return re.findall(r'"([^"\\]*)"', text[:text.index("}")])[:102]


def embedded_source(directory: Path, added: dict) -> Path:
    lines = ['#include "menu_files.h"']
    entries = []
    files = [(f"xbox/{path.name}", path.read_bytes()) for path in sorted((MENUS / "xbox").glob("*.xml"))
             if path.name != "patches.xml"] + sorted(added.items())
    for index, (name, data) in enumerate(files):
        lines.append(f"static const unsigned char file_{index}[] __attribute__((aligned(8))) = {{"
                     + ",".join(str(byte) for byte in data) + ",0};")
        entries.append(f'\t{{ "{name}", (const unsigned int *)file_{index}, {len(data)} }},')
    lines.append("const struct menu_file_embedded menu_files_embedded[] =\n{\n" + "\n".join(entries) + "\n};")
    lines.append(f"const unsigned int menu_files_embedded_count = {len(entries)};")
    target = directory / "embedded.c"
    target.write_text("\n".join(lines) + "\n")
    return target


def build(directory: Path, added: dict = None) -> Path:
    shutil.copytree(ROOT / "port/ios/tests/menu_patch", directory, dirs_exist_ok=True)
    for source in ("port/linux/game/menu_tags.c", "port/linux/src/menu_files.c", "port/linux/src/menu_files.h",
                   "port/linux/src/menu_platform.h", "port/linux/include/halo_menus.h",
                   "port/linux/include/menu_equivalence.h", "port/linux/include/menu_geometry.h",
                   "port/linux/include/halo_menu_meta.h", "port/ios/tests/menu_load_probe.c"):
        shutil.copy2(ROOT / source, directory)
    embedded = embedded_source(directory, added or {})
    binary = directory / "menu_load_probe"
    subprocess.run(["xcrun", "clang", "-std=gnu11", "-g", "-Wall", "-Werror", "-Wno-multichar",
                    "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-but-set-variable",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                    "-DHALO_ILP32=1", "-DHALO_IOS=1", "-DMENU_TAGS_PROBE=1",
                    f"--include-directory={directory}", f"--include-directory={EXPAT}",
                    f"--include-directory={ROOT / 'port/linux/game'}",
                    str(directory / "menu_load_probe.c"), str(directory / "menu_files.c"), str(embedded),
                    *(str(EXPAT / name) for name in ("xmlparse.c", "xmlrole.c", "xmltok.c")),
                    f"--output={binary}"], check=True)
    return binary


def run(binary: Path, directory: Path, environment: dict):
    (directory / "empty").mkdir(exist_ok=True)
    (directory / "tags.txt").write_text("\n".join(tag_names()) + "\n")
    (directory / "functions.txt").write_text("\n".join(function_names()) + "\n")
    return subprocess.run([str(binary), str(directory / "empty"), str(directory / "tags.txt"),
                           str(directory / "functions.txt")], capture_output=True, text=True,
                          env={"PATH": "/usr/bin:/bin", **environment})


@pytest.mark.parametrize("platform_set", sorted(PLATFORM_SETS))
def test_shipped_menus_load_for_every_platform_set(tmp_path, platform_set):
    binary = build(tmp_path)
    result = run(binary, tmp_path, PLATFORM_SETS[platform_set])
    problems = [line for line in result.stdout.splitlines() if "menus:" in line and "not added" not in line
                and ("error" in line or "nothing named" in line or "strings are for" in line or ": the map has no" in line)]
    assert result.returncode == 0, (platform_set, "\n".join(problems[:20]), result.stdout[-1500:], result.stderr[-1500:])
    assert "menus: not added" not in result.stdout


def test_the_probe_fails_on_a_menu_the_builder_refuses(tmp_path):
    """The probe is no rubber stamp: a widget the builder refuses (strings on a container) fails the load."""
    refused = b'<menus><widget name="xbox/refused/widget" type="container" strings="A|B"/></menus>'
    binary = build(tmp_path, {"xbox/refused.xml": refused})
    result = run(binary, tmp_path, PLATFORM_SETS["iphone"])
    assert result.returncode == 1 and "strings are for" in result.stdout, result.stdout[-800:]
