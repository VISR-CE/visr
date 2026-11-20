"""The INTERNET row on the multiplayer screen when internet play is off (network.online false)."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MENU_FUNCTIONS = (ROOT / "port/linux/game/menu_functions.c").read_text()
DESCRIPTION = (ROOT / "source/interface/ui_widget_game_data_input_functions.c").read_text()
PATCHES = (ROOT / "port/assets/menus/xbox/patches.xml").read_text()


def body(source, signature):
    start = source.rindex(signature) if signature.startswith('multiplayer_type') else source.index(signature)
    return source[start:source.index("\n}\n", start)]


def test_off_state_has_its_own_panel_and_picture():
    assert "Internet play is off." in PATCHES and "Turn it on in Network Settings." in PATCHES
    assert "internet_off.png" in PATCHES
    assert "xbox/art/internet_off.png" in (ROOT / "port/assets/menus/menus.json").read_text()
    assert (ROOT / "port/assets/menus/xbox/art/internet_off.png").is_file()


def test_row_stays_focusable_but_cannot_be_activated():
    helper = body(MENU_FUNCTIONS, "static boolean internet_item_off")
    assert 'config_boolean("network.online")' in helper
    assert "internet_item_off(widget)" in body(MENU_FUNCTIONS, "static boolean multiplayer_host(")
    assert re.search(r'"mp type set mode".{0,400}internet_item_off', MENU_FUNCTIONS, re.S)
    # dimmed through the draw hook, never through `disabled` (which would drop focus)
    assert "internet_item_off(widget)" in body(MENU_FUNCTIONS, "ui_widget_port_browser_row_unavailable")


def test_description_follows_the_online_setting_live():
    update = DESCRIPTION[DESCRIPTION.index("static void multiplayer_type_menu_update_extended_description(\n", DESCRIPTION.index("multiplayer_type_menu_update_extended_description(") + 100):][:1500]
    assert 'config_boolean("network.online")' in update and "index += 1" in update


def test_server_browser_no_longer_toasts_internet_off():
    assert "INTERNET PLAY IS OFF" not in MENU_FUNCTIONS
    assert "Internet play is off (Settings)" not in MENU_FUNCTIONS
