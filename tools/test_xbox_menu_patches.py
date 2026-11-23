"""The Xbox look's patch table (port/assets/menus/xbox/patches.xml), checked without the game.

`tools/test_menu_patches.py` runs the patch code against a stand-in ui.map; this checks the table's own content: that
every screen it opens and every function it runs exists, that the multiplayer type list ends with six rows that each
show a description and picture of their own, and that the rows only some devices show carry the platform that says so.
"""
import json
from pathlib import Path
import re
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / "port/assets/menus/xbox"
MP = "ui\\shell\\main_menu\\multiplayer_type_select\\"
# the Xbox's own rows of the multiplayer type list, in order, each with the description and picture of its place
MP_ROWS = ["multiplayer_type_coop_item", "multiplayer_type_split_item", "multiplayer_type_conn_item",
           "multiplayer_type_gametypes_item"]
PLATFORMS = {"tvos", "keyboard", "mac_app", "!keyboard"}


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
                    assert row.get(key) in functions, \
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
                assert row.get("platform").removeprefix("!") in PLATFORMS
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
        elif row.tag == "remove":
            # the throwaway INTERNET (off) row only leaves its description and picture behind (internet-off panel)
            order.remove(row.get("child").removeprefix(MP))
            descriptions.pop(row.get("child").removeprefix(MP))
        else:
            anchor = row.get("after")
            anchor = anchor[len(MP):] if anchor.startswith(MP) else anchor
            anchor = "multiplayer_type_join_lan" if anchor == "multiplayer_type_conn_item" else anchor
            order.insert(order.index(anchor) + 1, row.get("name"))
            assert row.get("description") and row.get("picture") is not None
            picture = row.get("picture")
            if picture.startswith("png:"):
                assert (MENUS.parent / picture[4:]).is_file(), "a picture of our own"
            else:
                assert 0 <= int(picture) < len(MP_ROWS), "a frame of the four the Xbox's picture has"
            added[row.get("name")] = (row.get("description"), row.get("picture"))
            descriptions[row.get("name")] = ("added", len(added))
    assert order == ["multiplayer_type_coop_item", "multiplayer_type_split_item", "multiplayer_type_join_lan",
                     "multiplayer_type_internet", "multiplayer_type_join_direct", "multiplayer_type_gametypes_item"]
    assert len(set(descriptions.values())) == 6
    assert len({text for text, _ in added.values()}) == len(added)


def test_multiplayer_modes_follow_the_row_names():
    """`multiplayer_mode_named` (menu_functions.c) chooses the mode from the row's name."""
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    body = source[source.index("static short multiplayer_mode_named"):]
    body = body[:body.index("\n}\n")]
    for name, pattern in (("multiplayer_type_join_lan", "join_lan"), ("multiplayer_type_join_direct", "join_direct")):
        assert f'strstr(name, "{pattern}")' in body and pattern in name
    # INTERNET is the browser's mode, the one no name pattern chooses
    assert "internet" in "multiplayer_type_internet" and not any(
        f'strstr(name, "{p}")' in body and p in "multiplayer_type_internet" for p in ("create_inter", "create_lan",
                                                                                       "join_lan", "join_direct"))


def test_profile_edit_rows():
    patch = next(p for p in table() if p.get("screen").endswith("player_profile_edit_screen"))
    texts = [(row.get("text"), row.get("platform")) for row in rows(patch)]
    assert texts == [("VIDEO", "mac_app"), ("GRAPHICS", "!mac_app"), ("AUDIO", None), ("NETWORK", None),
                     ("KEYBOARD & MOUSE", "keyboard"), ("ABOUT", "!keyboard"), ("DELETE PROFILE", "tvos"), (None, None), (None, None)]
    # the trailing entries are the <remove>s of SAVE CHANGES and the empty button bar (the profile editor saves as it goes)
    keyboard = next(row for row in rows(patch) if row.get("platform") == "keyboard")
    assert keyboard.get("open").endswith("controls_settings_screen")
    delete = rows(patch)[-3]
    assert delete.get("after").endswith("save_changes_profile_item") and delete.get("run") == "port delete request"


