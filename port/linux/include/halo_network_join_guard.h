/*
HALO_NETWORK_JOIN_GUARD.H

Whether a join may be started now. network_game_client_initiate_join_game
asserts these clauses; a second JOIN activation while the first is still
joining (the state is no longer "searching"), or one on a client whose
connection is up, used to reach that assert and halt the game. The callers
that start a join ask network_game_client_can_initiate_join, which uses this
verdict, and ignore the request when it names a clause.
*/

#ifndef __HALO_NETWORK_JOIN_GUARD_H
#define __HALO_NETWORK_JOIN_GUARD_H

enum halo_network_join_refusal
{
	HALO_NETWORK_JOIN_OK = 0,
	HALO_NETWORK_JOIN_NO_CLIENT,
	HALO_NETWORK_JOIN_NOT_SEARCHING, /* a join is under way, or the client is already in a game */
	HALO_NETWORK_JOIN_NO_GAME,
	HALO_NETWORK_JOIN_NO_PARAMETERS,
	HALO_NETWORK_JOIN_NO_CONNECTION,
	HALO_NETWORK_JOIN_CONNECTED, /* a connection from before is still up */
	HALO_NETWORK_JOIN_WRONG_PLATFORM,
};

/* the first clause of the join's assertion that does not hold, in its order */
static inline enum halo_network_join_refusal halo_network_join_refusal(int has_client, int searching, int has_game,
	int has_parameters, int has_connection, int connection_connected, int platform_matches)
{
	if (!has_client) return HALO_NETWORK_JOIN_NO_CLIENT;
	if (!searching) return HALO_NETWORK_JOIN_NOT_SEARCHING;
	if (!has_game) return HALO_NETWORK_JOIN_NO_GAME;
	if (!has_parameters) return HALO_NETWORK_JOIN_NO_PARAMETERS;
	if (!has_connection) return HALO_NETWORK_JOIN_NO_CONNECTION;
	if (connection_connected) return HALO_NETWORK_JOIN_CONNECTED;
	if (!platform_matches) return HALO_NETWORK_JOIN_WRONG_PLATFORM;
	return HALO_NETWORK_JOIN_OK;
}

/* the name of the clause, for the assertion message and the network log */
static inline char const *halo_network_join_refusal_name(enum halo_network_join_refusal refusal)
{
	switch (refusal)
	{
	case HALO_NETWORK_JOIN_OK: return "join allowed";
	case HALO_NETWORK_JOIN_NO_CLIENT: return "join refused: no network client";
	case HALO_NETWORK_JOIN_NOT_SEARCHING: return "join refused: client is not searching (a join is already under way or a game is joined)";
	case HALO_NETWORK_JOIN_NO_GAME: return "join refused: no game";
	case HALO_NETWORK_JOIN_NO_PARAMETERS: return "join refused: no join parameters";
	case HALO_NETWORK_JOIN_NO_CONNECTION: return "join refused: client has no connection";
	case HALO_NETWORK_JOIN_CONNECTED: return "join refused: client connection is still up";
	case HALO_NETWORK_JOIN_WRONG_PLATFORM: return "join refused: game is on another platform";
	}
	return "join refused";
}

#endif
