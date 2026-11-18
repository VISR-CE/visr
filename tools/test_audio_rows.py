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
