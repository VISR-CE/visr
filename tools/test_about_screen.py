"""Settings > ABOUT: its licenses are the code the Apple apps contain (NOTICE.md), each one's copyright holder is in the
license file it names, the screens in both looks are what tools/port_settings.py and tools/xbox_menus.py write, and the
help of every row fits the help's three lines."""
from pathlib import Path
import re
import shutil
import sys
import textwrap
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / "port/assets/menus"
sys.path.insert(0, str(ROOT / "tools"))
import about_data  # noqa: E402
import menu_catalog  # noqa: E402
import port_settings  # noqa: E402
import notices_text  # noqa: E402
import xbox_menus  # noqa: E402

PC_SCREEN = MENUS / "ce/main_menu.settings_select.player_setup.player_profile_edit.about.xml"
XBOX_SCREEN = MENUS / "xbox/settings.about.xml"

# NOTICE.md's components that are in the Apple apps, by the words of the entry that covers each
COVERED = {
    "xxHash": "XXHASH", "bc7enc_rdo": "BC7ENC", "bc7enc16": "BC7 DECODER", "Arm astc-encoder": "ASTC ENCODER",
    "PBRify_Remix": "PBRIFY", "FFmpeg": "FFMPEG", "Expat": "EXPAT", "KCP": "KCP", "Monocypher": "MONOCYPHER",
    "stb_truetype": "STB_TRUETYPE", "stb_vorbis": "STB_VORBIS", "tomlc17": "TOMLC17", "zlib": "ZLIB", "SMAA": "SMAA", "Surge AI": "STREAMER",
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


def test_notice_is_exact_fits_and_has_the_copyright_sign():
    lines = port_settings.about_notice(about_data.NOTICE).split("\\n")
    assert len(lines) <= port_settings.NOTICE_LINES
    assert " ".join(lines).replace("  ", " ") == about_data.NOTICE and "\u00a9" in about_data.NOTICE
    assert about_data.NOTICE.startswith("Halo, Halo: Combat Evolved, Master Chief")


def test_pc_screen_is_what_the_generator_writes():
    expected = "\n".join(port_settings.settings_files()["main_menu.settings_select.player_setup.player_profile_edit.about.xml"])
    assert menu_catalog.STRIP.sub("", PC_SCREEN.read_text()) == expected


def labels(path, prefix):
    root = ET.parse(path).getroot()
    lists = {s.get("name"): [x.get("text") for x in s] for s in root.iter("strings")}
    return lists[f"{prefix}/labels"], lists[f"{prefix}/values"]


def test_both_looks_list_the_same_rows():
    pc = labels(PC_SCREEN, f"{port_settings.PE}/about")
    xbox = labels(XBOX_SCREEN, "xbox/settings/about")
    assert pc == xbox
    label, values = pc
    assert label == ["BUILD", "ONLINE", "PRIVACY", "LICENSE", "OPEN SOURCE NOTICES", "SEND LOG"]
    assert "visr.gg" in values and about_data.PRIVACY in values and about_data.LICENSE in values
    for path in (PC_SCREEN, XBOX_SCREEN):
        text = path.read_text()
        assert "CREDITS" not in text and "op_credits" not in text and "op_sdl" not in text
        assert 'type="text"' in text and about_data.NOTICE.split(",")[0] in text and "\u00a9" in text
        for tag in ("website", "privacy", "license"):
            assert text.count(f'run="port open {tag}" platform="!tvos"') == 2


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


def test_about_has_title_lines_and_back():
    root = ET.parse(PC_SCREEN).getroot()
    assert not any(w.get("name", "").endswith("/title_port") for w in root.iter("widget"))
    assert [d.get("input") for w in root.iter("widget") if w.get("name", "").endswith("/title_version") for d in w.iter("data")] == ["port about version"]
    assert any(w.get("text") == "BACK" for w in root.iter("widget"))


def rows_by_platform(path, prefix):
    """the rows each build shows, by their y: ('send_log' builds, other builds)"""
    root = ET.parse(path).getroot()
    options = next(w for w in root.iter("widget") if w.get("name", "").endswith("/options_menu"))
    shown = {"on": [], "off": []}
    for child in options.findall("child"):
        widget = child.get("widget", "")
        if "/op_" not in widget:
            continue
        platform = child.get("platform")
        for build, matches in (("on", platform in (None, "send_log")), ("off", platform in (None, "!send_log"))):
            if matches:
                shown[build].append((int(child.get("y")), widget.rsplit("/op_", 1)[1]))
    return shown


def test_send_log_row_is_there_only_in_builds_that_can_send_a_log():
    for path, prefix in ((PC_SCREEN, f"{port_settings.PE}/about"), (XBOX_SCREEN, "xbox/settings/about")):
        shown = rows_by_platform(path, prefix)
        on, off = shown["on"], shown["off"]
        assert [name for _, name in on] == ["build", "website", "privacy", "license", "notices", "send_log"]
        assert "send_log" not in [name for _, name in off]
        assert [name for _, name in on if name != "send_log"] == [name for _, name in off]
        assert [name for _, name in on][-1] == "send_log"
        # each build's rows are a row apart with no gap where the row is missing (the Xbox look at its row pitch)
        pitch = xbox_menus.ROW_PITCH if path == XBOX_SCREEN else 30
        for rows in (on, off):
            assert [y for y, _ in rows] == [port_settings.ABOUT_ROW_TOP + pitch * index for index in range(len(rows))]
    text = PC_SCREEN.read_text()
    # pressing it asks first (the Xbox look's confirmation, which both looks open): SEND runs it and goes back
    assert text.count('open="xbox/settings/send_log_confirm"') == 2 and 'input="port send log status"' in text
    assert 'run="port send log"' not in text
    xbox = XBOX_SCREEN.read_text()
    assert xbox.count('run="port send log" back="true"') == 2 and 'text="SEND LOG?"' in xbox
    assert "Your log goes to the\\nVISR team via iCloud." in xbox


def test_send_log_is_wired_through_the_game_and_the_host():
    functions = (ROOT / "port/linux/game/menu_functions.c").read_text()
    tags = (ROOT / "port/linux/game/menu_tags.c").read_text()
    assert '"port send log"' in tags and '"port send log status"' in tags
    assert '"port send log"' in functions and '"port send log status"' in functions
    imports = (ROOT / "port/runtime/host_imports.list").read_text().split()
    assert "host_sdl_send_log_start" in imports and "host_sdl_send_log_status" in imports
    platform = (ROOT / "port/linux/src/menu_platform.h").read_text()
    assert "HALO_HOST_SEND_LOG" in platform
    assert "HALO_HOST_SEND_LOG" in (ROOT / "port/ios/host/host_main.m").read_text()
    # SEND LOG remains restricted to builds that support uploads.
    assert 'platform="send_log"' in XBOX_SCREEN.read_text()


def test_website_row_opens_the_site_except_on_the_apple_tv():
    for path in (PC_SCREEN, XBOX_SCREEN):
        text = path.read_text()
        assert text.count('run="port open website" platform="!tvos"') == 2, path
    tags = (ROOT / "port/linux/game/menu_tags.c").read_text()
    assert '"port open website"' in tags and '"port open website"' in (ROOT / "port/linux/game/menu_functions.c").read_text()
    assert "host_sdl_open_website" in (ROOT / "port/runtime/host_imports.list").read_text().split()
    host = (ROOT / "port/ios/host/host_open_website.m").read_text()
    assert about_data.WEBSITE_URL in host.replace('@"', '"') and "NSWorkspace" in host and "openURL" in host


def test_notices_text_is_current_and_contains_full_licenses():
    text = notices_text.OUTPUT.read_text(encoding="utf-8")
    assert text == notices_text.generate()
    for name, license, copyright, file, holder in about_data.LICENSES:
        assert f"{name}\n{license}\n{copyright}\n\n" in text
        source = (ROOT / file).read_text(encoding="utf-8").strip()
        if name == "STREAMER WORDS":
            source = source.split("```\n", 1)[1].split("```", 1)[0].strip()
        assert source in text, name
        assert holder.lower() in text.lower()
    assert "STB_VORBIS" in names()


def test_notices_and_vorbis_are_copied_into_the_bundle():
    build = (ROOT / "tools/ios_build.py").read_text()
    assert "('port/ios/Licenses/NOTICES.txt', 'NOTICES.txt')" in build
    assert "('port/third_party/stb_vorbis/LICENSE', 'stb_vorbis.txt')" in build
    cmake = (ROOT / "port/ios/CMakeLists.txt").read_text()
    assert 'file(GLOB HALO_LICENSES "${ROOT}/build/ios/licenses/*.txt")' in cmake
    assert 'MACOSX_PACKAGE_LOCATION Resources/Licenses' in cmake


def test_notices_screen_rows_scroll_help_and_back():
    root = ET.parse(XBOX_SCREEN).getroot()
    widgets = {w.get("name"): w for w in root.iter("widget")}
    base = "xbox/settings/about/notices"
    screen = widgets[port_settings.NOTICES_SCREEN]
    for event in ("b", "back"):
        assert screen.find(f'on[@event="{event}"]').get("back") == "true"
    for path, prefix in ((PC_SCREEN, f"{port_settings.PE}/about"), (XBOX_SCREEN, "xbox/settings/about")):
        row = next(w for w in ET.parse(path).iter("widget") if w.get("name") == prefix + "/op_notices")
        for event in ("a", "start"):
            assert row.find(f'on[@event="{event}"]').get("open") == port_settings.NOTICES_SCREEN
    options = widgets[base + "/options_menu"]
    assert options.get("type") == "column_list" and options.get("visible_rows") == "7"
    assert options.get("description") == base + "/help"
    assert options.find('data[@input="port settings help"]') is not None
    rows = [c for c in options.findall("child") if "/op_" in c.get("widget")]
    assert len(rows) == len(about_data.LICENSES)
    assert [int(c.get("y")) for c in rows] == [73 + 39 * i for i in range(len(rows))]
    assert labels(XBOX_SCREEN, base) == ([e[0] for e in about_data.LICENSES], [e[1] for e in about_data.LICENSES])
    strings = {s.get("name"): [x.get("text") for x in s] for s in root.iter("strings")}
    for index, row in enumerate(rows):
        item = widgets[row.get("widget")]
        # (36 units drawn inside the full 39-unit tap band)
        assert item.get("height") == str(xbox_menus.COMFORTABLE_PITCH)
        assert item.get("bitmap") == "xbox/bitmaps/row_36/option_bkds"
        assert item.find('on[@event="left_mouse"]').get("run") == "mouse emit accept event"
        label, value = [widgets[c.get("widget")] for c in item.findall("child")]
        assert label.get("color") == xbox_menus.BLUE and value.get("color") == xbox_menus.BLUE
        assert int(label.get("string_index")) == index
        help_text = strings[base + "/help_strings"][index + 1]
        assert help_text.replace("\\n", " ") == about_data.LICENSES[index][2]
        assert len(help_text.split("\\n")) <= 3
    for direction, y in (("up", 73), ("down", 313)):
        arrow = base + f"/options_menu/scroll_{direction}_arrow"
        assert options.find(f'child[@widget="{arrow}"]').get("y") == str(y)
        assert widgets[arrow].get("bitmap") == f"xbox/bitmaps/arrow_sm_{direction}"
    assert widgets[base + "/cancel"].get("text") == "BACK"
    assert base + "/a_butn" not in widgets
    assert all("=" not in w.get("text", "") for name, w in widgets.items() if name.startswith(base + "/"))


def test_about_and_notices_layout_clears_help_and_legend():
    root = ET.parse(XBOX_SCREEN).getroot()
    widgets = {w.get("name"): w for w in root.iter("widget")}
    base = "xbox/settings/about"
    help = widgets[base + "/help"]
    options = widgets[base + "/options_menu"]
    # the rows the list shows at once stop above the legal text; the rest scroll into those slots
    rows = [c for c in options.findall("child") if "/op_" in c.get("widget")]
    shown = rows[:int(options.get("visible_rows", len(rows)))]
    assert len(shown) < len(rows) and int(options.get("height")) <= int(help.get("top"))
    for child in shown:
        assert int(child.get("y")) + int(widgets[child.get("widget")].get("height")) <= int(help.get("top"))
    for direction in ("up", "down"):
        assert options.find(f'child[@widget="{base}/options_menu/scroll_{direction}_arrow"]') is not None
    assert int(help.get("top")) + int(help.get("height")) <= 414
    help = widgets[base + "/notices/help"]
    assert 73 + 6 * 39 + 36 <= int(help.get("top"))
    assert int(help.get("top")) + int(help.get("height")) <= 414


def test_xbox_about_is_current(tmp_path):
    # (the whole generator runs: row density, legends and spinners are passes over every document)
    assets = tmp_path / "menus"
    shutil.copytree(MENUS / "ce", assets / "ce")
    shutil.copytree(MENUS / "xbox/art", assets / "xbox/art")
    shutil.copy(MENUS / "menus.json", assets / "menus.json")
    xbox_menus.generate(assets)
    assert XBOX_SCREEN.read_text() == (assets / "xbox/settings.about.xml").read_text()
