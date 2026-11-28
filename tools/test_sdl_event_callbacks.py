"""SDL runs an event watcher or event filter on the thread that pushes the event, holding the event-watch lock. The
game thread pushes joystick and gamepad events (host_controller_refresh), while it holds the joystick lock, so a
callback that reaches an SDL joystick or gamepad function, however indirectly, takes the two locks in the opposite
order of that thread: a hang (the tvOS remote filter did, through text_input_blocked and hardware_held). This finds
every callback registered with SDL_AddEventWatch or SDL_SetEventFilter in port/ios/host and fails when one reaches
such a function (or host_main_sync, which blocks on main while main may wait on the watch lock) through the functions
defined in port/ios/host. Limits: macros, function-pointer calls and names outside the pattern are not followed."""

import re
from pathlib import Path

from probekit import source as probe_source

ROOT = Path(__file__).resolve().parent.parent
HOST = ROOT / "port/ios/host"
FORBIDDEN = re.compile(r"\bSDL_\w*(?:Gamepad|Joystick)\w*|\bhost_main_sync\w*|\bdispatch_sync\w*")
REGISTER = re.compile(r"\bSDL_(?:AddEventWatch|SetEventFilter)\s*\(\s*(\w+)")
DEFINITION = re.compile(r"^\w[^\n;{}]*\b(\w+)\([^;{}]*\)\s*\{", re.MULTILINE)


def strip_comments(text: str) -> str:
    """The text with its comments blanked and its strings kept (probekit.source's one comment splitter)."""
    return probe_source.mask(text, strings=False)


def functions(text: str) -> dict[str, str]:
    """Function name -> body, for the definitions at file scope (brace matched); a name defined more than once
    (the branches of an #if) gets its bodies joined."""
    found = {}
    for match in DEFINITION.finditer(text):
        name, depth, index = match.group(1), 1, match.end()
        if name in ("if", "for", "while", "switch"):
            continue
        while depth and index < len(text):
            depth += {"{": 1, "}": -1}.get(text[index], 0)
            index += 1
        found[name] = found.get(name, "") + text[match.end():index]
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


# A controller read must never wait on the main thread (host_main_thread.h's no-wait zone, which host_sdl.c's gamepad
# entry points open): main can be busy for a long time, and the wait sits just before the game uses what it read.
# The same walk, from those entry points, through every platform's definition of a function (host_ios_gamepad_axis
# and the rest are defined once per platform, so same-named bodies are joined).
PAD_READS = ("host_sdl_get_gamepads", "host_sdl_update_joysticks", "host_sdl_open_gamepad", "host_sdl_gamepad_from_id",
             "host_sdl_gamepad_axis", "host_sdl_gamepad_button", "host_sdl_gamepad_type")
# Waits named here are found in C functions; Objective-C methods and message sends are not followed.
MAIN_WAIT = re.compile(r"\bhost_main_sync\w*|\bdispatch_(?:barrier_)?sync\w*|\bdispatch_(?:semaphore|group)_wait\b|"
                       r"\bperformSelectorOnMainThread\b|\bwaitUntilDone\s*:\s*YES")


def every_definition(paths) -> dict[str, str]:
    """Function name -> the bodies of all its definitions in paths, joined."""
    joined: dict[str, str] = {}
    for path in paths:
        for name, body in functions(strip_comments(path.read_text())).items():
            joined[name] = joined.get(name, "") + "\n" + body
    return joined


def main_waits(defined: dict[str, str], roots) -> list[str]:
    problems = []
    for root in roots:
        seen, pending = set(), [root]
        while pending:
            name = pending.pop()
            if name in seen or name not in defined:
                continue
            seen.add(name)
            problems += [f"{root} reaches {hit} through {name}" for hit in MAIN_WAIT.findall(defined[name])]
            pending += [word for word in set(re.findall(r"\b(\w+)\s*\(", defined[name])) if word in defined]
    return sorted(set(problems))


def test_no_controller_read_waits_on_main():
    paths = sorted(path for pattern in ("*.c", "*.m", "*.mm") for path in HOST.glob(pattern))
    paths += sorted((ROOT / "port/macos").glob("*.m"))
    defined = every_definition(paths)
    assert all(root in defined for root in PAD_READS), "a gamepad entry point moved: update PAD_READS"
    problems = main_waits(defined, PAD_READS)
    assert not problems, "\n".join(problems)


def test_the_tvos_pad_read_shape_is_found():
    """What this would have said about host_tv.m before its pad reads loaded a published gate."""
    tv = """
static bool text_input_blocked(void) {
    if (host_game_thread_enabled() && !host_on_main_thread()) { host_main_sync(^{ text_input_blocked(); }); }
    return false;
}
int host_ios_gamepad_axis(SDL_Gamepad *pad, int axis) { if (text_input_blocked()) return 0; return 1; }
"""
    touch = "int host_ios_gamepad_axis(SDL_Gamepad *pad, int axis) { return SDL_GetGamepadAxis(pad, axis); }"
    entry = "int host_sdl_gamepad_axis(uint32_t gamepad, int axis) { return host_ios_gamepad_axis(0, axis); }"
    defined = {}
    for text in (entry, touch, tv):
        for name, body in functions(strip_comments(text)).items():
            defined[name] = defined.get(name, "") + body
    assert main_waits(defined, ["host_sdl_gamepad_axis"]) == [
        "host_sdl_gamepad_axis reaches host_main_sync through text_input_blocked"]
    clean = tv.replace("host_main_sync(^{ text_input_blocked(); });", "")
    defined = {}
    for text in (entry, touch, clean):
        for name, body in functions(strip_comments(text)).items():
            defined[name] = defined.get(name, "") + body
    assert main_waits(defined, ["host_sdl_gamepad_axis"]) == []


def test_every_branch_of_a_function_is_walked():
    """A function defined in both branches of an #if is walked in both, and the other wait spellings are found."""
    entry = "int host_sdl_gamepad_button(uint32_t gamepad, int button) { return host_window_up(); }"
    branches = """
#if TARGET_OS_TV
int host_window_up(void) { return 0; }
#else
int host_window_up(void) { dispatch_semaphore_wait(done, FOREVER); return 1; }
#endif
"""
    defined = functions(strip_comments(entry + branches))
    assert main_waits(defined, ["host_sdl_gamepad_button"]) == [
        "host_sdl_gamepad_button reaches dispatch_semaphore_wait through host_window_up"]
    for wait in ("dispatch_barrier_sync(queue, ^{});", "dispatch_group_wait(group, FOREVER);",
                 "[object performSelectorOnMainThread:selector withObject:nil waitUntilDone:YES];"):
        defined = functions(strip_comments(entry.replace("return host_window_up();", wait + " return 0;")))
        assert main_waits(defined, ["host_sdl_gamepad_button"]), wait
