"""Exercise the production co-op ally and static-object damage rules without game assets."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from probekit import cc, source as probe_source  # noqa: E402


def test_damage_rules():
    pieces = [probe_source.function("port/linux/src/input_replay.c", "input_replay_playing"),
              probe_source.function("port/linux/game/network_coop.c", "network_coop_protects_ally"),
              probe_source.function("port/linux/game/network_damage.c", "network_damage_static_rules"),
              probe_source.function("port/linux/game/network_damage.c", "static_target"),
              probe_source.function("port/linux/game/network_damage.c", "network_damage_static_source"),
              probe_source.function("port/linux/game/network_damage.c", "network_damage_updates_shields"),
              probe_source.function("port/linux/game/network_damage.c", "network_damage_deals"),
              probe_source.function("source/objects/damage.c", "damage_effect_suspend_static_authorization"),
              probe_source.function("source/objects/damage.c", "damage_effect_new_on_object"),
              probe_source.function("source/objects/damage.c", "damage_effect_new_at_location"),
              probe_source.function("source/networking/network_server_manager.c", "network_game_server_port_set_cooperative_ally_friendly_fire"),
              probe_source.function("source/networking/network_server_manager.c", "network_game_server_port_set_cooperative_player_collisions")]
    flags = (ROOT / "source/networking/network_game_manager.h").read_text()
    start = flags.index("enum\n{", flags.index("/* port: network_game's cooperative_flags */"))
    flags = flags[start:flags.index("};", start) + 2]
    template = (ROOT / "port/ios/tests/network_damage_rules_probe.c").read_text().replace("/* COOP_FLAGS */", flags).replace(
        "/* VERSION_HEADER */", '#include "' + str(ROOT / "port/linux/include/halo_network_version.h") + '"')
    body = "\n".join(pieces)
    variants = [("production", body)]
    for label, before, after in (
        ("ally switch", "!TEST_FLAG(game->cooperative_flags, _network_game_cooperative_no_ally_friendly_fire_bit)", "TRUE"),
        ("shield-only target", "object_get_maximum_shield_vitality(object_index, TRUE) > 0.0f", "FALSE"),
        ("static source authority", "return authorized || game_connection() != _game_connection_network_client;", "return TRUE;"),
        ("nested explosion", "distributed_damage_authorized = FALSE;", "distributed_damage_authorized = authorized;"),
        ("replay gate", "return replay.state_count != 0 || replay.action_count != 0;", "return FALSE;"),
        ("static version gate", "halo_network_version_has_static_shields(network_game_host_version())", "TRUE"),
        ("ally version gate", "!halo_network_version_has_ally_switch(network_game_host_version())", "FALSE"),
        ("ally flag polarity", "!ally_friendly_fire", "ally_friendly_fire"),
    ):
        assert body.count(before) == 1, label
        variants.append((label, body.replace(before, after)))
    with tempfile.TemporaryDirectory() as directory:
        for label, variant in variants:
            source = Path(directory) / "probe.c"
            source.write_text(template.replace("/* PRODUCTION_FUNCTIONS */", variant))
            executable = Path(directory) / "probe"
            cc.build([source], output=executable, std="c11", relaxed=["unused-function"], port_stubs=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            if label == "production":
                assert result.returncode == 0, result.stdout + result.stderr
            else:
                assert result.returncode != 0, f"Negative control escaped: {label}"


def test_rules_reach_damage_consumers():
    cause = probe_source.function("source/objects/damage.c", "object_cause_damage")
    assert cause.index("network_coop_protects_ally(damage, current_object_index)") < cause.index("object_damage_aftermath(")
    assert probe_source.function("source/objects/damage.c", "object_damage_update").count("network_damage_updates_shields(object_index)") == 2
    deals = probe_source.function("port/linux/game/network_damage.c", "network_damage_deals")
    assert deals.index("network_damage_static_source(damage)") < deals.index("distributed_player_is_local(damage->owner_player_index)")


def test_ally_setting_and_menu_match_wire_polarity():
    import port_settings
    import settings_schema

    setting = settings_schema.config_settings()["network.coop_ally_friendly_fire"]
    assert setting["default"] is True
    menu = "\n".join(port_settings.multiplayer_files()["main_menu.multiplayer_type_select.coop_options.xml"])
    assert 'strings="OFF|PLAYERS ONLY|AI ALLIES ONLY|ON"' in menu
    assert 'ally_friendly_fire_spinner' not in menu
    update = probe_source.function("port/linux/game/menu_functions.c", "coop_options_update")
    assert 'named(list, "friendly_fire_spinner", 0)' in update
    save = probe_source.function("port/linux/game/menu_functions.c", "coop_options_save")
    assert 'config_write("network.coop_ally_friendly_fire", allies ? "true" : "false")' in save
    start = probe_source.function("port/linux/game/menu_functions.c", "server_start")
    assert "server_coop_rules_apply();" in start
    assert "network_game_server_port_set_cooperative_ally_friendly_fire(" in probe_source.function("port/linux/game/menu_functions.c", "server_coop_rules_apply")
