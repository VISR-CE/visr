"""SDL runs an event watcher or event filter on the thread that pushes the event, holding the event-watch lock. The
game thread pushes joystick and gamepad events (host_controller_refresh), while it holds the joystick lock, so a
callback that reaches an SDL joystick or gamepad function, however indirectly, takes the two locks in the opposite
order of that thread: a hang (the tvOS remote filter did, through text_input_blocked and hardware_held). This finds
every callback registered with SDL_AddEventWatch or SDL_SetEventFilter in port/ios/host and fails when one reaches
such a function (or host_main_sync, which blocks on main while main may wait on the watch lock) through the functions
defined in port/ios/host. Limits: macros, function-pointer calls and names outside the pattern are not followed."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HOST = ROOT / "port/ios/host"
FORBIDDEN = re.compile(r"\bSDL_\w*(?:Gamepad|Joystick)\w*|\bhost_main_sync\w*|\bdispatch_sync\w*")
REGISTER = re.compile(r"\bSDL_(?:AddEventWatch|SetEventFilter)\s*\(\s*(\w+)")
DEFINITION = re.compile(r"^\w[^\n;{}]*\b(\w+)\([^;{}]*\)\s*\{", re.MULTILINE)


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


def violations(text: str, others: dict[str, str] | None = None) -> list[str]:
    """Callbacks registered in text that reach a forbidden call through functions defined in it or in others."""
    text = strip_comments(text)
    defined = dict(others or {})
    defined.update(functions(text))
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
            pending += [word for word in set(re.findall(r"\b(\w+)\s*\(", defined[name])) if word in defined and word not in seen]
    return problems


def test_no_event_callback_reaches_an_sdl_joystick_function():
    problems = []
    files = sorted(path for pattern in ("*.c", "*.m", "*.mm") for path in HOST.glob(pattern))
    everything = {}
    for path in files:
        everything.update(functions(strip_comments(path.read_text())))
    for path in files:
        problems += [f"{path.name}: {problem}" for problem in violations(path.read_text(), everything)]
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
    # the callback only blocks on the main thread
    sync = "static bool SDLCALL w(void *u, SDL_Event *e) { host_main_sync_f(0, 0); return 1; }\nvoid s(void) { SDL_AddEventWatch(w, 0); }"
    assert violations(sync) == ["w reaches host_main_sync_f through w"]
    # a helper in another file
    assert violations("void s(void) { SDL_AddEventWatch(w, 0); }", functions("static bool w(void *u, SDL_Event *e) { return SDL_IsGamepad(1); }")) == [
        "w reaches SDL_IsGamepad through w"]


def test_the_callbacks_are_found_in_the_host():
    registered = {name for path in HOST.glob("*.[cm]") for name in REGISTER.findall(strip_comments(path.read_text()))}
    assert {"remote_event", "text_prompt_watch", "controller_input_watch"} <= registered
