"""The Xbox look's patch table (port/assets/menus/xbox/patches.xml), checked without the game.

`tools/test_menu_patches.py` runs the patch code against a stand-in ui.map; this checks the table's own content: that
every screen it opens and every function it runs exists, that the multiplayer type list ends with six rows that each
show a description and picture of their own, and that the rows only some devices show carry the platform that says so.
"""
from pathlib import Path
import re
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / "port/assets/menus/xbox"
MP = "ui\\shell\\main_menu\\multiplayer_type_select\\"
# the Xbox's own rows of the multiplayer type list, in order, each with the description and picture of its place
MP_ROWS = ["multiplayer_type_coop_item", "multiplayer_type_split_item", "multiplayer_type_conn_item",
           "multiplayer_type_gametypes_item"]
PLATFORMS = {"quit", "tvos", "keyboard"}


def table():
    return ET.parse(MENUS / "patches.xml").getroot()


def our_widgets():
    names = set()
    for path in MENUS.glob("*.xml"):
        names.update(w.get("name") for w in ET.parse(path).getroot().iter("widget"))
    return names


def port_functions():
    """The names in menu_tags.c's port_function_names."""
    source = (ROOT / "port/linux/game/menu_tags.c").read_text()
    block = source[source.index("port_function_names[] ="):]
    block = block[:block.index("\n};")]
    return set(re.findall(r'"([^"]+)"', re.sub(r"/\*.*?\*/", "", block, flags=re.S)))


def rows(patch):
    return [row for row in patch if row.tag in ("row", "replace", "remove")]


def test_every_screen_and_function_exists():
    widgets, functions = our_widgets(), port_functions()
    for patch in table():
        for row in rows(patch):
            for key in ("open", "widget"):
                target = row.get(key)
                if target and "\\" not in target:
                    assert target in widgets, f"{row.get('name')}: no widget {target}"
            for key in ("run", "created"):
                if row.get(key):
                    assert row.get(key) in functions or row.get(key) in {"main menu quit game"}, \
                        f"{row.get('name')}: no port function {row.get(key)}"


def test_row_names_are_unique_and_short_and_platforms_known():
    names = []
    for patch in table():
        assert "\\" in patch.get("list") and "\\" in patch.get("screen")
        for row in rows(patch):
            if row.tag != "remove":
                names.append(row.get("name"))
                assert len(row.get("name")) <= 31, row.get("name")
            if row.get("platform"):
                assert row.get("platform") in PLATFORMS
    assert len(names) == len(set(names))


def test_multiplayer_type_has_six_rows_each_with_its_own_description_and_picture():
    patch = next(p for p in table() if p.get("list") == MP + "multiplayer_type_select_list")
    order = list(MP_ROWS)
    descriptions = {name: ("map", index) for index, name in enumerate(MP_ROWS)}
    added = {}
    for row in rows(patch):
        if row.tag == "replace":
            # SYSTEM LINK keeps the description its place had
            assert row.get("child") == MP + "multiplayer_type_conn_item" and not row.get("description")
            order[order.index("multiplayer_type_conn_item")] = row.get("name")
            descriptions[row.get("name")] = descriptions.pop("multiplayer_type_conn_item")
        else:
            anchor = row.get("after")
            anchor = anchor[len(MP):] if anchor.startswith(MP) else anchor
            anchor = "multiplayer_type_join_lan" if anchor == "multiplayer_type_conn_item" else anchor
            order.insert(order.index(anchor) + 1, row.get("name"))
            assert row.get("description") and row.get("picture") is not None
            assert 0 <= int(row.get("picture")) < len(MP_ROWS), "a frame of the four the Xbox's picture has"
            added[row.get("name")] = (row.get("description"), row.get("picture"))
            descriptions[row.get("name")] = ("added", len(added))
    assert order == ["multiplayer_type_coop_item", "multiplayer_type_split_item", "multiplayer_type_join_lan",
                     "multiplayer_type_internet", "multiplayer_type_join_direct", "multiplayer_type_gametypes_item"]
    assert len(set(descriptions.values())) == 6
    assert len({text for text, _ in added.values()}) == len(added)


