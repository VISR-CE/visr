"""Console-driven sampling windows: battles, cutscenes and the scripted-bot window.

The game's telnet console echoes each expression and then prints the value on its own line, lowercased. Everything
here talks to it through `Console`, and measures through the `Hooks` the Mac adapter supplies, so the window logic
runs against a recorded transcript in tests.
"""
from __future__ import annotations

import socket
import threading
import time

TICKS_PER_SECOND = 30
CUTSCENE_GRACE = 1.5  # a cutscene must finish within this multiple of its expected length


class RealClock:
    def now(self):
        return time.monotonic()

    def sleep(self, seconds):
        time.sleep(seconds)


class Console:
    """A telnet console session: a reader thread fills a buffer; `send` writes one line."""

    def __init__(self, send, initial=''):
        self._send = send
        self.buf = initial
        self.lock = threading.Lock()
        self.sent = []

    @classmethod
    def attach(cls, connection: socket.socket, initial=''):
        """Wrap a connected socket and start reading it."""
        connection.settimeout(None)
        console = cls(lambda line: connection.sendall((line + '\n').encode('latin1')), initial)

        def read():
            while True:
                try:
                    chunk = connection.recv(65536)
                except OSError:
                    return
                if not chunk:
                    return
                console.feed(chunk.decode('latin1'))
        threading.Thread(target=read, daemon=True).start()
        return console

    def feed(self, text):
        with self.lock:
            self.buf += text

    def send(self, line):
        self.sent.append(line)
        self._send(line)

    def mark(self):
        with self.lock:
            return len(self.buf)

    def lines_since(self, mark=0, partial=False):
        """Lines after the mark, stripped. A line still being written is left out unless `partial`: a print
        arrives without a trailing newline (the game prints `\\r\\nxpdone`)."""
        with self.lock:
            text = self.buf[mark:]
        lines = text.replace('\r', '').split('\n')
        return [line.strip() for line in (lines if partial else lines[:-1])]

    def seen(self, token, since=0):
        """True once the console printed `token` on a line of its own. Output is lowercase; the echo of the
        expression that prints it (which contains the token inside quotes) never matches."""
        want = token.lower()
        return any(line.strip('"') == want for line in self.lines_since(since, partial=True))

    def sync(self, clock, timeout=45.0):
        """Wait for the console to work through what is queued (the readiness probes the adapter sent while waiting
        for a player): print a token and wait for it, so the next query's reply is not an old echo. True if it came."""
        mark = self.mark()
        self.send('(print "XPSYNC")')
        deadline = clock.now() + timeout
        while clock.now() < deadline:
            if self.seen('xpsync', mark):
                return True
            clock.sleep(0.1)
        return False

    def query(self, expression, clock, timeout=3.0, numeric=False):
        """Send an expression and return the value line, or None when the console printed nothing back. With
        `numeric`, lines that are not numbers (echo fragments of earlier input) are skipped."""
        mark = self.mark()
        self.send(expression)
        deadline = clock.now() + timeout
        while True:
            replies = [line for line in self.lines_since(mark) if line and not line.startswith('(')
                       and (not numeric or number(line) is not None)]
            if replies:
                return replies[0]
            if clock.now() >= deadline:
                return None
            clock.sleep(0.05)


def number(text):
    """The leading number of a console reply, or None."""
    try:
        return float(str(text).split()[0])
    except (ValueError, IndexError):
        return None


class Hooks:
    """What the adapter measures. `prepare` does the slow setup (trace setup, its settle wait, the background CPU
    baseline) while the level runs, before the window opens; `begin` only marks the window's start, `mid` takes the
    mid-window screenshot and `end` closes the window and collects the trace. Each returns nothing.

    `shot` takes the scene's screenshot for the public page. It runs after `end` has returned, with the trace
    collected and the window closed, so a capture can never change a timing. A cutscene replays its script first,
    because the original has already faded out by then."""

    takes_shots = False

    def prepare(self): ...
    def begin(self): ...
    def mid(self): ...
    def end(self): ...
    def shot(self): ...


def _untimed_shot(hooks):
    if getattr(hooks, 'takes_shots', False):
        hooks.shot()


