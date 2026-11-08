from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).parent / 'perf_compare'))
import consolewin  # noqa: E402
import windows  # noqa: E402

WINDOWS = windows.load()


class FakeGame:
    """A console whose replies come from a table, on a clock that jumps instead of waiting."""

    def __init__(self, replies=None, prints=True, timed=()):
        self.t = 0.0
        self.replies = replies or {}
        self.prints = prints
        self.timed = sorted(timed)  # (seconds, text) the game prints on its own
        self.console = consolewin.Console(self.on_send)
        self.events = []
        self.sent_at = []

    # clock
    def now(self):
        return self.t

    def sleep(self, seconds):
        self.t += seconds
        while self.timed and self.timed[0][0] <= self.t:
            self.console.feed(self.timed.pop(0)[1])

    def on_send(self, line):
        self.sent_at.append((self.t, line))
        reply = self.replies(line, self.t) if callable(self.replies) else self.replies.get(line)
        self.console.feed(line + '\r\n')  # the echo
        if reply is not None and self.prints:
            self.console.feed(f'{reply}\r\n')


class Hooks(consolewin.Hooks):
    def __init__(self, game):
        self.game, self.log = game, []

    def prepare(self):
        self.log.append(('prepare', self.game.t))

    def begin(self):
        self.log.append(('begin', self.game.t))

    def mid(self):
        self.log.append(('mid', self.game.t))

    def end(self):
        self.log.append(('end', self.game.t))


def test_seen_matches_the_lowercase_print_not_the_echo():
    console = consolewin.Console(lambda line: None)
    console.feed('(begin (x70_bridge) (print "XPDONE"))\r\n')
    assert not console.seen('xpdone')
    console.feed('xpdone\r\n')
    assert console.seen('XPDONE')
    assert not console.seen('xpdone', console.mark())


def test_query_returns_the_value_after_the_echo_or_none_when_the_console_is_silent():
    game = FakeGame({'(game_time)': '1234'})
    assert consolewin.number(game.console.query('(game_time)', game)) == 1234
    quiet = FakeGame({'(game_time)': '1234'}, prints=False)
    assert quiet.console.query('(game_time)', quiet, timeout=1.0) is None


def test_battle_sends_deathless_then_commands_warms_up_then_samples_and_reads_counts():
    window = WINDOWS['b40-exta']
    game = FakeGame(lambda line, t: {'(ai_living_count ext_a_area_a_cov)': '40' if t < 60 else '12',
                                     '(ai_living_count ext_a_area_a_marines)': '5',
                                     '(unit_get_health (unit (list_get (players) 0)))': '1',
                                     '(game_time)': str(int(t * 30))}.get(line))
    hooks = Hooks(game)
    out = consolewin.run_window(window, game.console, hooks, game)
    sent = [line for _, line in game.sent_at]
    assert sent[0] == '(set cheat_deathless_player true)' and sent[1:7] == window['commands']
    assert [name for name, _ in hooks.log] == ['prepare', 'begin', 'mid', 'end']
    _, begin, mid, end = (t for _, t in hooks.log)
    assert begin - game.sent_at[6][0] >= window['warmup_s']
    assert end - begin == pytest.approx(60) and mid - begin == pytest.approx(30)
    assert out['start']['(ai_living_count ext_a_area_a_cov)'] == '40'
    assert out['end']['(ai_living_count ext_a_area_a_cov)'] == '12'
    assert out['seconds'] == pytest.approx(60)


def test_cutscene_ends_on_the_print_and_calls_the_script_after_the_lead_in():
    window = WINDOWS['x70-bridge']
    game = FakeGame({'(game_time)': '100'}, timed=[(20 + 126.3, 'xpdone\r\n')])
    hooks = Hooks(game)
    out = consolewin.run_window(window, game.console, hooks, game)
    call = [(t, line) for t, line in game.sent_at if 'x70_bridge' in line]
    assert call and call[0][1] == '(begin (sleep_until (>= (game_time) 2500) 1) (x70_bridge) (print "XPDONE"))' and call[0][0] >= 20
    assert out['ended'] == 'print' and out['seconds'] == pytest.approx(126.3, abs=0.2)
    assert [name for name, _ in hooks.log] == ['prepare', 'begin', 'mid', 'end']


def test_cutscene_without_a_print_times_out_at_one_and_a_half_times_the_expected_length():
    window = WINDOWS['cutscene-rescue']
    game = FakeGame({'(game_time)': '100'})
    out = consolewin.run_window(window, game.console, Hooks(game), game)
    assert out['ended'] == 'timeout' and out['seconds'] >= 1.5 * 3180 / 30