def test_profile_edit_drops_the_empty_button_bar():
    patch = next(p for p in table() if p.get("screen").endswith("player_profile_edit_screen"))
    removed = [row.get("child") for row in rows(patch) if row.tag == "remove" or row.get("child")]
    assert any(child.endswith("profile_edit_button_bar") for child in removed)


def test_in_game_help_is_added_to_advanced_setup():
    patch = next(p for p in table() if p.get("screen").endswith("advanced_controls_screen"))
    added = [r for r in rows(patch) if r.get("widget")]
    assert [r.get("widget").rsplit("/", 1)[1] for r in added] == [
        f"op_{k}_advanced"
        for k in ("invert_look", "look_sensitivity", "vibration", "flight_inversion", "autocenter", "ingame_help")
    ]
    assert all(r.get("pitch") == "39" for r in added)


ADVANCED_HELP_WORD = {
    "invert_look": "looks",
    "look_sensitivity": "How fast",
    "vibration": "vibrates",
    "flight_inversion": "Flight",
    "autocenter": "center",
    "ingame_help": "hints",
}


def test_each_advanced_row_has_help_for_its_own_values():
    patch = next(p for p in table() if p.get("screen").endswith("advanced_controls_screen"))
    added = [r for r in rows(patch) if r.get("widget")]
    assert len(added) == len(ADVANCED_HELP_WORD)
    for row in added:
        key = row.get("widget").rsplit("/", 1)[1].removeprefix("op_").removesuffix("_advanced")
        help_text = row.get("help")
        # (the list shows a row's help by its first line, one line a value for a two-value row; a slider has one)
        lines = help_text.split("|")
        assert len(lines) == (1 if key == "look_sensitivity" else 2), (key, help_text)
        assert ADVANCED_HELP_WORD[key] in lines[0], (key, help_text)
        others = [w for k, w in ADVANCED_HELP_WORD.items() if k != key and w in help_text]
        assert not others, (key, others)


def test_advanced_rows_are_standard_tall_rows():
    widgets = {w.get("name"): w for path in MENUS.glob("settings.gamepad_setup.xml") for w in ET.parse(path).getroot().iter("widget")}
    for key in ADVANCED_HELP_WORD:
        row = widgets[f"xbox/settings/gamepad_setup/op_{key}_advanced"]
        original = widgets[f"xbox/settings/gamepad_setup/op_{key}"]
        assert row.get("height") == "39" and "/row_36/" in row.get("bitmap"), key
        for attribute in ("height", "bitmap"):
            assert row.get(attribute) == original.get(attribute), (key, attribute)
        assert [c.get("y") for c in row.findall("child")] == [c.get("y") for c in original.findall("child")], key


def test_every_widget_a_patch_row_adds_is_a_standard_tall_row():
    """A widget a patch row puts into a disc list sits on no list of ours, so the density pass never reaches it
    (ADVANCED CONTROLS, Item Options and Player Options drew short pills with gaps). Each is made tall by the
    generator and says so, and the game restacks its list with the disc's own rows (rows="tall";
    tools/test_disc_contract.py lays every such list out and checks its footer)."""
    widgets = {w.get("name"): w for path in MENUS.glob("*.xml") for w in ET.parse(path).getroot().iter("widget")}
    checked = 0
    for patch in table():
        for row in rows(patch):
            if not row.get("widget"):
                continue
            widget = widgets[row.get("widget")]
            assert widget.get("height") == "39" and widget.get("row") == "tall", (
                row.get("widget"), widget.get("height"), widget.get("row"))
            assert patch.get("rows") == "tall", patch.get("list")
            checked += 1
    assert checked >= len(ADVANCED_HELP_WORD) + 4


def test_item_options_rows_set_and_save_their_own_spinners():
    patch = next(p for p in table() if p.get("screen").endswith("item_options_screen"))
    # (and the Xbox's own STARTING EQUIPMENT row, removed: the combined row takes its place)
    assert [row.get("name") for row in rows(patch)] == ["op_spawn_weapons", "op_map_weapons", "op_choose_weapons", None]
    functions = port_functions()
    widgets = {w.get("name"): w for path in MENUS.glob("*.xml") for w in ET.parse(path).getroot().iter("widget")}
    for row in rows(patch)[:3]:
        handlers = {on.get("event"): on.get("run") for on in widgets[row.get("widget")].findall("on")
                    if on.get("run") and on.get("event") in ("created", "deleted")}
        assert handlers == {"created": "port option row init", "deleted": "port option row save"}
        assert set(handlers.values()) <= functions
    # (the rows have to end above the help text the patch's footer names)
    assert patch.get("footer") == "321"


