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
    assert "host_sdl_is_phone()" in helper and "host_gamepad_physical_count()" in helper


def test_row_is_grayed_but_focusable_and_cannot_start():
    assert "split_item_off(widget)" in body(MENU_FUNCTIONS, "ui_widget_port_browser_row_unavailable")
    assert "menu_split_screen_needs_controller()" in body(HANDLER, "split_screen_game_initialize")


def test_panel_explains_and_follows_controller_state_live():
    assert "Split screen on iPhone needs a game" in PATCHES and "controller for each extra player." in PATCHES
    update = body(DESCRIPTION, "static void multiplayer_type_menu_update_extended_description(")
    assert "multiplayer_type_split" in update and "menu_split_screen_needs_controller()" in update


def test_grayed_split_row_ignores_activation_on_every_input():
    gate = body(MENU_FUNCTIONS, "boolean ui_widget_port_split_activation_blocked(")
    assert "BUTTON_A" in gate and "EVENT_CUSTOM_ACTIVATION" in gate
    assert "focused_child" in gate and "split_item_off(widget)" in gate
    ui_widget = (ROOT / "source/interface/ui_widget.c").read_text()
    dispatch = body(ui_widget, "static void event_handler_dispatch(\n\tstruct widget_instance *widget,\n\tstruct ui_widget_definition *definition,\n\tstruct event_record *event,\n\tstruct ui_widget_event_handler_reference *handler,\n\tboolean *calling_widget_deleted)\n{")
    assert dispatch.index("ui_widget_port_split_activation_blocked(widget, handler->event_type)") < dispatch.index("_event_handler_run_function_bit")


def test_turning_internet_play_off_shows_no_restart_alert():
    assert "turns off the next time" not in MENU_FUNCTIONS
    assert 'platform_show_message("Internet Play"' not in MENU_FUNCTIONS
