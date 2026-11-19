"""The split-screen row on an iPhone with no game controller connected."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MENU_FUNCTIONS = (ROOT / "port/linux/game/menu_functions.c").read_text()
HANDLER = (ROOT / "source/interface/ui_widget_event_handler_functions.c").read_text()
DESCRIPTION = (ROOT / "source/interface/ui_widget_game_data_input_functions.c").read_text()
PATCHES = (ROOT / "port/assets/menus/xbox/patches.xml").read_text()


def body(source, signature):
    start = source.rindex(signature)
    return source[start:source.index("\n}\n", start)]


def test_phone_import_is_wired_through_host_and_guest():
    assert "host_sdl_is_phone" in (ROOT / "port/runtime/host_imports.list").read_text().split()
    assert "int host_sdl_is_phone(void);" in (ROOT / "port/runtime/guest/runtime/guest_host.h").read_text()
    assert "host_sdl_is_phone" in (ROOT / "port/ios/host/host_sdl.c").read_text()


def test_needs_controller_only_on_a_phone_without_pads():
    helper = body(MENU_FUNCTIONS, "boolean menu_split_screen_needs_controller(void)")
    assert "host_sdl_is_phone()" in helper and "xinput_sdl_has_physical_gamepad()" in helper


def test_row_is_grayed_but_focusable_and_cannot_start():
    assert "split_item_off(widget)" in body(MENU_FUNCTIONS, "ui_widget_port_browser_row_unavailable")
    assert "menu_split_screen_needs_controller()" in body(HANDLER, "split_screen_game_initialize")


def test_panel_explains_and_follows_controller_state_live():
    assert "Split screen on iPhone needs a game" in PATCHES and "controller for each extra player." in PATCHES
    update = body(DESCRIPTION, "static void multiplayer_type_menu_update_extended_description(")
    assert "multiplayer_type_split" in update and "menu_split_screen_needs_controller()" in update
