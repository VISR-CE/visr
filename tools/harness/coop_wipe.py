"""Production checkpoint membership, host policy, save and death handlers."""
from . import function, read, structure


def generated():
    coop = read('port/linux/game/network_coop.c')
    state = read('source/saved games/game_state.c')
    main = read('source/main/main.c')
    types = structure(coop, 'coop_checkpoint_players')
    lobby = read('port/linux/src/p2p_lobby.c')
    code = function(lobby, 'p2p_set_hosting_public')
    code += function(read('port/linux/src/p2p.c'), 'p2p_cooperative_game_start')
    code += function(read('source/game/game.c'), 'remove_quitting_players_from_game')
    for name in ('coop_host', 'coop_player_has_living_unit', 'network_coop_checkpoint_saved', 'coop_player_present', 'checkpoint_has_living_current_player', 'checkpoint_players_match', 'coop_reconcile_departures',
                 'coop_respawn_if_no_living_players',
                 'network_coop_wipe_reverts', 'network_coop_new_game'):
        code += function(coop, name)
    respawn = function(read('source/game/players.c'), 'players_respawn_at_checkpoint')
    respawn = respawn.replace('players_respawn_at_checkpoint(', 'real_players_respawn_at_checkpoint(', 1)
    detach = function(read('source/game/players.c'), 'network_player_detach_unit')
    code = detach + respawn + '\nstatic void players_respawn_at_checkpoint(void) {respawns++; real_players_respawn_at_checkpoint();}\n' + code
    code += function(state, 'game_state_save')
    for name in ('main_lost_map', 'main_lost_map_scripted', 'main_death_timer_expired', 'main_lost_map_private'):
        code += function(main, name)
    return (('types.inc', types), ('under_test.inc', code))
