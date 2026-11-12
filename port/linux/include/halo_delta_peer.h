/* Game hooks for the optional Delta socket; addresses use the game's host byte order. */
#ifndef HALO_DELTA_PEER_H
#define HALO_DELTA_PEER_H
int delta_peer_host_open(void);
void delta_peer_game_host_idle(const unsigned int *addresses, const unsigned char *present,
	int local_machine, int limit, unsigned int now);
void delta_peer_game_host_stop(void);
void delta_peer_game_join(unsigned int address, unsigned short game_port, int advertised);
void delta_peer_game_client_idle(int machine, const unsigned char *has_players, unsigned int now);
void delta_peer_game_client_stop(void);
/* A peer's platform is a claim; 0 means unknown, legacy, or no active session. */
unsigned int delta_peer_machine_platform(int machine);
#endif
