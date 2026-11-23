"""Menu titles as text: which widget is each screen's title, and what it says.

A title is a widget the menus' data marks with a `<title>` (spec: menu titles as live text, step 5). The engine draws
it with one look (`HALO_MENU_ROLE_TITLE`, `port/linux/include/halo_menu_meta.h`): its helmet icon, its touch BACK
chevron and its touch shift come from that role, never from the widget's name or picture. Its text is a string list
of the menus, one string for each frame its picture had (a title the disc changes at run time picks a frame), so the
catalog (`tools/menu_catalog.py`, `port/assets/strings/en.toml`) gives each string a stable ID.

`DISC` names the disc's own title widgets by their path in `ui.map`; `VISR` names the generated screens' titles by
the bitmap they draw today. The texts are the pictures' (the disc titles' as `port/assets/titles/titles.json`
recorded them).
"""

import xml.etree.ElementTree as ET

# the disc screens' titles: widget path -> its text for each frame of its picture
DISC = {
    'ui\\shell\\main_menu\\difficulty_select\\header_choose_difficulty': ['CHOOSE DIFFICULTY'],
    'ui\\shell\\main_menu\\gametype_select\\header_select_gametype': ['SELECT GAMETYPE'],
    'ui\\shell\\main_menu\\multiplayer_type_select\\connected\\4way_profile_select\\header_select_profile':
        ['SELECT PROFILE'],
    'ui\\shell\\main_menu\\multiplayer_type_select\\connected\\pregame\\header_select_teams':
        ['SELECT TEAMS', 'ENLISTED PLAYERS'],
    'ui\\shell\\main_menu\\multiplayer_type_select\\connected\\server_list\\header_found_games': ['SYSTEM LINK GAMES'],
    'ui\\shell\\main_menu\\multiplayer_type_select\\coop\\header_select_profile_player_1': ['SELECT PROFILE: PLAYER 1'],
    'ui\\shell\\main_menu\\multiplayer_type_select\\coop\\header_select_profile_player_2': ['SELECT PROFILE: PLAYER 2'],
    'ui\\shell\\main_menu\\multiplayer_type_select\\header_multiplayer': ['MULTIPLAYER'],
    'ui\\shell\\main_menu\\multiplayer_type_select\\mp_map_select\\header_select_map': ['SELECT MAP'],
    'ui\\shell\\main_menu\\multiplayer_type_select\\split_screen\\4way_profile_select\\header_select_profile':
        ['SELECT PROFILE'],
    'ui\\shell\\main_menu\\player_profiles_select\\header_select_profile': ['SELECT PROFILE'],
    'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\indicator_options_edit\\header_indicator_options':
        ['INDICATOR OPTIONS'],
    'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\item_options_edit\\header_item_options': ['ITEM OPTIONS'],
    'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\name_edit\\header_gametype_name': ['GAMETYPE NAME'],
    'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\player_options_edit\\header_player_options':
        ['PLAYER OPTIONS'],
    'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\playlist_edit\\game_type_select\\header_rules':
        ['STEP 2: SET GAME RULES'],
    'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\playlist_edit\\header_edit_multiplayer_gametype':
        ['EDIT MULTIPLAYER GAMETYPE'],
    'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\playlist_edit\\header_set_options':
        ['STEP 1: SELECT GAME'],
    'ui\\shell\\main_menu\\settings_select\\multiplayer_setup\\playlist_select\\header_select_gametype_to_edit':
        ['SELECT GAMETYPE TO EDIT'],
    'ui\\shell\\main_menu\\settings_select\\player_setup\\header_select_profile_to_edit': ['SELECT PROFILE TO EDIT'],
    'ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\advanced_controls\\header_advanced_controls':
        ['ADVANCED CONTROLS'],
    'ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\color_edit\\header_profile_color':
        ['MULTIPLAYER COLOR'],
    'ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\controller_edit\\'
    'header_profile_controller_setting': ['CONTROLLER SETUP'],
    'ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\header_profile_settings':
        ['EDIT PROFILE SETTINGS'],
    'ui\\shell\\main_menu\\settings_select\\player_setup\\player_profile_edit\\name_edit\\header_profile_name':
        ['PROFILE NAME'],
    'ui\\shell\\main_menu\\solo_level_select\\header_replay_level': ['LOAD LEVEL'],
}

