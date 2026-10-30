# Menu pictures that are not redraws

The menus' bitmaps are drawn from the high-res redraws (`svg/`, from ui-svg-handmade) where there is
one. The PC version's pictures that have none (3D renders, screenshots and logos) are not in this
repository.

## From the Xbox's map

These have the same pictures, in the same order, in the Xbox's `ui.map`, which the menus draw them
from (the player's own; nothing is shipped). `mp_map_grafix`: the Xbox's has the 13 Xbox maps' pictures
(the PC version's first 13) and its "?"; the PC version's other 8 are its own maps'.

- `ui\shell\bitmaps\colors_sm`
- `ui\shell\bitmaps\mp_map_grafix`
- `ui\shell\bitmaps\sp_levels`
- `ui\shell\main_menu\difficulty_select\difficulty_options`
- `ui\shell\main_menu\difficulty_select\difficulty_options_small`
- `ui\shell\main_menu\halo_logo`
- `ui\shell\main_menu\settings_select\player_setup\player_profile_edit\color_edit\player_color_marine_large`

These frames are drawn from a frame of the Xbox map's bitmap of the same name, whose other frames differ:
the same picture, or for the profile settings the Xbox's picture for the same thing.

| The PC version's frame | The Xbox's frame |
| --- | --- |
| `shell/main_menu/settings_select/player_setup/player_profile_edit/profile_options__0.png` | `ui\shell\main_menu\settings_select\player_setup\player_profile_edit\profile_options` frame 0 |
| `shell/main_menu/settings_select/player_setup/player_profile_edit/profile_options__2.png` | `ui\shell\main_menu\settings_select\player_setup\player_profile_edit\profile_options` frame 1 |
| `shell/main_menu/settings_select/player_setup/player_profile_edit/profile_options__7.png` | `ui\shell\main_menu\settings_select\player_setup\player_profile_edit\profile_options` frame 3 |

These frames are added after a bitmap's, drawn scaled from the Xbox map's (the profile settings'
picture of Gamepad Setup: the Xbox's Controller Setup's pictures of the button settings).

| Our bitmap | The Xbox's frame |
| --- | --- |
| `main_menu/settings_select/player_setup/player_profile_edit/profile_options` | `ui\shell\main_menu\settings_select\player_setup\player_profile_edit\controller_edit\config_controller` frame 4 |
| `main_menu/settings_select/player_setup/player_profile_edit/profile_options` | `ui\shell\main_menu\settings_select\player_setup\player_profile_edit\controller_edit\config_controller` frame 5 |
| `main_menu/settings_select/player_setup/player_profile_edit/profile_options` | `ui\shell\main_menu\settings_select\player_setup\player_profile_edit\controller_edit\config_controller` frame 6 |
| `main_menu/settings_select/player_setup/player_profile_edit/profile_options` | `ui\shell\main_menu\settings_select\player_setup\player_profile_edit\controller_edit\config_controller` frame 7 |
| `main_menu/settings_select/player_setup/player_profile_edit/profile_options` | `ui\shell\main_menu\settings_select\player_setup\player_profile_edit\controller_edit\config_controller` frame 8 |

## PC description pictures

`tools/ce_menus.py` resolves the PC description frames before considering a placeholder.
The profile editor's ABOUT row uses our information emblem (`xbox/art/about.svg`),
in the palette and line weights of VIDEO, AUDIO, and INTERNET. The Xbox look does
not have an ABOUT row or a dedicated ABOUT picture. MOUSE SETUP uses our mouse
drawing (`xbox/art/mouse.svg`) with the same palette, glow, and line weights.

The other gaps use the player's `ui.map` pictures with a transparent fallback and
runtime extraction into `cache/pc/v1`, as the Xbox settings hub does. No game art ships.

| PC bitmap | Frames and sources |
| --- | --- |
| `mp_options` | 0 → Xbox 0; 1 → Xbox 1; 2 → INTERNET (invite-link join) |
| `gametype_options` | 0 → NAME; 1 → existing five-icon redraw; 2 → PLAYER; 3 → ITEM; 4 → VEHICLE; 5 → INDICATOR; 6 → TEAMPLAY; 7 → SAVE |
| `profile_options` | 1 → Xbox 0 (keyboard); 3 → MOUSE; 4 → AUDIO; 5 → VIDEO; 6 → INTERNET; 8 → ABOUT |

All 13 former placeholder PNGs have been removed. The original redraw for the
PC gametype chooser's frame 1 remains. The other seven frames use original
`xbox/art/gametype-*.svg` drawings, rendered at 4x. All eight frames draw at
256×128 in a matching widget centered above the text, preserving their 2:1
proportions without clipping or overlap.
None uses the player's map.
The picture widget selects the focused child's position, not its `string_index`:
VEHICLE is frame 4, INDICATOR is frame 5, and TEAMPLAY is frame 6.
See `playlist_settings_menu_update_extended_description` and
`ui_widget_port_list_index` in `source/interface/`.

The PC version's `ui\gamespy` and `ui\ticker` fonts are drawn with `ui\small_ui`, which
the Xbox's map has.