def test_multiplayer_modes_follow_the_row_names():
    """`multiplayer_mode_set` (menu_functions.c) chooses the mode from the row's name."""
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    body = source[source.index("static void multiplayer_mode_set"):]
    body = body[:body.index("\n}\n")]
    for name, pattern in (("multiplayer_type_join_lan", "join_lan"), ("multiplayer_type_join_direct", "join_direct")):
        assert f'strstr(name, "{pattern}")' in body and pattern in name
    # INTERNET is the browser's mode, the one no name pattern chooses
    assert "internet" in "multiplayer_type_internet" and not any(
        f'strstr(name, "{p}")' in body and p in "multiplayer_type_internet" for p in ("create_inter", "create_lan",
                                                                                       "join_lan", "join_direct"))


def test_main_menu_rows_are_quit_then_custom_campaign():
    """Both go in after SETTINGS, QUIT second, so that it ends up first and no gap is left above it."""
    patch = next(p for p in table() if p.get("screen") == "ui\\shell\\main_menu\\main_menu")
    custom, quit_row = rows(patch)
    settings = "ui\\shell\\main_menu\\main_menu_item_settings"
    assert custom.get("text") == "CUSTOM CAMPAIGN" and custom.get("created") == "disable if no custom campaigns"
    assert custom.get("run") == "port custom campaign begin" and not custom.get("platform")
    assert quit_row.get("text") == "QUIT" and quit_row.get("platform") == "quit"
    assert custom.get("after") == quit_row.get("after") == settings
    # (their labels are pictures, as the main menu's are, with the two frames the highlight switches between)
    bitmaps = {b.get("name"): b for b in ET.parse(MENUS / "bitmaps.xml").getroot()}
    for row in (custom, quit_row):
        assert len(bitmaps[row.get("bitmap")].findall("frame")) == 2, row.get("bitmap")


def test_profile_edit_rows():
    patch = next(p for p in table() if p.get("screen").endswith("player_profile_edit_screen"))
    texts = [(row.get("text"), row.get("platform")) for row in rows(patch)]
    assert texts == [("VIDEO", None), ("AUDIO", None), ("NETWORK", None), ("KEYBOARD & MOUSE", "keyboard"),
                     ("DELETE PROFILE", "tvos")]
    keyboard = next(row for row in rows(patch) if row.get("platform") == "keyboard")
    assert keyboard.get("open").endswith("controls_settings_screen")
    delete = rows(patch)[-1]
    assert delete.get("after").endswith("save_changes_profile_item") and delete.get("run") == "port delete request"


def test_gametype_delete_is_tvos_only():
    patch = next(p for p in table() if p.get("screen").endswith("gametype_edit_screen"))
    (row,) = rows(patch)
    assert row.get("platform") == "tvos" and row.get("text") == "DELETE GAMETYPE"


def test_in_game_help_is_added_to_advanced_setup():
    patch = next(p for p in table() if p.get("screen").endswith("advanced_controls_screen"))
    (row,) = rows(patch)
    assert row.get("widget") == "xbox/settings/gamepad_setup/op_ingame_help_advanced"
    assert row.get("after").endswith("op_auto_center")


def test_item_options_rows_set_and_save_their_own_spinners():
    patch = next(p for p in table() if p.get("screen").endswith("item_options_screen"))
    assert [row.get("name") for row in rows(patch)] == ["op_map_weapons", "op_loadout", "op_primary_weapon",
                                                         "op_secondary_weapon"]
    functions = port_functions()
    widgets = {w.get("name"): w for path in MENUS.glob("*.xml") for w in ET.parse(path).getroot().iter("widget")}
    for row in rows(patch):
        handlers = {on.get("event"): on.get("run") for on in widgets[row.get("widget")].findall("on")}
        assert handlers == {"created": "port option row init", "deleted": "port option row save"}
        assert set(handlers.values()) <= functions
    # (the rows have to end above the help text the patch's footer names)
    assert patch.get("footer") == "321"


def test_every_patch_has_an_explicit_footer():
    assert all(patch.get("footer") is not None for patch in table())