def _read_all(console, expressions, clock, numeric=False):
    timeout = 10.0 if numeric else 3.0
    return {expression: console.query(expression, clock, timeout=timeout, numeric=numeric) for expression in expressions}


def _living(window):
    return [f'(ai_living_count {name})' for name in window.get('encounters', [])]


def run_battle(window, console, hooks, clock, lead_in=10.0):
    """Deathless player, one batch of commands, a warm-up, then the sample; counts are read before and after."""
    clock.sleep(lead_in)
    hooks.prepare()
    console.send('(set cheat_deathless_player true)')
    for command in window['commands']:
        console.send(command)
        clock.sleep(0.2)
    clock.sleep(window['warmup_s'])
    start_reads = _read_all(console, _living(window) + ['(game_time)', '(unit_get_health (unit (list_get (players) 0)))'], clock)
    hooks.begin()
    started = clock.now()
    half = window['sample_s'] / 2
    clock.sleep(half)
    hooks.mid()
    clock.sleep(max(0.0, window['sample_s'] - (clock.now() - started)))
    seconds = clock.now() - started
    # Read the end counts before the trace is collected (which takes tens of seconds), so they match the window.
    end_reads = _read_all(console, window['fingerprints'] + ['(game_time)'], clock)
    hooks.end()
    _untimed_shot(hooks)
    return {'kind': 'battle', 'seconds': seconds, 'start': start_reads, 'end': end_reads, 'ended': 'time'}


def run_cutscene(window, console, hooks, clock, prints=True, lead_in=20.0):
    """Call the script from the console; sample until its print arrives (or its tick budget passes)."""
    clock.sleep(lead_in)
    hooks.prepare()
    expected = window['expected_ticks']
    start_tick = number(console.query('(game_time)', clock))
    wait_ticks = max(0, window.get('start_tick', 0) - (start_tick or 0))
    budget = (expected + wait_ticks) / TICKS_PER_SECOND
    mark = console.mark()
    wait = f'(sleep_until (>= (game_time) {window["start_tick"]}) 1) ' if 'start_tick' in window else ''
    console.send(f'(begin {wait}({window["script"]}) (print "XPDONE"))')
    hooks.begin()
    started = clock.now()
    shot_done, ended = False, 'timeout'
    while clock.now() - started < budget * CUTSCENE_GRACE:
        elapsed = clock.now() - started
        if not shot_done and elapsed >= min(8.0, budget / 2):
            hooks.mid()
            shot_done = True
        if prints and console.seen('xpdone', mark):
            ended = 'print'
            break
        if not prints and start_tick is not None:
            now_tick = number(console.query('(game_time)', clock, timeout=1.0))
            if now_tick is not None and now_tick >= start_tick + wait_ticks + expected:
                ended = 'ticks'
                break
        clock.sleep(0.1)
    seconds = clock.now() - started
    if not shot_done:
        hooks.mid()
    end_tick = number(console.query('(game_time)', clock, timeout=1.0))
    hooks.end()
    if getattr(hooks, 'takes_shots', False):
        # The scene has ended: play it again and capture its middle, after the timed window.
        console.send(f'(begin ({window["script"]}) (print "XPSHOT"))')
        clock.sleep(min(8.0, budget / 2))
        hooks.shot()
    if not prints and ended == 'timeout' and start_tick is None:
        # Nothing readable from this console: the window is the expected length on the wall clock.
        ended = 'wall-clock'
    return {'kind': 'cutscene', 'seconds': seconds, 'ended': ended, 'expected_ticks': expected,
            'start_tick': start_tick, 'end_tick': end_tick, 'start': {}, 'end': {}}


