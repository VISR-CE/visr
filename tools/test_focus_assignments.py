"""Every move of a menu widget's focus goes through `focus_child` or `focus_chain` (menu_functions.c).

A focus set on a descendant alone (`list->focused_child = row`) leaves the screen's real focus, the chain an event follows
from the screen through each `focused_child`, on another panel: the server browser's first game took the focus that way, the
screen's focus stayed on its detail panel, and no Down press reached the list's handler. So port code may assign
`focused_child` only inside `focus_child`, or to clear it (NULL), or on a line listed in ALLOWED with the reason.
"""

from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]

# (file, function) -> why that function may assign `focused_child` directly
ALLOWED = {
    ("port/linux/game/menu_functions.c", "focus_child"): "the one setter every other focus move in the port's menu code calls",
    ("port/linux/include/halo_menu_focus.h", None): "the pointer click's focus macro (shared with ui_widget.c): its _give moves "
        "the focus along the whole chain, or between siblings that already share the focused parent",
}

ASSIGN = re.compile(r"->\s*focused_child\s*=(?!=)\s*([^;]*);")
FUNCTION = re.compile(r"^(?:static\s+)?[A-Za-z_][\w \*]*?\b(\w+)\([^;]*$")


def port_sources():
    out = subprocess.run(["git", "ls-files", "port"], cwd=ROOT, check=True, capture_output=True, text=True).stdout
    return [path for path in out.split() if path.endswith((".c", ".m", ".h")) and "/assets/" not in path]


def assignments(path):
    function = None
    for number, line in enumerate((ROOT / path).read_text(errors="replace").splitlines(), 1):
        match = FUNCTION.match(line)
        if match and not line.startswith(("\t", " ", "#", "/")):
            function = match.group(1)
        found = ASSIGN.search(line)
        if found:
            yield number, function, found.group(1).strip(), line.strip()


def test_focus_is_only_assigned_through_the_helper():
    bad = []
    for path in port_sources():
        for number, function, value, line in assignments(path):
            if value in ("NULL", "0") or value.endswith("= NULL"):
                continue   # clearing a focus moves it nowhere
            if (path, function) not in ALLOWED:
                bad.append(f"{path}:{number} ({function}): {line}")
    assert not bad, "assign `focused_child` through focus_child or focus_chain (they set the ancestors too):\n" + "\n".join(bad)


def test_the_allowlist_is_not_stale():
    used = {(path, function) for path in port_sources() for _, function, value, _ in assignments(path) if value != "NULL"}
    assert set(ALLOWED) <= used, f"allowlist entries with no assignment left: {set(ALLOWED) - used}"
