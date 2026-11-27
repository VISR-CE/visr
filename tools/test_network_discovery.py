"""Exercise the real LAN compatibility gate against advertised kinds.

The kind bytes are literals from the permitted network-format description.
A missing kind check must let SWAT through and fail this probe.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from probekit import source as probe_source  # noqa: E402


def function(source, signature):
    """The text from `signature` through its closing brace. Kept only for the tests that import it
    (rule 3a's legacy list); new code uses probekit.source."""
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def test_advertised_unsupported_kind_is_refused_before_join(tmp_path):
    source = (ROOT / "source/networking/network_client_manager.c").read_text()
    production = probe_source.function("source/networking/network_client_manager.c", "network_game_client_advertised_game_compatible")
    fixture = (ROOT / "port/ios/tests/network_discovery_probe.c").read_text()
    program = fixture.replace("/* PRODUCTION */", production)
    for name in ("halo_network_version.h", "halo_network_content.h"):
        program = program.replace('"' + name + '"',
                                  '"' + str(ROOT / "port/linux/include" / name) + '"')
    probe, binary = tmp_path / "discovery.c", tmp_path / "discovery"
    probe.write_text(program)
    subprocess.run(["clang", "-pthread", str(ROOT / "port/linux/src/delta_legacy_runtime.c"), "-std=c11", "-Wall", "-Werror", "-Wno-unused-function",
                    str(probe), "--output=" + str(binary)], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_lan_kind_is_retained_and_older_reserved_bytes_are_ignored(tmp_path):
    source = (ROOT / "source/networking/network_client_manager.c").read_text()
    declarations = "\n".join(probe_source.structure_in(source, name) for name in (
        "transport_key_id", "transport_key", "transport_xnaddr", "network_advertised_game",
        "message_server_game_advertise"))
    end = source.index("} network_game_client_advertised_versions")
    start = source.rindex("static struct\n", 0, end)
    declarations += "\n" + source[start:source.index(";", end) + 1]
    production = probe_source.function_in(source, "add_advertised_game")
    fixture = (ROOT / "port/ios/tests/network_advertisement_probe.c").read_text()
    program = fixture.replace("/* DECLARATIONS */", declarations).replace("/* PRODUCTION */", production)
    program = program.replace('"halo_network_version.h"',
                              '"' + str(ROOT / "port/linux/include/halo_network_version.h") + '"')
    program = program.replace('"halo_delta.h"', '"' + str(ROOT / "port/linux/include/halo_delta.h") + '"')
    probe, binary = tmp_path / "advertisement.c", tmp_path / "advertisement"
    probe.write_text(program)
    subprocess.run(["clang", "-pthread", str(ROOT / "port/linux/src/delta_legacy_runtime.c"), "-std=c11", "-Wall", "-Werror", "-Wno-unused-function",
                    str(probe), "--output=" + str(binary)], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_browser_names_the_advertised_mode_before_join(tmp_path):
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    declarations = probe_source.function_in(source, "game_type_name")
    declarations += "\n" + probe_source.function_in(source, "game_kind_type_text")
    if probe_source.defined_in(source, "advertised_game_type_text"):
        declarations += "\n" + probe_source.function_in(source, "advertised_game_type_text")
    browser = probe_source.function_in(source, "browser_update")
    start = browser.index("\t\tgame = multiplayer.games[index];")
    end = browser.index('\t\tvisible_set(named(row, "server_item_classic", 0), FALSE);', start)
    rows = browser[start:end]
    declarations += "\n" + probe_source.function_in(source, "server_kind_frame")
    declarations += "\n" + probe_source.function_in(source, "server_kind_icon_set")
    declarations += "\n" + probe_source.function_in(source, "browser_row_index")
    join_enum = source[source.index("enum\n{\n\t_browser_join_ok"):]
    declarations += "\n" + join_enum[:join_enum.index("};") + 3]
    declarations += "\n" + probe_source.function_in(source, "browser_join_block")
    declarations += "\n" + probe_source.function_in(source, "browser_listing_blocked")
    declarations += "static boolean menu_split_screen_needs_controller(void) { return FALSE; }\n"
    declarations += probe_source.function_in(source, "internet_item_off")
    declarations += "\n" + probe_source.function_in(source, "split_item_off")
    declarations += "\n" + probe_source.function_in(source, "ui_widget_port_browser_row_unavailable")
    renderer = (ROOT / "source/interface/ui_widget.c").read_text()
    start = renderer.index("\tif (ui_widget_port_browser_row_unavailable(widget))")
    tint = renderer[start:renderer.index(";", start) + 1]
    fixture = (ROOT / "port/ios/tests/network_browser_probe.c").read_text()
    program = fixture.replace("/* DECLARATIONS */", declarations).replace("/* ROW */", rows)
    program = program.replace("/* TINT */", tint)
    program = program.replace('"halo_network_content.h"',
                              '"' + str(ROOT / "port/linux/include/halo_network_content.h") + '"')
    probe, binary = tmp_path / "browser.c", tmp_path / "browser"
    probe.write_text(program)
    subprocess.run(["clang", "-pthread", str(ROOT / "port/linux/src/delta_legacy_runtime.c"), "-std=c11", "-Wall", "-Werror", "-Wno-unused-function",
                    "-iquote", str(ROOT / "port/linux/src"), "-iquote", str(ROOT / "port/linux/include"),
                    str(probe), "--output=" + str(binary)], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
