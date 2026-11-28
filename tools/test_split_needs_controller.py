"""The split-screen row on an iPhone with no game controller connected."""
from pathlib import Path

from probekit import source as probe_source

ROOT = Path(__file__).resolve().parents[1]
MENU_FUNCTIONS = (ROOT / "port/linux/game/menu_functions.c").read_text()
HANDLER = (ROOT / "source/interface/ui_widget_event_handler_functions.c").read_text()
DESCRIPTION = (ROOT / "source/interface/ui_widget_game_data_input_functions.c").read_text()
PATCHES = (ROOT / "port/assets/menus/xbox/patches.xml").read_text()


def test_phone_import_is_wired_through_host_and_guest():
    assert "host_sdl_is_phone" in (ROOT / "port/runtime/host_imports.list").read_text().split()
    assert "int host_sdl_is_phone(void);" in (ROOT / "port/runtime/guest/runtime/guest_host.h").read_text()
    assert "host_sdl_is_phone" in (ROOT / "port/ios/host/host_sdl.c").read_text()


def test_needs_controller_only_on_a_phone_without_pads():
    helper = probe_source.body_in(MENU_FUNCTIONS, "menu_split_screen_needs_controller")
    assert "host_sdl_is_phone()" in helper and "host_gamepad_physical_count()" in helper


def test_row_is_grayed_but_focusable_and_cannot_start():
    assert "split_item_off(widget)" in probe_source.body_in(MENU_FUNCTIONS, "ui_widget_port_browser_row_unavailable")
    assert "menu_split_screen_needs_controller()" in probe_source.body_in(HANDLER, "split_screen_game_initialize")


def test_panel_explains_and_follows_controller_state_live():
    assert "Split screen on iPhone needs" in PATCHES and "controllers for extra players." in PATCHES
    update = probe_source.body_in(DESCRIPTION, "multiplayer_type_menu_update_extended_description")
    assert "multiplayer_type_split" in update and "menu_split_screen_needs_controller()" in update


def test_grayed_split_row_ignores_activation_on_every_input():
    gate = probe_source.body_in(MENU_FUNCTIONS, "ui_widget_port_disabled_row_activation")
    assert "BUTTON_A" in gate and "EVENT_CUSTOM_ACTIVATION" in gate
    assert "focused_child" in gate and "split_item_off(widget)" in gate
    ui_widget = (ROOT / "source/interface/ui_widget.c").read_text()
    dispatch = probe_source.body_in(ui_widget, "event_handler_dispatch")
    assert dispatch.index("ui_widget_port_disabled_row_activation(widget, handler->event_type)") < dispatch.index("_event_handler_run_function_bit")


def test_turning_internet_play_off_shows_no_restart_alert():
    assert "turns off the next time" not in MENU_FUNCTIONS
    assert 'platform_show_message("Internet Play"' not in MENU_FUNCTIONS


def test_every_disabled_row_refuses_every_input_with_the_disc_failure_sound():
    """Controller and keyboard A, a mouse click and a touch tap all reach the row as A (or the custom activation);
    the refusal plays the disc's flag_failure sound, the one a locked graphics row plays, from ui_widget.c."""
    ui_widget = (ROOT / "source/interface/ui_widget.c").read_text()
    gate = probe_source.body_in(MENU_FUNCTIONS, "ui_widget_port_disabled_row_activation")
    assert "internet_item_off(widget)" in gate and "split_item_off(widget)" in gate
    refuse = probe_source.body_in(ui_widget, "widget_instance_port_refuse_graphics_edit")
    assert "ui_widget_port_disabled_row_activation(widget, _gamepad_analog_button_a)" in refuse
    assert "_ui_audio_feedback_flag_failure" in refuse
    port_dispatch = probe_source.body_in(ui_widget, "ui_widget_port_dispatch_event")
    assert "ui_widget_port_disabled_row_activation(widget, event_type)" in port_dispatch
    assert "_ui_audio_feedback_flag_failure" in port_dispatch
    assert "ui_play_audio_feedback_sound(SOUND_ERROR)" in probe_source.body_in(MENU_FUNCTIONS, "campaign_fail")
    assert "#define SOUND_ERROR 4" in MENU_FUNCTIONS and "_ui_audio_feedback_flag_failure," in ui_widget


