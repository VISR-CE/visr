/* Exercise the real socket integration with a deterministic xnet stand-in.
No sockets, service, app, system clock, or game data are used. */
#define __HALO_LINUX_PLATFORM_H
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "delta_peer.h"
#include "halo_delta_peer.h"

typedef int SOCKET;
typedef unsigned long u_long;
typedef unsigned char BYTE;
#define INVALID_SOCKET (-1)
#define AF_INET 2
#define SOCK_DGRAM 2
#define FIONBIO 1
struct sockaddr { unsigned short family; char data[14]; };
struct in_addr { unsigned int s_addr; };
struct sockaddr_in { unsigned short sin_family, sin_port; struct in_addr sin_addr; char pad[8]; };
static unsigned short halo_ws_htons(unsigned short n) { return (unsigned short)((n >> 8) | (n << 8)); }
static unsigned halo_ws_htonl(unsigned n) { return __builtin_bswap32(n); }
static unsigned halo_ws_ntohl(unsigned n) { return __builtin_bswap32(n); }
static int enabled = 1, announcing = 1, busy, next_socket, bound_port, closes;
static int queued_socket = -1, queued_size;
static unsigned char queued[1201];
static struct sockaddr_in queued_from;
static struct delta_peer_packet sent[200];
static int sent_count;
int config_boolean(const char *name)
{
	return !strcmp(name, "network.delta_announce") ? announcing : enabled;
}
static int halo_ws_socket(int family, int type, int protocol)
{
	assert(family == AF_INET && type == SOCK_DGRAM && protocol == 0); return ++next_socket;
}
static int halo_ws_bind(int socket, const struct sockaddr *address, int length)
{
	const struct sockaddr_in *in = (const struct sockaddr_in *)address;
	(void)socket; assert(length == sizeof(*in)); bound_port = halo_ws_htons(in->sin_port); return busy;
}
static int halo_ws_ioctlsocket(int socket, long flag, u_long *value)
{
	(void)socket; assert(flag == FIONBIO && *value == 1); return 0;
}
static int halo_ws_closesocket(int socket) { (void)socket; closes++; return 0; }
static int halo_ws_sendto(int socket, const char *bytes, int size, int flags, const struct sockaddr *to, int length)
{
	(void)socket; (void)flags; (void)to; assert(length == sizeof(struct sockaddr_in));
	assert(sent_count < 200 && delta_peer_decode(&sent[sent_count++], (const unsigned char *)bytes, size));
	return size;
}
static int halo_ws_recvfrom(int socket, char *bytes, int size, int flags, struct sockaddr *from, int *length)
{
	(void)flags;
	if (queued_socket != socket) return -1;
	assert(size >= queued_size && *length == sizeof(queued_from));
	memcpy(bytes, queued, queued_size); memcpy(from, &queued_from, sizeof(queued_from));
	queued_socket = -1; return queued_size;
}
static int XNetRandom(BYTE *bytes, unsigned size)
{
	assert(size == 4); memset(bytes, 0, size); bytes[0] = 123; return 0;
}
#include "../../linux/src/delta_peer_game.c"

