"""Run the production internet listing parser and browser metadata binding.

A missing stamp read or compatibility gate admits unsupported games. Literal
records follow the permitted format description, independent of listing_make.
"""
from pathlib import Path
import subprocess
from test_network_discovery import function

ROOT = Path(__file__).resolve().parents[1]


def test_internet_kind_and_malformed_listings(tmp_path):
    source = (ROOT / "port/linux/src/p2p_lobby.c").read_text()
    declarations = "\n".join(function(source, "struct " + name + "\n") + ";"
                             for name in ("listing", "game", "tombstone", "queued"))
    production = "\n".join(function(source, signature) for signature in (
        "static unsigned long get_long(", "static int get_text(",
        "static int listing_read(", "static void listing_show(", "static void listing_take("))
    fixture = (ROOT / "port/ios/tests/network_internet_kind_probe.c").read_text()
    program = fixture.replace("/* DECLARATIONS */", declarations).replace("/* PRODUCTION */", production)
    probe, binary = tmp_path / "internet.c", tmp_path / "internet"
    probe.write_text(program)
    subprocess.run(["clang", "-pthread", str(ROOT / "port/linux/src/delta_legacy_runtime.c"), "-std=c11", "-Wall", "-Werror", "-Wno-unused-function",
                    "-iquote", str(ROOT / "port/linux/src"), "-iquote", str(ROOT / "port/linux/include"),
                    str(probe), "--output=" + str(binary)], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_internet_browser_names_a_kind_by_the_one_kind_table(tmp_path):
    """Bug class: the internet list named a kind in a copy of its own (a ticker reason, a row label) that drifted from
    the kind table. The row's and the panel's type text and the join reason read `halo_network_kind_title` and
    `halo_network_kind_reason` alone; the legacy table's row text (LAN) lives in its own function."""
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    assert "halo_network_kind_title(game->kind)" in function(
        source, "static void browser_listing_type_text(struct p2p_listing const *game, wchar_t *text, short size)\n{")
    assert "halo_network_kind_reason(game->kind)" in function(source, "static short browser_join_block(")
    update = function(source, "static void lobby_browser_update(struct widget_instance *list)\n{")
    assert "halo_network_kind_reason" not in update and "game_kind_type_text" not in update