def test_panel_lines_fit_the_panel_without_clipping():
    """The multiplayer type panel does not wrap: a line wider than its box is clipped mid-word (a 35-character line
    showed "needs a g"). The widest line known to fit is "Turn it on in Network Settings." (31 characters), so no
    panel line here is longer, and the split sentence is broken by hand."""
    import re
    strings = {m.group(1): m.group(2) for m in re.finditer(
        r'^\[xbox\.patches\.(\w+)\]\n(?:text = .*\n)?description = "(.*)"$', (ROOT / "port/assets/strings/en.toml").read_text(), re.M)}
    text = strings["multiplayer_type_split_need_pad"]
    assert text == "Split screen on iPhone needs\\\\ncontrollers for extra players."
    for key in ("multiplayer_type_internet", "multiplayer_type_internet_off", "multiplayer_type_join_direct",
                "multiplayer_type_split_need_pad"):
        for line in strings[key].split("\\\\n"):
            assert len(line) <= 31, (key, line)


def test_disabled_row_predicate_behavior(tmp_path):
    """The real predicate, with the setting and the controller count stubbed: the screen as the start widget with the
    grayed row down the focus chain, the row itself, the rows that stay enabled, and the inputs that are not presses."""
    import subprocess
    functions = "\n".join(probe_source.function_in(MENU_FUNCTIONS, name) for name in (
        "internet_item_off", "split_item_off", "ui_widget_port_disabled_row_activation"))
    program = r"""
#include <assert.h>
#include <string.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define BUTTON_A 0
#define EVENT_CUSTOM_ACTIVATION 32
struct widget_instance { char const *name; struct widget_instance *focused_child; };
static int online, needs_pad;
/* fake: the probe switches the online setting per case */
static int config_boolean(char const *key) { (void)key; return online; }
boolean menu_split_screen_needs_controller(void) { return needs_pad; }
""" + functions + r"""
static struct widget_instance row(char const *name) { struct widget_instance r = { name, 0 }; return r; }
int main(void)
{
    struct widget_instance screen = { "screen", 0 }, split = row("multiplayer_type_split_item"),
        internet = row("multiplayer_type_internet"), create = row("multiplayer_type_create_interne"),
        lan = row("multiplayer_type_join_lan"), direct = row("multiplayer_type_join_direct");

    needs_pad = 1; online = 1;
    screen.focused_child = &split;
    assert(ui_widget_port_disabled_row_activation(&screen, BUTTON_A));
    assert(ui_widget_port_disabled_row_activation(&screen, EVENT_CUSTOM_ACTIVATION));
    assert(ui_widget_port_disabled_row_activation(&split, BUTTON_A));
    assert(!ui_widget_port_disabled_row_activation(&screen, 2) && !ui_widget_port_disabled_row_activation(&screen, 12));
    needs_pad = 0;
    assert(!ui_widget_port_disabled_row_activation(&screen, BUTTON_A));
    screen.focused_child = &internet;
    assert(!ui_widget_port_disabled_row_activation(&screen, BUTTON_A));
    online = 0;
    assert(ui_widget_port_disabled_row_activation(&screen, BUTTON_A));
    screen.focused_child = &create;
    assert(ui_widget_port_disabled_row_activation(&screen, BUTTON_A));
    screen.focused_child = &lan;
    assert(!ui_widget_port_disabled_row_activation(&screen, BUTTON_A));
    screen.focused_child = &direct;
    assert(!ui_widget_port_disabled_row_activation(&screen, BUTTON_A));
    assert(!ui_widget_port_disabled_row_activation(NULL, BUTTON_A));
    return 0;
}
"""
    source, binary = tmp_path / "predicate.c", tmp_path / "predicate"
    source.write_text(program)
    subprocess.run(["cc", "-Wall", "-Werror", "-Wno-unused-function", "-fsanitize=address,undefined", str(source),
                    "--output", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
