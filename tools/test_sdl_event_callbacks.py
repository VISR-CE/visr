"""SDL runs an event watcher or event filter on the thread that pushes the event, holding the event-watch lock. The
game thread pushes joystick and gamepad events (host_controller_refresh), while it holds the joystick lock, so a
callback that reaches an SDL joystick or gamepad function, however indirectly, takes the two locks in the opposite
order of that thread: a hang (the tvOS remote filter did, through text_input_blocked and hardware_held). This finds
every callback registered with SDL_AddEventWatch or SDL_SetEventFilter in port/ios/host and fails when one reaches
such a function through the functions defined in its own file."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HOST = ROOT / "port/ios/host"
FORBIDDEN = re.compile(r"\bSDL_(?:GetGamepad\w*|GetJoystick\w*|OpenGamepad|CloseGamepad|UpdateJoysticks|"
                       r"LockJoysticks|UnlockJoysticks|OpenJoystick|RumbleGamepad\w*|SetJoystickVirtual\w*)\b")
REGISTER = re.compile(r"\bSDL_(?:AddEventWatch|SetEventFilter)\s*\(\s*(\w+)")
DEFINITION = re.compile(r"^(?:static\s+)?[\w\s\*]*?\b(\w+)\s*\([^;{)]*\)\s*\{", re.MULTILINE)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", " ", text)


def functions(text: str) -> dict[str, str]:
    """Function name -> body, for the definitions at file scope (brace matched)."""
    found = {}
    for match in DEFINITION.finditer(text):
        name, depth, index = match.group(1), 1, match.end()
        if name in ("if", "for", "while", "switch"):
            continue
        while depth and index < len(text):
            depth += {"{": 1, "}": -1}.get(text[index], 0)
            index += 1
        found.setdefault(name, text[match.end():index])
    return found


def violations(text: str) -> list[str]:
    text = strip_comments(text)
    defined = functions(text)
    problems = []
    for callback in sorted(set(REGISTER.findall(text))):
        seen, pending = set(), [callback]
        while pending:
            name = pending.pop()
            if name in seen or name not in defined:
                continue
            seen.add(name)
            for hit in FORBIDDEN.findall(defined[name]):
                problems.append(f"{callback} reaches {hit} through {name}")
            pending += [other for other in defined if other not in seen and re.search(rf"\b{other}\b", defined[name])]
    return problems


def test_no_event_callback_reaches_an_sdl_joystick_function():
    problems = []
    for path in sorted(HOST.glob("*.[cm]")):
        problems += [f"{path.name}: {problem}" for problem in violations(path.read_text())]
    assert not problems, "\n".join(problems)


def test_the_tvos_remote_filter_shape_is_found():
    """What this would have said about the filter before it read a cached flag."""
    source = """
static bool hardware_held(void) { int n; SDL_GetGamepads(&n); return n; }
static bool text_input_blocked(void) { return hardware_held(); }
static bool SDLCALL remote_event(void *userdata, SDL_Event *event) { return text_input_blocked(); }
void start(void) { SDL_SetEventFilter(remote_event, 0); }
"""
    assert violations(source) == ["remote_event reaches SDL_GetGamepads through hardware_held"]
    clean = source.replace("return text_input_blocked();", "return cached;")
    assert violations(clean) == []


def test_the_callbacks_are_found_in_the_host():
    registered = {name for path in HOST.glob("*.[cm]") for name in REGISTER.findall(strip_comments(path.read_text()))}
    assert {"remote_event", "text_prompt_watch", "controller_input_watch"} <= registered