def test_cutscene_on_a_silent_console_falls_back_to_the_wall_clock():
    window = WINDOWS['x70-bridge']
    game = FakeGame({}, prints=False)
    out = consolewin.run_window(window, game.console, Hooks(game), game, prints=False)
    assert out['ended'] == 'wall-clock' and out['start_tick'] is None


def test_cutscene_without_prints_ends_when_game_time_reaches_the_expected_tick():
    window = WINDOWS['x70-bridge']
    game = FakeGame(lambda line, t: str(int((t - 20) * 30) + 100) if line == '(game_time)' else None, prints=True)
    out = consolewin.run_window(window, game.console, Hooks(game), game, prints=False)
    assert out['ended'] == 'ticks' and out['seconds'] == pytest.approx((window['start_tick'] + window['expected_ticks'] - 100) / 30, abs=1.0)


def test_bot_window_runs_from_start_to_end_print_and_reads_fingerprints_at_both_ends():
    window = WINDOWS['b30-bot']
    game = FakeGame({'(game_time)': '900', '(unit_get_health (unit (list_get (players) 0)))': '0.9',
                     '(unit_get_total_grenade_count (unit (list_get (players) 0)))': '3'},
                    timed=[(30, 'xp_start\r\n'), (90, 'xp_end\r\n')])
    hooks = Hooks(game)
    out = consolewin.run_window(window, game.console, hooks, game)
    assert [name for name, _ in hooks.log] == ['prepare', 'begin', 'mid', 'end']
    assert out['ended'] == 'print' and out['seconds'] == pytest.approx(60, abs=0.5)
    assert out['start']['(game_time)'] == '900' and out['end']['(unit_get_total_grenade_count (unit (list_get (players) 0)))'] == '3'
    schedule = [line for _, line in game.sent_at if 'sleep_until' in line][0]
    assert '(>= (game_time) 900)' in schedule and '(sleep 1800)' in schedule and '(game_speed 0)' in schedule


def test_bot_window_that_never_starts_is_reported():
    game = FakeGame({'(game_time)': '1'})
    out = consolewin.run_window(WINDOWS['b30-bot'], game.console, Hooks(game), game)
    assert out['ended'] == 'never-started'


def test_opening_is_not_a_console_window():
    game = FakeGame()
    with pytest.raises(ValueError):
        consolewin.run_window(WINDOWS['c10'], game.console, Hooks(game), game)


def test_numeric_query_skips_echo_fragments_of_earlier_input():
    game = FakeGame(lambda line, t: 'ist_count (players)) 0) (print "PERF_READY"))\r\n2' if line == '(list_count (players))' else None)
    assert game.console.query('(list_count (players))', game, numeric=True) == '2'


def test_sync_waits_for_the_token_after_queued_echoes():
    game = FakeGame(timed=[(3.0, 'xpsync\r\n')])
    assert game.console.sync(game, timeout=10) is True and game.t >= 3.0
    assert FakeGame().console.sync(FakeGame(), timeout=1) is False


def test_hosted_session_warms_up_samples_and_reads_players_and_time_at_both_ends():
    window = WINDOWS['mp-chillout']
    game = FakeGame(lambda line, t: {'(list_count (players))': '1', '(game_time)': str(int(t * 30)), '(print "XPSYNC")': 'xpsync'}.get(line))
    hooks = Hooks(game)
    out = consolewin.run_window(window, game.console, hooks, game)
    assert [name for name, _ in hooks.log] == ['prepare', 'begin', 'mid', 'end']
    assert hooks.log[0][1] == 15 and out['seconds'] == pytest.approx(60, abs=0.5)
    assert out['start']['(list_count (players))'] == '1' and int(out['end']['(game_time)']) > int(out['start']['(game_time)'])
    assert out['kind'] == 'match'


