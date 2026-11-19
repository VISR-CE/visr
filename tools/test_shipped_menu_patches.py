"""Resolves every op of the shipped Xbox menu patches against the original menus.

The applier in port/linux/game/menu_tags.c drops a patch's whole set of ops when any one
of them does not resolve (patch_fail), and the original menus then show on the device.
This test walks port/assets/menus/xbox/patches.xml and patches.compact.xml and requires
every before=, after=, remove child= and replace child= to name a row or screen child of
the original widgets in port/assets/menus/ce/*.xml, or a row an earlier op of the same
patch inserted.  Lists the repository does not hold (they live in the disc's ui.map)
are named in UNVERIFIABLE and checked only for their anchors being well formed.
"""
import glob
import os
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MENUS = os.path.join(ROOT, "port", "assets", "menus")
PATCHES = ["patches.xml", "patches.compact.xml"]

# lists and screens defined only in the disc's ui.map, so not in ce/*.xml
UNVERIFIABLE = {
    "ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\advanced_controls\\advanced_controls_menu",
}


# original rows the disc's ui.map has but the PC-derived ce/*.xml lacks; the repo holds no copy of
# the disc's menus, so these can only be checked by name here and are confirmed on a device
DISC_ONLY = {
    "main_menu/settings_select/player_setup/player_profile_edit/save_changes_profile_item",
    "main_menu/multiplayer_type_select/multiplayer_type_conn_item",
    # (the Xbox SPLIT SCREEN row, which the PC menus never had; the need-controller row is inserted after it)
    "main_menu/multiplayer_type_select/multiplayer_type_split_item",
    "main_menu/settings_select/multiplayer_setup/playlist_edit/save_settings_item",
    # (the PC list of ce/*.xml no longer has the INDICATORS row, which moved into TEAMS & HUD; the disc's list does,
    # and ui_map_tags.py --check-menus resolves the remove against it)
    "main_menu/settings_select/multiplayer_setup/playlist_edit/playlist_edit_indicators_item",
    # (the disc's STARTING EQUIPMENT row, which the port's combined row replaces; gone from the PC list of ce/*.xml)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/op_starting_equipment",
    # (the four respawn rows of the disc's Player Options, which RESPAWN OPTIONS takes over; gone from the PC list)
    "main_menu/settings_select/multiplayer_setup/player_options_edit/op_respawn_time",
    "main_menu/settings_select/multiplayer_setup/player_options_edit/op_respawn_time_growth",
    "main_menu/settings_select/multiplayer_setup/player_options_edit/op_odd_man_out",
    "main_menu/settings_select/multiplayer_setup/player_options_edit/op_suicide_penalty",
}


def widget_name(tag_path):
    assert tag_path.startswith("ui\\shell\\"), tag_path
    return tag_path[len("ui\\shell\\"):].replace("\\", "/")


def load_original_widgets():
    widgets = {}
    for path in glob.glob(os.path.join(MENUS, "ce", "*.xml")):
        for node in ET.parse(path).getroot().iter("widget"):
            name = node.get("name")
            if name:
                widgets[name] = [c.get("widget") for c in node.findall("child")]
    return widgets


def check_patch(widgets, patch, label):
    errors = []
    list_path, screen_path = patch.get("list"), patch.get("screen")
    if list_path in UNVERIFIABLE:
        return errors
    rows = widgets.get(widget_name(list_path))
    screen = widgets.get(widget_name(screen_path))
    if rows is None:
        return [f"{label}: list {list_path} is not in the original menus"]
    if screen is None:
        return [f"{label}: screen {screen_path} is not in the original menus"]
    rows, screen = list(rows), list(screen)
    inserted = set()

    def known(name):
        if "\\" in name:
            short = widget_name(name)
            return short in rows or short in screen or short in DISC_ONLY
        return name in inserted

    for op in patch:
        if op.tag == "row":
            for key in ("before", "after"):
                anchor = op.get(key)
                if anchor and not known(anchor):
                    errors.append(f"{label}: row {op.get('name')} {key}={anchor} does not resolve")
            inserted.add(op.get("name"))
        elif op.tag in ("remove", "replace"):
            child = op.get("child")
            if not known(child):
                errors.append(f"{label}: {op.tag} child={child} does not resolve")
            if op.tag == "replace":
                inserted.add(op.get("name"))
    return errors


def test_shipped_patches_resolve():
    widgets = load_original_widgets()
    errors, checked = [], 0
    for name in PATCHES:
        root = ET.parse(os.path.join(MENUS, "xbox", name)).getroot()
        for patch in root.iter("patch"):
            checked += 1
            errors += check_patch(widgets, patch, f"{name} {patch.get('list').rsplit(chr(92), 1)[-1]}")
    assert checked >= 8
    assert not errors, "\n".join(errors)
