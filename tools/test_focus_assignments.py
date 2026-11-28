"""Every move of a menu widget's focus goes through `focus_child` or `focus_chain` (menu_functions.c).

A focus set on a descendant alone (`list->focused_child = row`) leaves the screen's real focus, the chain an event follows
from the screen through each `focused_child`, on another panel: the server browser's first game took the focus that way, the
screen's focus stayed on its detail panel, and no Down press reached the list's handler. Two checks over `port/` (source/interface/ui_widget.c is
the decompiled engine; it sets focus where it loads and dispatches widgets, so it is out of scope):

1. `focused_child` is assigned only in `focus_child` (or cleared, to NULL), or in a function listed in ASSIGNERS with the reason.
2. `focus_child(` itself (which does not touch the ancestors) is called only from a function listed in LOCAL_FOCUS with the
   reason its ancestors' focus is already on that path. A new grant of focus uses `focus_chain` or justifies itself here.
"""

from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]

# (file, function or None for a macro) -> why that code may assign `focused_child` itself
ASSIGNERS = {
    ("port/linux/game/menu_functions.c", "focus_child"): "the one setter every other focus move in the port's menu code calls",
    ("port/linux/include/halo_menu_focus.h", None): "the pointer click's focus macro (shared with ui_widget.c): its _give moves "
        "the focus along the whole chain, or between siblings that already share the focused parent",
}

# function in menu_functions.c -> why a local `focus_child` is right there (the ancestors' focus is not changed)
LOCAL_FOCUS = {
    "focus_chain": "it is the walk that calls it for every ancestor",
    "server_browser_initialize": "the screen is the root: it has no ancestors",
    "focus_row": "the caller's list already holds the focus; this moves it between the list's own rows",
    "graphics_rows_show": "repair: a row just hidden had the list's focus, so the list holds it",
    "audio_rows_show": "repair: a row just hidden had the list's focus, so the list holds it",
    "video_rows_show": "repair: a row just hidden had the list's focus, so the list holds it",
    "focus_off_hidden": "repair: a bar whose focused button was hidden holds the focus",
    "browser_focus": "its own list's fallback, run only while the list holds the focus (it returns early otherwise)",
    "lobby_browser_focus_row": "the browser's rows: layout C keeps the list's child while another panel holds the focus; "
        "a caller that must give the focus calls focus_chain next (browser_first_game_focus, browser_chips_navigation)",
}

MENU = "port/linux/game/menu_functions.c"
ASSIGN = re.compile(r"(?:->|\.|\))\s*focused_child\s*=(?!=)\s*([^;]*);", re.S)
ADDRESS = re.compile(r"(?<!&)&\s*\(?[\w>.\-\[\]]*focused_child")
FUNCTION = re.compile(r"^(?:static\s+)?[A-Za-z_][\w \*]*?\b(\w+)\([^;]*$")
CLEAR = re.compile(r"^\(?\s*(?:[\w>.\-\[\]]+\s*=\s*)*(?:NULL|0)\s*\)?$")


def sources():
    out = subprocess.run(["git", "ls-files", "port"], cwd=ROOT, check=True,
                         capture_output=True, text=True).stdout
    return [path for path in out.split() if path.endswith((".c", ".m", ".h")) and "/assets/" not in path and "/tests/" not in path]


def functions_by_line(text):
    """The name of the top-level function each line is in (None outside one: a macro)."""
    name, result = None, []
    for line in text.splitlines():
        match = FUNCTION.match(line)
        if match and not line.startswith(("\t", " ", "#", "/")):
            name = match.group(1)
        result.append(name)
    return result


def assignments(path):
    text = (ROOT / path).read_text(errors="replace")
    names = functions_by_line(text)
    for match in ASSIGN.finditer(text):
        number = text.count("\n", 0, match.start()) + 1
        yield number, names[number - 1], match.group(1).strip(), " ".join(match.group(0).split())
    for match in ADDRESS.finditer(text):
        number = text.count("\n", 0, match.start()) + 1
        yield number, names[number - 1], "&alias", " ".join(match.group(0).split())


def is_clear(value):
    return bool(CLEAR.match(value))


def test_focus_is_only_assigned_through_the_helper():
    bad = []
    for path in sources():
        for number, function, value, text in assignments(path):
            if is_clear(value):
                continue   # clearing a focus moves it nowhere
            if (path, function) not in ASSIGNERS and (path, None) not in ASSIGNERS:
                bad.append(f"{path}:{number} ({function}): {text}")
    assert not bad, "move a focus with focus_child (one parent) or focus_chain (the whole chain), or list the function:\n" + "\n".join(bad)


def test_the_allowlist_is_not_stale():
    used = {(path, function) for path in sources() for _, function, value, _ in assignments(path) if not is_clear(value)}
    stale = {key for key in ASSIGNERS if key not in used and (key[0], None) not in used}
    assert not stale, f"allowlist entries with no assignment left: {stale}"


def test_focus_child_is_called_only_where_the_ancestors_already_hold_the_focus():
    text = (ROOT / MENU).read_text()
    names = functions_by_line(text)
    bad, used = [], set()
    for match in re.finditer(r"\bfocus_child\(", text):
        number = text.count("\n", 0, match.start()) + 1
        line = text.splitlines()[number - 1]
        if line.startswith("static void focus_child("):
            continue
        used.add(names[number - 1])
        if names[number - 1] not in LOCAL_FOCUS:
            bad.append(f"{MENU}:{number} ({names[number - 1]}): {line.strip()}")
    assert not bad, "focus_child leaves the ancestors' focus alone: give a grant focus_chain, or list the function:\n" + "\n".join(bad)
    assert set(LOCAL_FOCUS) <= used, f"LOCAL_FOCUS entries with no call left: {set(LOCAL_FOCUS) - used}"