def test_hosted_settings_per_port():
    import adapters
    match, coop = WINDOWS['mp-chillout'], WINDOWS['coop-c10']
    assert adapters.hosted_spec(match) == 'host:chillout'
    assert adapters.hosted_spec(coop) == 'host:levels\\c10\\c10:coop'
    debug, network, env = adapters.hosted_settings('visr', coop)
    assert 'network_test = "host:levels\\\\c10\\\\c10:coop"' in debug  # escaped for TOML
    assert 'enemy_multiplier = 1' in debug and 'network_test_players = 2' in debug and not env
    assert network == ['coop_enemies_mode = "none"']  # standard enemy settings, as in the other ports
    assert not any('test_input' in line for line in debug)  # co-op takes no input
    debug, network, env = adapters.hosted_settings('opence-macos', coop)
    assert network == ['coop_enemies_mode = "none"'] and 'enemy_multiplier = 1' not in debug
    debug, network, env = adapters.hosted_settings('halo-og', match)
    assert 'test_input = "bot:1"' in debug and not network
    debug, network, env = adapters.hosted_settings('chupathingy', coop)
    assert env['HALO_NETWORK_TEST_LOCAL_PLAYERS'] == '2' and env['HALO_NET_COOP_ENEMIES_MODE'] == 'none' and not debug
    assert adapters.hosted_settings('chupathingy', match)[2]['HALO_START_WITH_ONE_PLAYER'] == '1'


class ShotHooks(Hooks):
    takes_shots = True

    def shot(self):
        self.log.append(('shot', self.game.t))


def test_the_screenshot_pass_runs_after_the_window_has_closed():
    window = WINDOWS['b40-exta']
    game = FakeGame(lambda line, t: {'(game_time)': str(int(t * 30))}.get(line))
    hooks = ShotHooks(game)
    consolewin.run_window(window, game.console, hooks, game)
    names = [name for name, _ in hooks.log]
    assert names == ['prepare', 'begin', 'mid', 'end', 'shot']
    assert hooks.log[-1][1] >= hooks.log[-2][1]


def test_a_cutscene_screenshot_replays_the_script_after_the_window_and_captures_its_middle():
    window = WINDOWS['x70-bridge']
    game = FakeGame({'(game_time)': '100'}, timed=[(20 + 126.3, 'xpdone\r\n')])
    hooks = ShotHooks(game)
    consolewin.run_window(window, game.console, hooks, game)
    end_at, shot_at = dict(hooks.log)['end'], dict(hooks.log)['shot']
    replay = [t for t, line in game.sent_at if line == '(begin (x70_bridge) (print "XPSHOT"))']
    assert replay and replay[0] >= end_at and shot_at - replay[0] == pytest.approx(8.0)


def test_without_the_screenshot_pass_nothing_is_replayed_or_captured():
    window = WINDOWS['x70-bridge']
    game = FakeGame({'(game_time)': '100'}, timed=[(20 + 126.3, 'xpdone\r\n')])
    hooks = Hooks(game)
    consolewin.run_window(window, game.console, hooks, game)
    assert not any('XPSHOT' in line for _, line in game.sent_at)
    assert [name for name, _ in hooks.log] == ['prepare', 'begin', 'mid', 'end']


POSE = [1.0, 2.0, 3.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.2]
PLAYER_VIEW = [5.0, 6.0, 7.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0]


class CameraFile(consolewin.ShotHooks):
    """camera.txt as a port sees it, plus the captures taken."""

    def __init__(self, game, harvesting=False):
        self.game, self.harvesting, self.text, self.shots = game, harvesting, None, []

    def place(self, camera):
        self.text = consolewin.camera_text(camera)

    def clear(self):
        self.text = None

    def harvest(self):
        return self.text

    def readback(self):
        return self.text

    def shot(self, name):
        self.shots.append((name, self.game.t))


def shot_game(window, now=100, camera_commands=True, prints=True, freezes=True):
    """A game at tick `now` that freezes on the shot tick once told to, and whose debug camera commands work
    unless `camera_commands` is off."""
    state = {'frozen': False, 'view': list(PLAYER_VIEW)}
    start_t = [None]
    files = []

    def tick(t):
        ticks = now + int(t * 30)
        target = window['shot']['tick']
        return min(ticks, target) if state['frozen'] else ticks

    def reply(line, t):
        if line == '(print "XPSYNC")':
            return 'xpsync' if prints else None  # CoreCE and Chupathingy print nothing; numbers still come back
        if line == '(game_time)':
            return str(tick(t))
        if line.endswith('(game_speed 0))') and freezes:
            state['frozen'] = True
        if camera_commands and line == '(debug_camera_load)' and files[0].text:
            state['view'] = consolewin.parse_camera(files[0].text)
        if camera_commands and line == '(debug_camera_save)':
            files[0].text = consolewin.camera_text(state['view'])
        return None

    game = FakeGame(reply)
    files.append(None)
    return game, files, start_t