def run_bot(window, console, hooks, clock, prints=True):
    """The scripted player for `ticks` game ticks from `start_tick`; the game freezes at the end so reads are exact."""
    t0, ticks = window['start_tick'], window['ticks']
    hooks.prepare()
    console.send('(set cheat_deathless_player true)')
    mark = console.mark()
    console.send(f'(begin (sleep_until (>= (game_time) {t0}) 1) (print "XP_START") (sleep {ticks}) (print "XP_END") (game_speed 0))')
    started = clock.now()
    limit = (t0 + ticks) / TICKS_PER_SECOND * CUTSCENE_GRACE + 30
    begun = False
    start_reads, ended = {}, 'timeout'
    shot_done = False
    began_at = None
    while clock.now() - started < limit:
        tick = None if prints else number(console.query('(game_time)', clock, timeout=0.5))
        if not begun and (console.seen('xp_start', mark) if prints else tick is not None and tick >= t0):
            start_reads = _read_all(console, window['fingerprints'], clock)
            began_at = clock.now()
            hooks.begin()
            begun = True
        if begun and not shot_done and clock.now() - began_at >= ticks / TICKS_PER_SECOND / 2:
            hooks.mid()
            shot_done = True
        if begun and (console.seen('xp_end', mark) if prints else tick is not None and tick >= t0 + ticks):
            ended = 'print'
            break
        clock.sleep(0.1)
    if not begun:
        return {'kind': 'bot', 'seconds': 0.0, 'ended': 'never-started', 'start': {}, 'end': {}}
    seconds = clock.now() - began_at
    hooks.end()
    _untimed_shot(hooks)
    end_reads = _read_all(console, window['fingerprints'], clock)
    return {'kind': 'bot', 'seconds': seconds, 'ended': ended, 'start': start_reads, 'end': end_reads}


def run_session(window, console, hooks, clock):
    """A hosted game (a match or split-screen co-op) that is already running: warm up after player readiness, then
    sample. The player count and game time are read at both ends of the window for the content checks."""
    # The readiness probes the adapter sent while waiting for a player are still queued, and the game answers
    # about one console line per tick: drain them during the warm-up so the counts read at the window's ends are current.
    opened = clock.now()
    console.sync(clock, timeout=150.0)
    clock.sleep(max(0.0, window['warmup_s'] - (clock.now() - opened)))
    hooks.prepare()
    start_reads = _read_all(console, window['fingerprints'], clock, numeric=True)
    hooks.begin()
    started = clock.now()
    clock.sleep(window['sample_s'] / 2)
    hooks.mid()
    clock.sleep(max(0.0, window['sample_s'] - (clock.now() - started)))
    seconds = clock.now() - started
    end_reads = _read_all(console, window['fingerprints'], clock, numeric=True)
    hooks.end()
    _untimed_shot(hooks)
    return {'kind': window['kind'], 'seconds': seconds, 'start': start_reads, 'end': end_reads, 'ended': 'time'}


class ShotHooks:
    """What a shots-only run needs from the adapter. `place` writes a camera pose where the port's
    `debug_camera_load` reads it, `clear` removes those files again, `harvest` keeps the pose `debug_camera_save`
    wrote, `readback` returns the pose the port saved after loading (None when it wrote nothing) and `shot` captures
    the window under a file name."""

    harvesting = False

    def place(self, camera): ...
    def clear(self): ...
    def harvest(self): ...
    def readback(self): ...
    def shot(self, name): ...


SHOT_SETTLE_S = 5.0  # console text fades and the frozen frame redraws before the capture
SHOT_CHECK_S = 2.0   # a second capture this much later shows whether the view is really still


def shot_start(window):
    """The tick a shot run fires the window's commands or script at."""
    shot = window['shot']
    return shot.get('start', window.get('start_tick'))


def shot_lines(window):
    """Console lines that play a window up to its shot tick in-game, so every port reaches the same game state:
    commands and scripts fire on a fixed tick rather than when the console line arrives, and the game freezes on the
    shot tick."""
    kind, shot = window['kind'], window['shot']
    lines = []
    if kind in ('battle', 'bot'):
        lines.append('(set cheat_deathless_player true)')
    start = shot_start(window)
    if kind == 'battle':
        lines.append(f'(begin (sleep_until (>= (game_time) {start}) 1) ' + ' '.join(window['commands']) + ')')
    elif kind == 'cutscene':
        lines.append(f'(begin (sleep_until (>= (game_time) {start}) 1) ({window["script"]}))')
    lines.append(f'(begin (sleep_until (>= (game_time) {shot["tick"]}) 1) (game_speed 0))')
    return lines


