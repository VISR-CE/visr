"""Settings > ABOUT: its licenses are the code the Apple apps contain (NOTICE.md), each one's copyright holder is in the
license file it names, the screens in both looks are what tools/port_settings.py and tools/xbox_menus.py write, and the
help of every row fits the help's three lines."""
from pathlib import Path
import re
import sys
import textwrap
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / "port/assets/menus"
sys.path.insert(0, str(ROOT / "tools"))
import about_data  # noqa: E402
import port_settings  # noqa: E402

PC_SCREEN = MENUS / "ce/main_menu.settings_select.player_setup.player_profile_edit.about.xml"
XBOX_SCREEN = MENUS / "xbox/settings.about.xml"

# NOTICE.md's components that are in the Apple apps, by the words of the entry that covers each
COVERED = {
    "xxHash": "XXHASH", "bc7enc_rdo": "BC7ENC", "bc7enc16": "BC7 DECODER", "Arm astc-encoder": "ASTC ENCODER",
    "PBRify_Remix": "PBRIFY", "FFmpeg": "FFMPEG", "Expat": "EXPAT", "KCP": "KCP", "Monocypher": "MONOCYPHER",
    "stb_truetype": "STB_TRUETYPE", "tomlc17": "TOMLC17", "zlib": "ZLIB", "SMAA": "SMAA", "Surge AI": "STREAMER",
    "english-words": "ENGLISH", "SDL": "SDL", "musl": "MUSL", "Khronos": "KHRONOS", "Overpass": "OVERPASS",
    "OpenCE": "OPENCE", "Newtown": "NEWTOWN", "Orbitron": "ORBITRON", "Mbed TLS": "MBED",
}


def names():
    return [entry[0] for entry in about_data.LICENSES]


def test_each_license_file_holds_its_copyright_holder():
    for name, _, copyright, file, holder in about_data.LICENSES:
        if file is None:
            continue  # (downloaded at build time: tools/ios_build.py copies its notice into the app)
        text = (ROOT / file).read_text(errors="replace")
        assert holder.lower() in text.lower(), (name, file, holder)


def test_downloaded_licenses_are_copied_into_the_app():
    build = (ROOT / "tools/ios_build.py").read_text()
    assert "'SDL.txt'" in build and "'musl.txt'" in build


def test_every_component_the_apple_apps_contain_is_listed():
    notice = (ROOT / "NOTICE.md").read_text()
    rows, section = [], ""
    for line in notice.splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if line.startswith("## "):
            section = line
        elif line.startswith("|") and len(cells) == 4 and cells[3].startswith(("Yes", "Notice only")):
            rows.append(cells[0])
        elif line.startswith("|") and len(cells) == 3 and section in ("## Downloaded at build time", "## Fonts") and \
                cells[0] not in ("Component", "Font") and not cells[0].startswith("-"):
            rows.append(cells[0])
    assert rows
    for row in rows:
        key = next((key for key in COVERED if key.lower() in row.lower()), None)
        assert key, f"NOTICE.md lists {row!r}, which COVERED does not map"
        assert any(COVERED[key].lower() in name.lower() for name in names()), (row, COVERED[key])


def test_help_of_each_row_fits_three_lines():
    for _, _, copyright, *_ in about_data.LICENSES:
        assert len(textwrap.wrap(copyright, 50)) <= 3, copyright
    assert len(textwrap.wrap(about_data.CREDIT, 50)) <= 3


def test_pc_screen_is_what_the_generator_writes():
    expected = "\n".join(port_settings.settings_files()["main_menu.settings_select.player_setup.player_profile_edit.about.xml"])
    assert PC_SCREEN.read_text() == expected


def labels(path, prefix):
    root = ET.parse(path).getroot()
    lists = {s.get("name"): [x.get("text") for x in s] for s in root.iter("strings")}
    return lists[f"{prefix}/labels"], lists[f"{prefix}/help_strings"], lists[f"{prefix}/values"]


def test_both_looks_list_the_same_rows():
    pc = labels(PC_SCREEN, f"{port_settings.PE}/about")
    xbox = labels(XBOX_SCREEN, "xbox/settings/about")
    assert pc == xbox
    label, helps, values = pc
    assert label[:4] == ["VERSION", "BUILD", "WEBSITE", "CREDITS"] and label[4:] == names()
    assert len(helps) == len(label) + 1
    assert about_data.WEBSITE in values
    assert any("punpckhdq" in help_text for help_text in helps)


def test_the_profile_editor_opens_it_in_both_looks():
    pc = (MENUS / "ce/main_menu.settings_select.player_setup.player_profile_edit.xml").read_text()
    assert re.search(r'about_item".*?<on event="a" open="[^"]*/about/about_screen"/>', pc, re.S)
    patches = (MENUS / "xbox/patches.xml").read_text()
    assert 'name="profile_item_about"' in patches and 'open="xbox/settings/about/about_screen"' in patches


def test_the_version_row_has_a_game_data_function():
    for name in ("version", "build"):
        assert f'input="port about {name}"' in PC_SCREEN.read_text()
        assert f'input="port about {name}"' in XBOX_SCREEN.read_text()
    tags = (ROOT / "port/linux/game/menu_tags.c").read_text()
    functions = (ROOT / "port/linux/game/menu_functions.c").read_text()
    assert '"port about version"' in tags and '"port about build"' in tags and '"port about build"' in functions
    assert "host_sdl_app_build" in (ROOT / "port/runtime/host_imports.list").read_text()


def test_about_has_a_scroll_window_and_back():
    root = ET.parse(PC_SCREEN).getroot()
    lists = [w for w in root.iter("widget") if w.get("name", "").endswith("/options_menu")]
    assert lists[0].get("visible_rows") == "8"
    assert any(w.get("text") == "BACK" for w in root.iter("widget"))
