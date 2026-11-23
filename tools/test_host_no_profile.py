"""Create Game with no profile opens the profile screen; it never auto-creates one.

Hosting from Internet or Direct Link on a fresh install finds no profile. The
host-profile screen lists profiles and the empty slot; a chosen profile (or a
named new one) hosts, and B creates nothing.
"""
from pathlib import Path
from test_network_discovery import function

ROOT = Path(__file__).resolve().parents[1]


def test_host_no_profile():
    source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    assert 'multiplayer_profile_create' not in source
    player = function(source, 'static boolean multiplayer_player(short controller)')
    assert 'campaign_fail' in player

    host = function(source, 'static boolean multiplayer_host_mode(')
    assert 'campaign_profile' in host
    assert 'ui_widget_port_open(widget, HOST_PROFILE_NAME' in host
    assert host.index('HOST_PROFILE_NAME') < host.index('multiplayer_host_start')

    # the picker is the game's own (the campaign's), not a screen of the port's
    assert ('HOST_PROFILE_NAME "ui\\\\shell\\\\main_menu\\\\player_profiles_select\\\\'
            'solo_game_player_profile_select_screen"') in source

    poll = function(source, 'void ui_widget_port_host_profile_poll(struct widget_instance *list)\n{')
    assert 'virtual_keyboard_active()' in poll
    assert 'campaign_profile' in poll
    assert 'host_profile_continue' in poll

    cont = function(source, 'static boolean host_profile_continue(')
    assert 'HOST_MAP_SELECT_NAME' in cont
    assert 'multiplayer_host_start' in cont

    # the game's own profile screen is used: no custom screen remains
    assert 'host_profile_screen' not in source
    tags = (ROOT / 'port/linux/game/menu_tags.c').read_text()
    assert 'port host profile' not in tags
    equiv = (ROOT / 'port/linux/include/menu_equivalence.h').read_text()
    assert 'host_profile_screen' not in equiv
    xml = (ROOT / 'port/assets/menus/xbox/main_menu.multiplayer_type_select.lobby.xml').read_text()
    assert 'host_profile' not in xml


if __name__ == '__main__':
    test_host_no_profile()