static void incoming(int socket, unsigned address, unsigned short port, const struct delta_peer_packet *p)
{
	queued_size = delta_peer_encode(queued, sizeof(queued), p); assert(queued_size);
	queued_socket = socket; memset(&queued_from, 0, sizeof(queued_from));
	queued_from.sin_family = AF_INET; queued_from.sin_addr.s_addr = halo_ws_htonl(address);
	queued_from.sin_port = halo_ws_htons(port);
}
int main(void)
{
	unsigned addresses[128] = {0};
	unsigned char present[128] = {0};
	struct delta_peer_packet p;
	int socket, before;
	addresses[7] = 42; present[0] = present[7] = 1;
	busy = 1; delta_peer_game_host_idle(addresses, present, 0, 128, 0);
	assert(!delta_peer_host_open() && bound_port == 5160 && closes == 1);
	delta_peer_game_host_stop(); busy = 0;
	delta_peer_game_host_idle(addresses, present, 0, 128, 0);
	assert(delta_peer_host_open()); socket = next_socket;
	memset(&p, 0, sizeof(p)); p.major = 1; p.type = 1; p.session = 33; p.machine = 7;
	p.capabilities = 1; p.key[0] = 20; p.key[1] = 1;
	incoming(socket, 41, 6000, &p);
	delta_peer_game_host_idle(addresses, present, 0, 128, 1); assert(!sent_count);
	incoming(socket, 42, 6000, &p);
	delta_peer_game_host_idle(addresses, present, 0, 128, 2);
	assert(sent_count == 1 && sent[0].type == 2 && sent[0].session == 33);
	assert(delta_peer_machine_platform(7) == 20 && !delta_peer_machine_platform(8));
	delta_peer_game_host_idle(addresses, present, 0, 128, 249); assert(sent_count == 1);
	delta_peer_game_host_idle(addresses, present, 0, 128, 250);
	assert(sent_count == 2 && sent[1].type == 4 && sent[1].count == 2 && sent[1].room == 1);
	assert(sent[1].entries[1].machine == 7 && sent[1].entries[1].key[0] == 20);
	present[8] = 1; addresses[8] = 43;
	delta_peer_game_host_idle(addresses, present, 0, 128, 500);
	assert(sent_count == 3 && sent[2].room == 0 && sent[2].entries[2].flags == 0);
	p.type = 5; incoming(socket, 42, 6000, &p);
	delta_peer_game_host_idle(addresses, present, 0, 128, 750);
	assert(!delta_peer_machine_platform(7));
	/* A full room needs four datagrams; every machine must appear once. */
	{
		unsigned char seen[128] = {0};
		int i, j, total = 0;
		for (i = 0; i < 128; i++) { present[i] = 1; addresses[i] = (unsigned)(1000 + i); }
		addresses[7] = 42; p.type = 1; p.session = 34;
		before = sent_count; incoming(socket, 42, 6000, &p);
		delta_peer_game_host_idle(addresses, present, 0, 128, 1250);
		assert(sent_count == before + 5 && sent[before].type == 2);
		for (i = before + 1; i < sent_count; i++)
		{
			assert(sent[i].type == 4 && sent[i].count == 32);
			for (j = 0; j < sent[i].count; j++)
			{
				assert(!seen[sent[i].entries[j].machine]++); total++;
			}
		}
		assert(total == 128);
	}
	delta_peer_game_host_stop(); assert(!delta_peer_host_open());
	announcing = 0; before = next_socket;
	delta_peer_game_host_idle(addresses, present, 0, 128, 1000); assert(next_socket == before);

	/* A client must neither open a socket for a legacy host nor send before its index exists. */
	delta_peer_game_join(42, 5150, 0); delta_peer_game_client_idle(7, present, 0); assert(next_socket == before);
	delta_peer_game_join(42, 5150, 1); delta_peer_game_client_idle(-1, present, 0); assert(next_socket == before);
	sent_count = 0; delta_peer_game_client_idle(7, present, 10); socket = next_socket;
	assert(sent_count == 1 && sent[0].type == 1 && sent[0].session == 123 && bound_port == 0);
	delta_peer_game_client_idle(7, present, 509); assert(sent_count == 1);
	delta_peer_game_client_idle(7, present, 510); assert(sent_count == 2);
	p = sent[0]; p.type = 2; p.agreed = 1; p.machine = 0; p.key[0] = 22;
	incoming(socket, 41, 5160, &p); delta_peer_game_client_idle(7, present, 511);
	assert(!delta_peer_machine_platform(0));
	incoming(socket, 42, 5161, &p); delta_peer_game_client_idle(7, present, 512);
	assert(!delta_peer_machine_platform(0));
	p.session = 124; incoming(socket, 42, 5160, &p); delta_peer_game_client_idle(7, present, 513);
	assert(!delta_peer_machine_platform(0));
	p.session = 123; incoming(socket, 42, 5160, &p); delta_peer_game_client_idle(7, present, 514);
	assert(delta_peer_machine_platform(0) == 22);
	/* Independent roster datagrams accumulate and expire on the local clock. */
	memset(&p, 0, sizeof(p)); p.major = 1; p.type = 4; p.session = 123; p.count = 1;
	p.entries[0].machine = 40; p.entries[0].flags = 3; p.entries[0].agreed = 1; p.entries[0].key[0] = 20;
	incoming(socket, 42, 5160, &p); delta_peer_game_client_idle(7, present, 515);
	p.entries[0].machine = 70; p.entries[0].key[0] = 21;
	incoming(socket, 42, 5160, &p); delta_peer_game_client_idle(7, present, 516);
	assert(delta_peer_machine_platform(40) == 20 && delta_peer_machine_platform(70) == 21);
	present[40] = 0; delta_peer_game_client_idle(7, present, 517); assert(!delta_peer_machine_platform(40));
	delta_peer_game_client_idle(7, present, 12515); assert(delta_peer_machine_platform(70) == 21);
	delta_peer_game_client_idle(7, present, 12516); assert(!delta_peer_machine_platform(70));
	delta_peer_game_client_stop(); assert(sent[sent_count - 1].type == 5 && !delta_peer_machine_platform(0));
	delta_peer_game_join(42, 5150, 1); delta_peer_game_client_idle(7, present, 10000);
	delta_peer_game_client_idle(7, present, 14000); before = sent_count;
	delta_peer_game_client_idle(7, present, 15000); assert(sent_count == before);
	delta_peer_game_client_stop(); puts("delta peer game probe ok"); return 0;
}
