"""Every place that draws menu focus asks one rule, ui_widget_port_draws_focus.

Bug class: focus drawn by a place that decides on its own, so one kind of item looks different from another on the
same input. The rule decides by the kind of item: a list row (the engine's list rows, a generated list item's plasma
and white text, the port's own rows such as Video Setup and the server browser) draws its focus on every input, touch
too; a carousel card draws no focus outline on touch. Focus is drawn in several places (the engine's
list updaters, its render loop and carousel, and the port's own lists in menu_functions.c). This test reads every
focus-drawing site in both files and fails on one that decides without the rule.
"""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "source/interface/ui_widget.c"
PORT = ROOT / "port/linux/game/menu_functions.c"

RULE = "ui_widget_port_draws_focus"
LIST_RULE = "ui_list_child_draws_focus"

# functions that read the pointer kind for something other than drawing focus (touch wording, the touch legend)
NOT_FOCUS = {"ui_touch_disc_text", "ui_widget_port_postgame_legend"}


def enclosing_function(source, offset):
    """the name of the top-level function whose body holds offset"""
    head = None
    for match in re.finditer(r"^(?:static |__inline |boolean |void |int |short |long |char |struct |wchar_t |real )"
                             r"[^;{]*?\b(\w+)\(", source[:offset], re.MULTILINE):
        head = match.group(1)
    return head


def line_offset(source, line):
    offset = 0
    for _ in range(line - 1):
        offset = source.index("\n", offset) + 1
    return offset


def statements(source):
    """(line, statement text) for each ;-terminated statement, whitespace folded"""
    line, start = 1, 0
    for match in re.finditer(r";", source):
        text = source[start:match.end()]
        yield line + source.count("\n", start, start + len(text) - len(text.lstrip())), " ".join(text.split())
        line += text.count("\n")
        start = match.end()


def frame_violations(source):
    """frame-index writes that show focus (frame 1 of a two-frame bitmap) without the rule"""
    found = []
    for line, text in statements(source):
        if "current_frame_index" not in text or "=" not in text:
            continue
        compares_focus = re.search(r"focused_child|\bfocused\b|selected_index|saved_selected", text)
        if compares_focus and RULE not in text and LIST_RULE not in text:
            found.append((line, text))
    # a frame set to 1 inside an if on two-frame bitmaps: the if (or the one around it) must ask the rule
    for match in re.finditer(r"current_frame_index = 1;", source):
        window = source[max(0, match.start() - 240):match.start()]
        if RULE not in window and LIST_RULE not in window and "number_of_sprite_frames == 2" in window:
            found.append((source.count("\n", 0, match.start()) + 1, "current_frame_index = 1"))
    return found


def render_violations(source):
    """render calls and focus variables that pass focus or plasma decided without the rule"""
    found = []
    for match in re.finditer(r"widget_instance_render_recursive\(([^;]*?)\);", source):
        args = [" ".join(arg.split()) for arg in match.group(1).split(",")]
        if len(args) != 5 or args[0].startswith("struct"):
            continue
        for arg in args[3:]:
            if arg in ("focus", "use_nifty_plasma_fx", "TRUE", "FALSE") or RULE in arg:
                continue
            found.append((source.count("\n", 0, match.start()) + 1, arg))
    for line, text in statements(source):
        assigned = re.search(r"(?:^|[{}] )(focus|use_nifty_plasma_fx) = (.*);$", text)
        # (the drawing functions' focus flags; elsewhere `focus` can name an index being moved, not drawn)
        if not assigned or "render" not in (enclosing_function(source, line_offset(source, line)) or ""):
            continue
        value = assigned.group(2)
        # (TRUE: a definition flagged to always draw the plasma, which is not focus)
        if RULE in value or LIST_RULE in value or re.match(r"focus &&", value) or value == "TRUE":
            # (a non-list parent passes its plain focused child on, which only matters to a list beneath it, and a
            # list decides with the rule)
            continue
        found.append((line, text))
    return found


def pointer_violations(source):
    """reads of the touch pointer kind outside the rule and the non-focus functions"""
    found = []
    for match in re.finditer(r"halo_menu_pointer_controls\(\) [!=]= 2", source):
        name = enclosing_function(source, match.start())
        if name not in NOT_FOCUS | {RULE}:
            found.append((source.count("\n", 0, match.start()) + 1, name))
    return found


def violations(engine, port):
    return {
        "engine frames": frame_violations(engine),
        "engine render": render_violations(engine),
        "engine pointer": pointer_violations(engine),
        "port frames": frame_violations(port),
        "port pointer": pointer_violations(port),
    }


def test_every_focus_draw_site_asks_the_rule():
    engine, port = ENGINE.read_text(), PORT.read_text()
    found = {kind: sites for kind, sites in violations(engine, port).items() if sites}
    assert not found, f"focus drawn without {RULE}: {found}"


def test_the_rule_is_where_the_lists_get_it():
    engine = ENGINE.read_text()
    rule = engine[engine.index(f"int {RULE}("):]
    assert "halo_menu_pointer_controls() != 2" in rule[:rule.index("}")]
    list_rule = engine[engine.index(f"static boolean {LIST_RULE}("):]
    assert RULE in list_rule[:list_rule.index("}")]
    assert '#include "halo_menu_meta.h"' in PORT.read_text()
    header = (ROOT / "port/linux/include/halo_menu_meta.h").read_text()
    assert f"int {RULE}(enum halo_menu_focus_item item, int focused);" in header


def test_the_scan_finds_each_kind_of_bypass():
    """the sites as they were before the rule, one per kind, each found"""
    engine, port = ENGINE.read_text(), PORT.read_text()
    bypasses = [
        ("port", "row->animation.current_frame_index = ui_widget_port_draws_focus(HALO_MENU_FOCUS_ROW, row == list->focused_child) ? 1 : 0;",
         "row->animation.current_frame_index = row == list->focused_child ? 1 : 0;"),
        ("port", "row->animation.current_frame_index = ui_widget_port_draws_focus(HALO_MENU_FOCUS_ROW, row == focused) ? 1 : 0;",
         "row->animation.current_frame_index = row == focused ? 1 : 0;"),
        ("engine", "child->animation.current_frame_index =\n\t\t\t\t\t\tui_list_child_draws_focus(list, child) ? 1 : 0;",
         "child->animation.current_frame_index = child == list->focused_child ? 1 : 0;"),
        ("engine", "ui_widget_port_draws_focus(HALO_MENU_FOCUS_ROW, item_index == widget->parameters.list.selected_index));",
         "item_index == widget->parameters.list.selected_index);"),
        ("engine", "ui_widget_port_draws_focus(ui_list_focus_item(list), item == saved_selected),\n\t\t\tui_widget_port_draws_focus(ui_list_focus_item(list), item == saved_selected));",
         "item == saved_selected, item == saved_selected);"),
        ("engine", "if (ui_list_child_draws_focus(widget, child))\n\t\t{\n\t\t\tif (child->animation.number_of_sprite_frames == 2)",
         "if (child == widget->focused_child)\n\t\t{\n\t\t\tif (child->animation.number_of_sprite_frames == 2)"),
        ("engine", "ui_list_child_draws_focus(widget, child) : child == widget->focused_child;",
         "child == widget->focused_child : child == widget->focused_child;"),
    ]
    for where, good, bad in bypasses:
        source = engine if where == "engine" else port
        assert source.count(good) == 1, good
        broken = source.replace(good, bad)
        found = violations(broken, port) if where == "engine" else violations(engine, broken)
        assert any(found.values()), f"the scan missed: {bad}"