def test_every_patch_has_an_explicit_footer():
    assert all(patch.get("footer") is not None for patch in table())


def test_delete_confirmations_finish_both_edit_flows():
    widgets = {w.get('name'): w for path in MENUS.glob('*.xml') for w in ET.parse(path).getroot().iter('widget')}
    # (the rows that ask to delete: the profile page's patch row, and the generated gametype editor's row)
    deletes = [row for patch in table() for row in rows(patch) if row.get('run') == 'port delete request']
    deletes += [on for w in widgets.values() for on in w.findall('on')
                if on.get('event') == 'a' and on.get('run') == 'port delete request']
    for kind in ('profile', 'gametype'):
        (row,) = {row.get('open'): row for row in deletes
                  if row.get('open') == f'xbox/error/confirm_delete_{kind}_modal_fullscreen'}.values()
        modal = widgets[row.get('open')]
        for event in ('b', 'back'):
            cancel = modal.find(f'on[@event="{event}"]')
            assert cancel.get('run') == 'cancel profile delete' and cancel.get('back') == 'true'
        assert not any('button_bar' in c.get('widget', '') for c in modal.findall('child'))
        for button, caption in (('a', 'OK'), ('b', 'CANCEL')):
            legend = f'xbox/error/confirm_delete_{kind}/legend/{button}_butn'
            assert modal.find(f'child[@widget="{legend}"][@y="294"]') is not None
            assert widgets[f'{legend}_glyph'].get('bitmap') == f'xbox/browser/{button}_butn'
            assert widgets[f'{legend}/touch_caption'].get('text') == caption
            assert widgets[legend].find('on').get('run') == ('mouse emit accept event' if button == 'a' else 'mouse emit back event')
        for event in ('a', 'start'):
            handler = modal.find(f'on[@event="{event}"]')
            assert handler.get('run') == 'port delete finish' and handler.get('close') is None

def test_video_is_mac_app_only_in_profile_and_pause_settings():
    profile = next(p for p in table() if p.get('screen').endswith('player_profile_edit_screen'))
    video = profile.find("row[@name='profile_item_video']")
    assert video.get('platform') == 'mac_app'
    # A filtered VIDEO must not leave AUDIO referring to a missing anchor.
    audio = profile.find("row[@name='profile_item_audio']")
    assert not audio.get('platform')
    assert audio.get('before').endswith('save_changes_profile_item')
    assert audio.get('model').endswith('color_profile_item')
    hub = ET.parse(MENUS / 'settings.xml')
    options = hub.find("widget[@name='xbox/settings/settings_list']")
    children = {c.get('widget').rsplit('/', 1)[-1]: c for c in options.findall('child')}
    assert children['video'].get('platform') == 'mac_app'
    assert not children['audio'].get('platform') and not children['network'].get('platform')
    # The runtime packs visible rows at the first two remaining rows' pitch.
    assert options.get('visible_rows') == '6'
    assert [int(children[key].get('y')) for key in ('controls', 'video', 'audio', 'network', 'keyboard')] == [
        78, 117, 156, 195, 273]


def hub_flow_prefixes():
    """The name prefixes menu_tags.c's `menu_screen_in_hub_flow` keeps the hub flow for."""
    source = (ROOT / "port/linux/game/menu_tags.c").read_text()
    block = source[source.index("static boolean menu_screen_in_hub_flow"):]
    block = block[:block.index("\n}\n")]
    return [p.replace("\\\\", "\\") for p in re.findall(r'\[\] = "([^"]+)"', block)]


