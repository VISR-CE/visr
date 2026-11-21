"""Resolves every op of the shipped menu patches against the disc's own menus.

The applier in port/linux/game/menu_tags.c drops a patch's whole set of ops when any one of them does not
resolve (patch_fail), and the original list then shows on the device. This test walks
port/assets/menus/xbox/patches.xml and requires every list=, screen=, before=, after=, remove child= and
replace child= to name a widget of the disc's menus, as tools/fixtures/disc_menu_layout.json records them
(structure only, written from a ui.map by tools/ui_map_tags.py --write-layout), or a row an earlier op of the
same patch inserted.
"""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import ui_map_tags  # noqa: E402

PATCHES = ROOT / "port/assets/menus/xbox/patches.xml"


# Children a patch removes that the map the manifest was written from does not have (another release's does):
# the game skips a screen child it cannot find.
ABSENT_FROM_THIS_MAP = {
    "ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\profile_edit_button_bar",
}


def check_patch(layout, patch):
    errors = []
    list_path, screen_path = patch.get("list"), patch.get("screen")
    label = list_path.rsplit("\\", 1)[-1]
    if list_path not in layout:
        return [f"{label}: list {list_path} is not in the disc's menus"]
    if screen_path not in layout:
        return [f"{label}: screen {screen_path} is not in the disc's menus"]
    if layout[list_path]["type"] != "column_list":
        errors.append(f"{label}: {list_path} is a {layout[list_path]['type']}, not a column list")
    rows = [child["widget"] for child in layout[list_path]["children"]]
    screen = [child["widget"] for child in layout[screen_path]["children"]]
    inserted = set()

    def known(name):
        if "\\" in name:
            return name in rows or name in screen
        return name in inserted

    for op in patch:
        if op.tag == "row":
            for key in ("before", "after"):
                anchor = op.get(key)
                if anchor and not known(anchor):
                    errors.append(f"{label}: row {op.get('name')} {key}={anchor} does not resolve")
            model = op.get("model")
            if model and model not in layout:
                errors.append(f"{label}: row {op.get('name')} model={model} is not in the disc's menus")
            inserted.add(op.get("name"))
        elif op.tag in ("remove", "replace"):
            child = op.get("child")
            if child in ABSENT_FROM_THIS_MAP:
                continue
            if not known(child):
                errors.append(f"{label}: {op.tag} child={child} does not resolve")
            if op.tag == "replace":
                inserted.add(op.get("name"))
            else:
                inserted.discard(child)
    return errors


def test_shipped_patches_resolve():
    layout = ui_map_tags.load_layout()
    errors, checked = [], 0
    for patch in ET.parse(PATCHES).getroot().iter("patch"):
        checked += 1
        errors += check_patch(layout, patch)
    assert checked >= 5
    assert not errors, "\n".join(errors)


def test_the_layout_manifest_holds_structure_only():
    layout = ui_map_tags.load_layout()
    assert len(layout) > 400
    for path, widget in layout.items():
        assert path.startswith("ui\\"), path
        assert set(widget) == set(ui_map_tags.LAYOUT_KEYS), path
        assert "text" not in widget