def run_shot_with(window, harvesting=False, **kwargs):
    game, files, _ = shot_game(window, **kwargs)
    hooks = CameraFile(game, harvesting)
    files[0] = hooks
    out = consolewin.run_shot(window, game.console, hooks, game, prints=kwargs.get('prints', True))
    return out, hooks, [line for _, line in game.sent_at]


def with_camera(window, camera=POSE):
    return dict(window, shot=dict(window['shot'], camera=camera))


def test_every_window_has_a_shot_tick_after_its_start():
    for key, window in WINDOWS.items():
        shot = window.get('shot')
        assert shot, key
        start = consolewin.shot_start(window)
        assert start is None or shot['tick'] > start, key


def test_shot_lines_fire_commands_and_scripts_on_a_tick_and_freeze_on_the_shot_tick():
    battle = WINDOWS['b40-exta']
    lines = consolewin.shot_lines(battle)
    assert lines[0] == '(set cheat_deathless_player true)'
    assert lines[1].startswith('(begin (sleep_until (>= (game_time) 600) 1) ' + battle['commands'][0])
    assert lines[-1] == '(begin (sleep_until (>= (game_time) 1050) 1) (game_speed 0))'
    cutscene = WINDOWS['cutscene-rescue']
    assert consolewin.shot_lines(cutscene)[0] == f'(begin (sleep_until (>= (game_time) 900) 1) ({cutscene["script"]}))'
    assert consolewin.shot_lines(WINDOWS['mp-chillout']) == ['(begin (sleep_until (>= (game_time) 900) 1) (game_speed 0))']


def test_camera_text_round_trips_and_rejects_other_text():
    assert consolewin.parse_camera(consolewin.camera_text(POSE)) == pytest.approx(POSE)
    assert consolewin.parse_camera('1 2 3') is None and consolewin.parse_camera(None) is None
    assert consolewin.parse_camera('a b c d e f g h i j') is None
    assert consolewin.same_camera(POSE, [v + 0.0005 for v in POSE]) and not consolewin.same_camera(POSE, PLAYER_VIEW)


def test_a_shot_freezes_on_its_tick_places_the_pose_hides_the_hud_and_captures_twice():
    window = with_camera(WINDOWS['mp-chillout'])
    out, hooks, sent = run_shot_with(window)
    assert out['ended'] == 'frozen' and out['tick'] == 900 and out['camera'] == 'placed'
    assert '(show_hud 0)' in sent and '(show_hud_help_text 0)' in sent
    assert sent.index('(debug_camera_load)') < sent.index('(cls)')
    assert [name for name, _ in hooks.shots] == ['shot.png', 'shot-check.png']
    cls_at = [t for t, line in hooks.game.sent_at if line == '(cls)'][0]
    assert hooks.shots[0][1] - cls_at == pytest.approx(consolewin.SHOT_SETTLE_S)


def test_a_port_without_the_camera_commands_is_marked_not_placed():
    out, _, _ = run_shot_with(with_camera(WINDOWS['mp-chillout']), camera_commands=False)
    assert out['camera'] == 'not-placed'


def test_harvesting_takes_the_players_view_and_then_places_it_like_a_stored_pose():
    out, hooks, sent = run_shot_with(WINDOWS['mp-chillout'], harvesting=True)
    assert out['harvested'] == pytest.approx(PLAYER_VIEW) and out['camera'] == 'placed'
    assert sent.count('(debug_camera_save)') == 2 and '(debug_camera_load)' in sent


def test_without_a_pose_or_harvest_the_shot_is_taken_without_a_camera_verdict():
    out, hooks, sent = run_shot_with(WINDOWS['mp-chillout'])
    assert out['ended'] == 'frozen' and 'camera' not in out and '(debug_camera_load)' not in sent
    assert len(hooks.shots) == 2


def test_a_game_already_past_the_start_is_late_and_takes_no_shot():
    out, hooks, sent = run_shot_with(with_camera(WINDOWS['a50-hangar']), now=700)
    assert out['ended'] == 'late' and not hooks.shots
    assert not any('game_speed' in line for line in sent)


def test_a_game_that_never_freezes_takes_no_shot():
    out, hooks, _ = run_shot_with(with_camera(WINDOWS['mp-chillout']), freezes=False)
    assert out['ended'] == 'never-froze' and not hooks.shots


def test_a_console_that_does_not_print_skips_the_sync_but_still_freezes_by_tick():
    out, hooks, sent = run_shot_with(with_camera(WINDOWS['mp-chillout']), prints=False)
    assert '(print "XPSYNC")' not in sent
    assert out['ended'] == 'frozen' and out['camera'] == 'placed' and len(hooks.shots) == 2
