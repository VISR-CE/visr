"""Audio Setup's rows (#146): Sound first, then the volumes and Music, each applied live."""
import re
from pathlib import Path

import port_settings

ROOT = Path(__file__).resolve().parent.parent


def test_audio_rows_are_in_the_order_of_their_dependence():
    rows = [setting for _, setting, *_ in port_settings.SCREENS['audio_settings']['rows']] \
        if hasattr(port_settings, 'SCREENS') else None
    if rows is None:
        text = (ROOT / 'tools/port_settings.py').read_text()
        rows = re.findall(r'\("[A-Z ]+:", "(audio\.[a-z_]+)"', text)
    assert rows == ['audio.enabled', 'audio.volume', 'audio.effects_volume', 'audio.music_enabled',
                    'audio.music_volume']


def test_menu_code_hides_rows_by_state_and_help_no_longer_says_restart():
    code = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    assert 'audio_rows_show(widget)' in code
    assert 'op_music_volume' in code
    text = (ROOT / 'tools/port_settings.py').read_text()
    assert 'Play sound at all' not in text


def test_music_off_wiring():
    root = Path(__file__).resolve().parent.parent
    sound = (root / "source/sound/sound_manager.c").read_text()
    assert 'config_boolean("audio.music_enabled")' in sound
    dsound = (root / "port/linux/src/dsound_sdl.c").read_text()
    assert "audio_apply_category" in dsound and "audio_music_open" in dsound
    host = (root / "port/ios/host/host_external.m").read_text()
    assert "host_sdl_audio_any_open()" in host


def test_music_off_mixes_without_ducking():
    root = Path(__file__).resolve().parent.parent
    dsound = (root / "port/linux/src/dsound_sdl.c").read_text()
    assert '"VISR_AUDIO_MIX_ONLY"' in dsound and "SDL_AUDIO_CATEGORY" not in dsound
    session = (root / "port/ios/host/host_audio_session.m").read_text()
    assert "AVAudioSessionCategoryPlayback" in session
    assert "AVAudioSessionCategoryOptionMixWithOthers" in session
    assert "DuckOthers" not in session and "AVAudioSessionCategoryAmbient" not in session
    host = (root / "port/ios/host/host_sdl.c").read_text()
    assert host.count("host_audio_session_apply();") >= 2


def test_last_close_notifies_others():
    root = Path(__file__).resolve().parent.parent
    session = (root / "port/ios/host/host_audio_session.m").read_text()
    assert "AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation" in session
    host = (root / "port/ios/host/host_sdl.c").read_text()
    assert "host_audio_session_release();" in host
    cmake = (root / "port/ios/CMakeLists.txt").read_text()
    assert "host/host_audio_session.m" in cmake


def test_audio_cycle_wiring():
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    assert '"debug.audio_cycle"' in config and "int config_set_boolean_unsaved" in config
    assert "debug.audio_cycle" in (ROOT / "port/linux/README.md").read_text()
    dsound = (ROOT / "port/linux/src/dsound_sdl.c").read_text()
    assert "void audio_cycle_step(void)" in dsound
    assert "audio_cycle_step();" in (ROOT / "port/linux/src/d3d8_device.c").read_text()
    host = (ROOT / "port/ios/host/host_sdl.c").read_text()
    assert "int host_sdl_audio_open_count(void)" in host and "int host_thread_count(void)" in host


def test_committed_audio_menus_match_generator():
    import re
    import port_settings
    expected = [row[0] for row in port_settings.SCREENS["audio_settings"]["rows"]] if "audio_settings" in port_settings.SCREENS else None
    assert expected, "audio_settings screen missing from port_settings.SCREENS"
    for path in [ROOT / "port/assets/menus/xbox/settings.audio_settings.xml"]:
        text = path.read_text()
        labels = [t for t in re.findall(r'<string text="([^"]*:)"', text) if t in expected]
        assert labels == expected, path.name
