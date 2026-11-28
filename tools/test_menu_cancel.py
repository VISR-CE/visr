"""Run the real widget event dispatcher against the shipped menu handlers.

Removing the root's inert cancel handlers must reproduce the empty UI-map
screen. The tag loader and game boundaries are replaced, not cancel dispatch,
history navigation, or handler dispatch. No game data or GPU is required.
"""
from pathlib import Path
import runpy
import subprocess
from xml.etree import ElementTree

from probekit import source as probe_source

ROOT = Path(__file__).resolve().parents[1]


def block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def test_settings_back_keeps_changes_and_video_applies_on_close():
    menus = ROOT / 'port/assets/menus/xbox'
    for path in menus.glob('settings.*.xml'):
        root = ElementTree.parse(path).getroot()
        screen = root.find('widget')
        for event in ('b', 'back'):
            # Back only closes; it must neither discard edits nor resave the
            # whole page (which could reopen the Internet Play question).
            on = screen.find(f'on[@event="{event}"]')
            assert on.attrib == {'event': event, 'back': 'true'}
        close = screen.find('on[@event="deleted"]')
        if 'video_settings' in path.name:
            assert close is not None and close.get('run') == 'port settings close'
        else:
            assert close is None





def test_native_profile_back_saves_and_confirmed_names_save():
    source = (ROOT / 'source/interface/ui_widget_event_handler_functions.c').read_text()
    handler = probe_source.function_in(source, 'player_profile_end_editing', 'ui_widget_event_handler_functions.c')
    assert handler.index('pc_menu_profile_save_on_back()') < handler.index('player_ui_end_editing_profile()')
    playlist = probe_source.function_in(source, 'playlist_profile_end_editing', 'ui_widget_event_handler_functions.c')
    assert 'pc_menu_profile_save_on_back' not in playlist
    color = probe_source.function_in(source, 'player_profile_color_picker_select_color', 'ui_widget_event_handler_functions.c')
    assert 'return pc_menu_profile_save_on_back();' in color
    keyboard = (ROOT / 'source/interface/virtual_keyboard.c').read_text()
    done = keyboard[keyboard.index('case _vkey_done:', keyboard.index('static boolean virtual_keyboard_select(\n\tvoid)\n{')):]
    assert 'if (virtual_keyboard_globals.last_exit_saved_text)' in done
    assert 'pc_menu_profile_name_saved(virtual_keyboard_globals.text_buffer)' in done


if __name__ == "__main__":
    test_settings_back_keeps_changes_and_video_applies_on_close()
    test_native_profile_back_saves_and_confirmed_names_save()