MT = 'xbox/main_menu/multiplayer_type_select'
MS = 'xbox/main_menu/settings_select/multiplayer_setup'
PE = 'xbox/main_menu/settings_select/player_setup/player_profile_edit'

# the generated screens' titles: the bitmap a title widget draws today -> its text
VISR = {
    f'{MT}/coop_options/header_coop_options': 'CO-OP OPTIONS',
    f'{MT}/direct_ip/header_direct_ip': 'DIRECT IP',
    f'{MT}/game_options_edit/header_game_options': 'GAME',
    f'{MT}/join_game/filters/header_filters': 'FILTERS',
    f'{MT}/join_game/header_password': 'PASSWORD',
    f'{MT}/join_game/header_server_browser': 'SERVER BROWSER',
    f'{MT}/join_game/header_direct_link': 'DIRECT LINK',
    f'{MT}/join_game/header_internet': 'INTERNET LOBBY',
    f'{MT}/join_game/header_lan': 'LAN LOBBY',
    f'{MT}/lobby/header_lobby': 'GAME LOBBY',
    f'{MT}/lobby/header_add_player': 'ADD PLAYER',
    f'{MT}/mp_map_select/header_select_map': 'SELECT MAP',
    f'{MT}/respawn_options_edit/header_respawn_options': 'RESPAWN OPTIONS',
    f'{MT}/server_settings/header_server_settings': 'SERVER SETUP',
    f'{MT}/teams_hud_edit/header_teams_hud': 'TEAMS & HUD',
    f'{MT}/vehicle_types_edit/header_vehicle_types': 'VEHICLE TYPES',
    f'{MT}/weapons_edit/header_weapons': 'CHOOSE WEAPONS',
    f'{MS}/playlist_edit/header_edit_multiplayer_gametype': 'EDIT MULTIPLAYER GAMETYPE',
    f'{MS}/playlist_select/header_select_gametype_to_edit': 'SELECT GAMETYPE TO EDIT',
    f'{MS}/vehicle_options_edit/header_vehicle_options': 'VEHICLE OPTIONS',
    f'{MS}/playlist_edit/header_set_options': 'STEP 2: SET GAME RULES',
    f'{PE}/about/header_about': 'ABOUT',
    f'{PE}/about/header_open_source_notices': 'OPEN SOURCE NOTICES',
    f'{PE}/audio_settings/header_profile_audio_settings': 'AUDIO SETUP',
    f'{PE}/controls_setup/header_profile_controls_settings': 'CONTROLS SETUP',
    f'{PE}/controller_edit/header_profile_controller_setting': 'CONTROLLER SETUP',
    f'{PE}/graphics/header_graphics': 'GRAPHICS',
    f'{PE}/maps/header_maps': 'MAPS',
    f'{PE}/mouse_settings/header_profile_mouse_settings': 'MOUSE SETUP',
    f'{PE}/network_setup/header_profile_network_settings': 'NETWORK SETUP',
    f'{PE}/video_settings/header_profile_video_settings': 'VIDEO SETUP',
    f'{PE}/header_edit_profile_settings': 'EDIT PROFILE SETTINGS',
}


def is_title_picture(bitmap):
    """whether a generated widget's bitmap is a screen title's picture (a dialog's header is not a title)"""
    return bitmap in VISR


def title(root, widget, texts):
    """Declare widget (ours by name, or the map's by its path) the screen's title, saying texts (one for each frame
    of its picture): a string list of the menus' and a <title> that names it."""
    name = widget.replace('\\', '/') + '/title'
    strings = ET.SubElement(root, 'strings', name=name)
    for text in texts:
        ET.SubElement(strings, 'string', text=text)
    ET.SubElement(root, 'title', widget=widget, strings=name)


def declare(documents, disc_document):
    """Every title: the generated screens' in the documents that hold their widgets, the disc's in disc_document."""
    for root in documents.values():
        for widget in list(root.iter('widget')):
            if is_title_picture(widget.get('bitmap', '')):
                title(root, widget.get('name'), [VISR[widget.get('bitmap')]])
    for widget, texts in DISC.items():
        title(disc_document, widget, texts)
