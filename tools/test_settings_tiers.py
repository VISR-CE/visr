"""Setting tiers (port_config.c's enum config_tier): every setting says who it is for.

Bug class: a value tuned in headset sessions became a player setting by default, written into every
config.toml and the public settings reference. A new setting that is neither debug.* nor a profile
setting must now name a tier, and a player setting must be on a menu row or on the reviewed list of
file-only choices below. The C side (writing, removal of a written default, config_write of a tuning
value) is checked by port/ios/tests/config_origins_probe.c's check_tiers.
"""
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import settings_schema as schema

ROOT = Path(__file__).resolve().parents[1]

# Player settings without a menu row, reviewed: each is a choice a player (or a server operator, or a
# streamer) makes by editing config.toml. Adding a setting here is a decision: a value we tune belongs
# in _tier_tuning instead.
FILE_ONLY_PLAYER = {
    # desktop windows and the flat picture
    'display.fullscreen', 'display.resolution_scaling', 'display.window_scale', 'display.screen_width',
    'display.native_hud', 'display.scoreboard_background_color', 'display.movies',
    'display.first_person_legs', 'display.first_person_legs_seat_shift',
    # rendering switches kept for comparison (candidates for tuning, not yet decided)
    'display.decal_batching', 'display.decal_dense_upload', 'display.light_volume_batching',
    'display.shadow_batching', 'display.per_pixel_lighting', 'display.point_light_highlight',
    'display.point_light_limit', 'display.soft_particles', 'display.ambient_occlusion',
    'display.shadow_props', 'display.ui_redraw', 'display.ui_redraw_pictures', 'display.upscaled_textures_memory',
    # frame timing (candidates for tuning, not yet decided)
    'display.frame_deadline', 'display.late_look', 'display.late_start', 'display.frame_source',
    'display.frames_in_flight', 'display.frame_spacing',
    # Apple Vision Pro: the window or immersive space, and aiming (menu rows come with the HEADSET screen)
    'display.immersive', 'display.stereo', 'display.theater_width', 'display.theater_distance',
    'display.theater_environment', 'input.aim', 'input.aim_hand',
    # TODO(comfort and aim design, phase 2): input.comfort's COMFORT row replaces these five, which then become
    # tuning overrides (decided by the coordinator, may be revisited); display.screen_framing waits on a decision
    'input.turn', 'input.snap_angle', 'input.smooth_turn_speed', 'input.comfort_vignette',
    'display.stereo_vehicle_screen', 'display.screen_framing',
    # input
    'input.ui_touch_target', 'input.controller_rumble', 'input.blast_rumble', 'input.stick_dead_zone',
    'controls.move_forward', 'controls.move_backward', 'controls.strafe_left', 'controls.strafe_right',
    'controls.jump', 'controls.crouch', 'controls.fire', 'controls.throw_grenade', 'controls.melee',
    'controls.reload', 'controls.zoom', 'controls.switch_weapon', 'controls.switch_grenade', 'controls.action',
    'controls.flashlight', 'controls.scoreboard', 'controls.pause',
    # streamer mode's filters, audio, the game
    'streamer.slurs', 'streamer.swearing', 'streamer.insults', 'audio.fast_filter', 'audio.rate_bus',
    'audio.loose_sounds', 'game.console_log', 'game.language', 'game.loading_screen',
    'game.loading_screen_seconds', 'game.garbage_limit', 'game.enhanced_animations', 'game.custom_edition',
    'paths.data', 'paths.saves', 'paths.custom_edition',
    # network: addresses, servers and a host's rules
    'network.address', 'network.broadcast', 'network.lan_broadcast', 'network.online_ask', 'network.tunnel_port',
    'network.background_grace', 'network.public_lobby', 'network.delta_browse', 'network.delta_table',
    'network.delta_peer', 'network.delta_announce', 'network.delta_url', 'network.host_public',
    'network.host_version', 'network.map_share', 'network.map_share_upload', 'network.coop_public',
    'network.coop_friendly_fire', 'network.coop_own_friendly_fire', 'network.coop_ally_friendly_fire',
    'network.coop_enemies_mode', 'network.coop_enemies', 'network.coop_enemies_multiplier',
    'network.brokers_file', 'network.stun_servers', 'network.block_mouse_aim_assist',
    'discord.application_id',
}


def menu_settings():
    """The settings an Xbox-look menu row is bound to (setting="section.key")."""
    found = set()
    for path in (ROOT / 'port/assets/menus/xbox').glob('*.xml'):
        found |= set(re.findall(r'setting="([a-z_]+\.[a-z_0-9]+)"', path.read_text()))
    return found


