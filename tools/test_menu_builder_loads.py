"""The shipped menus load through the real menu builder, for every platform set.

Bug class: a menu file the builder refuses ("menus: not added; using the game's own menus") takes every VISR menu with it,
and a rule the builder has that Python only mirrors (a widget attribute allowed on one type, a name that must resolve) is
not tested by the mirror. port/ios/tests/menu_load_probe.c includes port/linux/game/menu_tags.c and menu_files.c, embeds
the shipped port/assets/menus/xbox/*.xml (patches.xml left out: tools/test_shipped_menu_patches.py resolves it against
the disc's tags) and loads them against a ui.map that has the disc's tags, no others (tools/fixtures/disc_menu_layout.json:
its widgets and the bitmaps, string lists and fonts they name, each in its own group), once for each platform set.
"""
from pathlib import Path
import json
import re
import shutil
import subprocess
import sys
import tempfile

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


LAYOUT = ROOT / "tools/fixtures/disc_menu_layout.json"
WIDGET_TYPES = {"container": 0, "text_box": 1, "spinner_list": 2, "column_list": 3}
# (the sounds the shipped menus name and the disc's widgets do not: handlers carry sounds, which the layout manifest
# leaves out)
SOUNDS = ("sound\\sfx\\ui\\back", "sound\\sfx\\ui\\forward")


def xml_names() -> set:
    names = set()
    for path in (MENUS / "xbox").glob("*.xml"):
        if path.name == "patches.xml":
            continue  # (resolved against the disc's tags by tools/test_shipped_menu_patches.py)
        for attribute, value in re.findall(r'\b(\w+)="([^"]*\\[^"]*)"', path.read_text(encoding="utf-8")):
            if attribute in TAG_ATTRIBUTES and re.fullmatch(r"[\w ]+(\\[\w ]+)+", value):
                names.add(value)
    return names


def tag_lines() -> list:
    """ui.map's tags as the disc has them (the layout manifest's widgets, and the bitmaps, string lists and fonts they
    name), each in its own group, so a path the menus misspell is a tag the map does not have"""
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    lines = {f"W {WIDGET_TYPES[widget['type']]} {name}" for name, widget in layout.items()}
    for widget in layout.values():
        for key, group in (("bitmap", "B"), ("header_bitmap", "B"), ("footer_bitmap", "B"), ("text_list", "S"),
                           ("font", "F")):
            if widget[key]:
                lines.add(f"{group} {widget[key]}")
    lines.update(f"N {name}" for name in SOUNDS)
    known = {line.split(" ", 2)[-1] for line in lines}
    unknown = sorted(xml_names() - known)
    assert not unknown, ("the shipped menus name tags the disc's layout does not have", unknown[:10])
    return sorted(lines)


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
    (directory / "tags.txt").write_text("\n".join(tag_lines()) + "\n")
    (directory / "functions.txt").write_text("\n".join(function_names()) + "\n")
    return subprocess.run([str(binary), str(directory / "empty"), str(directory / "tags.txt"),
                           str(directory / "functions.txt")], capture_output=True, text=True,
                          env={"PATH": "/usr/bin:/bin", **environment})


def check_platform_set(directory: Path, platform_set: str) -> None:
    binary = build(directory)
    result = run(binary, directory, PLATFORM_SETS[platform_set])
    problems = [line for line in result.stdout.splitlines() if line.startswith("log: menus:")]
    assert result.returncode == 0 and "menus: not added" not in result.stdout, (
        platform_set, "\n".join(problems[:20]), result.stdout[-1500:], result.stderr[-1500:])


def check_probe_fails_on_a_refused_menu(directory: Path) -> None:
    """The probe is no rubber stamp: a widget the builder refuses (strings on a container) fails the load."""
    refused = b'<menus><widget name="xbox/refused/widget" type="container" strings="A|B"/></menus>'
    binary = build(directory, {"xbox/refused.xml": refused})
    result = run(binary, directory, PLATFORM_SETS["iphone"])
    assert result.returncode == 1 and "strings are for" in result.stdout, result.stdout[-800:]


def check_probe_fails_on_a_misspelled_path(directory: Path) -> None:
    """The map has only the disc's tags: a path the menus misspell (a widget, a font) is refused, as in the game."""
    misspelled = (b'<menus><widget name="xbox/misspelled/widget" type="text" font="ui\\shell\\no_such_font">'
                  b'<child widget="ui\\shell\\error\\no_such_widget"/></widget></menus>')
    binary = build(directory, {"xbox/misspelled.xml": misspelled})
    result = run(binary, directory, PLATFORM_SETS["iphone"])
    assert result.returncode == 1 and "no_such" in result.stdout, result.stdout[-800:]


def test_the_probe_fails_on_a_path_the_map_does_not_have(tmp_path):
    check_probe_fails_on_a_misspelled_path(tmp_path / "misspelled")


def test_shipped_menus_load_for_every_platform_set(tmp_path):
    for platform_set in sorted(PLATFORM_SETS):
        check_platform_set(tmp_path / platform_set, platform_set)


def test_the_probe_fails_on_a_menu_the_builder_refuses(tmp_path):
    check_probe_fails_on_a_refused_menu(tmp_path / "refused")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as folder:
        for name in sorted(PLATFORM_SETS):
            check_platform_set(Path(folder) / name, name)
            print(f"menus load: {name}")
        check_probe_fails_on_a_refused_menu(Path(folder) / "refused")
        print("the probe fails on a refused menu")
        check_probe_fails_on_a_misspelled_path(Path(folder) / "misspelled")
        print("the probe fails on a path the map does not have")
    sys.exit(0)
