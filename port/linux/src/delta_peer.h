/* Delta Peer framing and sessions; callers supply addresses, randomness and milliseconds. */
#ifndef DELTA_PEER_H
#define DELTA_PEER_H
#include "halo_delta.h"

#define DELTA_PEER_PORT_OFFSET 10
#define DELTA_PEER_MAX_PACKET 1200
#define DELTA_PEER_MAX_ENTRIES 32
#define DELTA_PEER_MACHINES 128
#define DELTA_PEER_HELLO 1
#define DELTA_PEER_WELCOME 2
#define DELTA_PEER_LEGACY 3
#define DELTA_PEER_ROSTER 4
#define DELTA_PEER_BYE 5
#define DELTA_PEER_WAITING 1
#define DELTA_PEER_ACTIVE 2
#define DELTA_PEER_FALLBACK 3

struct delta_peer_entry
{
	unsigned char machine, flags, key[8];
	unsigned int agreed, revision;
	unsigned char player_id[16];
};
struct delta_peer_packet
{
	unsigned char major, type, machine, key[8], limit, count;
	unsigned int session, capabilities, agreed, serial, room;
	unsigned short legacy;
	char build[32];
	struct delta_peer_entry entries[DELTA_PEER_MAX_ENTRIES];
};
struct delta_peer_rate
{
	unsigned int time, tokens;
	int initialized;
};
struct delta_peer_session
{
	int state;
	unsigned int session, started, sent, changed;
	int has_sent, has_changed;
	unsigned int agreed;
	unsigned char key[8];
	struct delta_peer_rate rate;
};
int delta_peer_encode(unsigned char *bytes, unsigned capacity, const struct delta_peer_packet *packet);
int delta_peer_decode(struct delta_peer_packet *packet, const unsigned char *bytes, unsigned size);
int delta_peer_rate_take(struct delta_peer_rate *rate, unsigned now, unsigned per_second, unsigned burst);
void delta_peer_client_start(struct delta_peer_session *state, unsigned session, unsigned now);
int delta_peer_client_tick(struct delta_peer_session *state, unsigned now, int machine);
int delta_peer_client_receive(struct delta_peer_session *state, const struct delta_peer_packet *packet, unsigned now);
/* Address membership is checked before even the common header is parsed. */
int delta_peer_host_receive(struct delta_peer_session peers[DELTA_PEER_MACHINES],
	const unsigned int addresses[DELTA_PEER_MACHINES], unsigned sender,
	const unsigned char *bytes, unsigned size, unsigned now,
	const struct delta_peer_packet *identity, struct delta_peer_packet *reply);
#endif