def tiers():
    settings = schema.config_settings()
    return settings, {name: schema.setting_tier(name, item) for name, item in settings.items()}


def test_every_setting_has_a_tier():
    _, tier = tiers()
    missing = sorted(name for name, value in tier.items() if value is None)
    assert not missing, ('settings with no tier: add _tier_player or _tier_tuning to their entries in '
                         f'port_config.c: {missing}')


def test_derived_tiers_are_not_stored_twice():
    settings, tier = tiers()
    doubled = sorted(name for name, item in settings.items()
                     if tier[name] in ('profile', 'debug') and 'x-tier' in item)
    assert not doubled, f'a profile or debug.* setting takes its tier from that, not the field: {doubled}'


def test_player_settings_are_on_a_menu_row_or_reviewed():
    _, tier = tiers()
    menus = menu_settings()
    unreviewed = sorted(name for name, value in tier.items()
                        if value == 'player' and name not in menus and name not in FILE_ONLY_PLAYER)
    assert not unreviewed, ('player settings with no menu row and not on FILE_ONLY_PLAYER: give them a row, '
                            f'make them _tier_tuning, or review them onto the list: {unreviewed}')


def test_the_reviewed_list_is_current():
    _, tier = tiers()
    stale = sorted(name for name in FILE_ONLY_PLAYER if tier.get(name) != 'player' or name in menu_settings())
    assert not stale, f'FILE_ONLY_PLAYER names settings that are not file-only player settings: {stale}'


def test_no_menu_row_sets_a_tuning_value():
    _, tier = tiers()
    rows = sorted(name for name in menu_settings() if tier.get(name) == 'tuning')
    assert not rows, f'a menu row writes a tuning value, which is never saved: {rows}'


def test_tuning_values_leave_the_public_reference():
    settings, tier = tiers()
    table = schema.markdown('config', settings)
    listed = sorted(name for name, value in tier.items() if value == 'tuning' and f'| `{name}` |' in table)
    assert not listed, f'tuning values in the settings reference: {listed}'
    # (still in the editor schema, since config.toml may set them)
    config = schema.config_schema(settings)
    assert config['properties']['input']['properties']['aim_steady']['x-tier'] == 'tuning'


# Tuning values that are not Apple Vision Pro's alone, reviewed: each is written nowhere and read everywhere.
TUNING_OFF_HEADSET = {
    # the audio mixing thread's scheduling class, chosen from measurements; a player has no way to judge it
    'audio.worker_priority',
    # compares the corrected interpolation lead with the old timing on every display; a measurement switch
    'display.presentation_lead',
    # the override of smooth motion, which follows the frame rate in force (smooth_motion.h); the SMOOTH MOTION row is gone
    'display.interpolation',
    # a safety switch, on by default; off only to compare with the Xbox's Save and Quit-only writes
    'saves.checkpoint_persist',
    # the Metal encode thread, chosen per device from measured pairs; a device profile will carry it
    'display.encode_thread',
}
VISIONOS_PLAYER = {'display.immersive', 'display.stereo', 'display.theater_distance', 'display.theater_environment',
                   'display.theater_width', 'input.aim', 'input.aim_hand',
                   # (until phase 2's COMFORT row: see FILE_ONLY_PLAYER)
                   'input.turn', 'input.snap_angle', 'input.smooth_turn_speed', 'input.comfort_vignette',
                   'display.stereo_vehicle_screen', 'display.screen_framing'}
VISIONOS_PROFILE = {'display.foveation', 'display.frame_repeat', 'display.hud_resolution', 'display.lod_scale',
                    'display.render_quality', 'input.aim_loss_grace'}


def test_tuning_values_are_apple_vision_pro_alone():
    settings, tier = tiers()
    other = sorted(name for name, value in tier.items() if value == 'tuning' and name not in TUNING_OFF_HEADSET
                   and settings[name]['x-platforms'] != ['visionos'])
    assert not other, f'tuning values not marked _platform_visionos (or reviewed onto TUNING_OFF_HEADSET): {other}'


def test_apple_vision_pro_player_and_profile_settings_are_reviewed():
    settings, tier = tiers()
    visionos = {name for name, item in settings.items() if item['x-platforms'] == ['visionos']}
    assert {name for name in visionos if tier[name] == 'player'} == VISIONOS_PLAYER
    assert {name for name in visionos if tier[name] == 'profile'} == VISIONOS_PROFILE


def test_aim_defaults_to_auto():
    settings, _ = tiers()
    assert settings['input.aim']['default'] == 'auto'