def test_back_from_a_screen_opened_in_the_hub_flow_reaches_the_screen_below():
    """Bug class: BACK reopens the screen below by name, and the always-redirected screens (the profile picker and
    edit page, which the main menu reaches as the hub) open as the hub unless the hub flow is up. Every screen the
    profile page's rows open must keep the flow, or BACK from it lands on the hub (reported on the Duo: BACK from a
    profile's setting went to SETTINGS). Walks the profile page's patch rows against the flow's prefixes."""
    always = re.findall(r'\{"([^"]+)", "pc\\\\xbox\\\\settings\\\\settings_screen"\}',
                        (ROOT / "port/linux/include/menu_equivalence.h").read_text())
    assert len(always) == 2  # (the profile picker and edit page)
    prefixes = hub_flow_prefixes()
    assert len(prefixes) == 4
    for screen in always:
        screen = screen.replace("\\\\", "\\")
        assert any(screen.startswith(p) for p in prefixes), f"{screen} is not in the hub flow"
    patch = next(p for p in table() if p.get("screen", "").endswith("player_profile_edit_screen"))
    opened = [r.get("open") for r in rows(patch) if r.get("open")]
    assert len(opened) >= 5
    for target in opened:
        name = "pc\\" + target.replace("/", "\\")
        assert any(name.startswith(p) for p in prefixes), f"BACK from {target} would leave the hub flow"


def patched_row_widgets():
    return {r.get("widget") for p in table() for r in rows(p) if r.get("widget")}


def sub_screen_rows():
    """(file, row, open targets, parent list) for every widget of ours that opens a screen on A."""
    documents = {path: ET.parse(path).getroot() for path in sorted(MENUS.glob("*.xml"))
                 if not path.name.startswith("patches")}
    parents = {}
    for root in documents.values():
        for widget in root.iter("widget"):
            for child in widget.findall("child"):
                parents.setdefault(child.get("widget"), widget)
    found = []
    for path, root in documents.items():
        for widget in root.iter("widget"):
            opens = {o.get("open") for o in widget.findall("on") if o.get("event") == "a" and o.get("open")}
            if opens:
                found.append((path, widget, opens, parents.get(widget.get("name"))))
    return found


def test_every_row_that_opens_a_sub_screen_answers_a_start_and_a_tap():
    """Checks the XML shape only: every row that opens a screen has the same screens on A and Start (when it has a
    Start handler) and sits in a list the pointer walks, or is named by a patch row. It does not prove a tap opens
    the screen: the gametype editor's tap-goes-back bug is open, and the device tap walk with the pointer log
    (a kind 0 hit on the row, never kind 4) is the test that covers it. A tap presses A on the row under it
    (`ui_mouse_widget_is_item`: a child of a column list, or of a list that tabs through its children)."""
    rows_found = sub_screen_rows()
    assert len(rows_found) >= 20
    problems = []
    for path, widget, opens, parent in rows_found:
        name = widget.get("name", "").rsplit("/", 1)[-1]
        starts = {o.get("open") for o in widget.findall("on") if o.get("event") == "start" and o.get("open")}
        if starts != opens:
            problems.append(f"{path.name} {name}: A and Start open different screens")
        if parent is None:
            # (a row the patch table puts into a disc list: patches.xml must name it, or nothing holds it)
            if widget.get("name") not in patched_row_widgets():
                problems.append(f"{path.name} {name}: no list holds it and no patch row adds it")
            continue
        if parent.get("name", "").endswith("_screen"):
            continue  # (a legend button on its screen: the engine's button targets, not a list row)
        flags = parent.get("flags", "")
        if parent.get("type") != "column_list" and "tabs_children" not in flags and "tabs_items" not in flags:
            problems.append(f"{path.name} {name}: its list {parent.get('name')} is not walked by the pointer")
    assert not problems, "\n".join(problems)


def test_the_gametype_editor_rows_that_open_screens_are_tap_targets():
    """The rows a touch player taps in the gametype editor (RESPAWN, CHOOSE WEAPONS, VEHICLES and the rest of the patch)
    are among the rows the walk above covers."""
    found = sub_screen_rows()
    covered = {w.get("name", "").rsplit("/", 1)[-1] for _, w, _, _ in found}
    assert {"op_respawn", "op_choose_weapons"} <= covered
    # (and the seven rows of the generated gametype editor's list)
    editor = [w for _, w, _, parent in found if parent is not None and parent.get("name", "").endswith(
        "playlist_edit/gametype_list")]
    assert len(editor) == 7