def camera_text(camera):
    """A pose as `debug_camera_load` reads it: position, forward, up, field of view."""
    rows = (camera[0:3], camera[3:6], camera[6:9], camera[9:10])
    return ''.join(' '.join(f'{value:f}' for value in row) + '\n' for row in rows)


def parse_camera(text):
    """The ten numbers of a saved pose, or None when the text is not one."""
    try:
        values = [float(item) for item in text.split()]
    except (AttributeError, ValueError):
        return None
    return values if len(values) == 10 else None


def same_camera(a, b, position=0.01, other=0.001):
    return a is not None and b is not None and all(
        abs(x - y) <= (position if index < 3 else other) for index, (x, y) in enumerate(zip(a, b)))


def run_shot(window, console, hooks, clock, prints=True):
    """Shots-only run: play to the window's shot tick, freeze, place the stored camera, hide the HUD, capture.

    No timing is taken. The result says which tick the game froze on and whether the port took the pose; the
    publisher drops any frame that did not."""
    target = window['shot']['tick']
    if prints:
        console.sync(clock, timeout=150.0)
    else:
        clock.sleep(5.0)  # a console that prints nothing cannot sync; its numeric replies still come back
    now = number(console.query('(game_time)', clock, timeout=10.0, numeric=True))
    if now is None and window['kind'] in ('match', 'coop'):
        # A hosted game starts a few seconds after launch, and on some ports the console answers only once it runs.
        for _ in range(24):
            clock.sleep(5.0)
            now = number(console.query('(game_time)', clock, timeout=10.0, numeric=True))
            if now is not None:
                break
    first = min(target, shot_start(window) or target)
    if now is None or now >= first:
        return {'kind': 'shot', 'ended': 'late' if now is not None else 'no-reply', 'tick': now, 'target_tick': target}
    for line in shot_lines(window):
        console.send(line)
    deadline = clock.now() + (target - now) / TICKS_PER_SECOND * CUTSCENE_GRACE + 60
    previous, tick = None, None
    while clock.now() < deadline:
        tick = number(console.query('(game_time)', clock, timeout=2.0, numeric=True))
        if tick is not None and tick >= target and tick == previous:
            break
        previous = tick
        clock.sleep(1.0)
    else:
        return {'kind': 'shot', 'ended': 'never-froze', 'tick': tick, 'target_tick': target}
    result = {'kind': 'shot', 'ended': 'frozen', 'tick': tick, 'target_tick': target}
    console.send('(show_hud 0)')
    console.send('(show_hud_help_text 0)')
    camera = window['shot'].get('camera')
    if getattr(hooks, 'harvesting', False):
        # The player's own view at the shot tick becomes the pose; it is then placed like any stored pose, so the
        # harvesting port's frame comes from the same flying camera as every other port's.
        hooks.clear()
        console.send('(debug_camera_save)')
        clock.sleep(1.0)
        camera = parse_camera(hooks.harvest())
        result['harvested'] = camera
    if camera:
        hooks.place(camera)
        console.send('(debug_camera_load)')
        clock.sleep(1.0)
        hooks.clear()  # so a port without `debug_camera_save` cannot pass by leaving our own file in place
        console.send('(debug_camera_save)')
        clock.sleep(1.0)
        saved = hooks.readback()
        result['camera'] = 'placed' if same_camera(parse_camera(saved), camera) else 'not-placed'
    console.send('(cls)')
    clock.sleep(SHOT_SETTLE_S)
    hooks.shot('shot.png')
    clock.sleep(SHOT_CHECK_S)
    hooks.shot('shot-check.png')
    return result


def run_window(window, console, hooks, clock=None, prints=True):
    clock = clock or RealClock()
    kind = window['kind']
    if kind == 'battle':
        return run_battle(window, console, hooks, clock)
    if kind == 'cutscene':
        return run_cutscene(window, console, hooks, clock, prints)
    if kind == 'bot':
        return run_bot(window, console, hooks, clock, prints)
    if kind in ('match', 'coop'):
        return run_session(window, console, hooks, clock)
    raise ValueError('not a console window: ' + kind)
