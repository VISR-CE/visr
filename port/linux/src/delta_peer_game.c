/* The game owns this state on its network idle. Only the advertised open flag
is read by the listing worker. All datagrams go through xnet.c's tunnel path. */
#include "platform.h"
#include "port_config.h"
#include "delta_peer.h"
#include "delta_platform.h"
#include "halo_delta_peer.h"
#include "../../../source/networking/network_game_protocol.h"
#include <stdio.h>
#include <string.h>

static struct
{
	SOCKET socket;
	int initialized, attempted, local, dirty, roster_sent;
	unsigned int addresses[DELTA_PEER_MACHINES], roster_time;
	unsigned char present[DELTA_PEER_MACHINES];
	struct sockaddr_in endpoints[DELTA_PEER_MACHINES];
	struct delta_peer_session peers[DELTA_PEER_MACHINES];
	struct delta_peer_packet identity;
} host;
static struct
{
	SOCKET socket;
	int initialized, wanted;
	struct sockaddr_in endpoint;
	struct delta_peer_session session;
	struct delta_peer_packet identity;
	unsigned char platforms[DELTA_PEER_MACHINES];
	unsigned int platform_seen[DELTA_PEER_MACHINES];
} client;
static int host_open;

int delta_peer_host_open(void)
{
	return __atomic_load_n(&host_open, __ATOMIC_RELAXED);
}
#ifdef HALO_IOS
void host_sdl_app_version(char *buffer, unsigned int size);
static int host_flag(const char *name)
{
	const char *value = getenv(name);
	return value && !strcmp(value, "1");
}
#endif
static void identity_make(struct delta_peer_packet *p)
{
	memset(p, 0, sizeof(*p)); p->major = DELTA_MAJOR; p->capabilities = DELTA_CAP_PLATFORM;
	p->legacy = HALO_PORT_NETWORK_VERSION; p->machine = 255;
#ifdef HALO_IOS
	delta_platform_key(p->key, delta_platform_current(host_flag("HALO_HOST_MAC"),
		host_flag("HALO_HOST_TVOS"), host_flag("HALO_HOST_VISIONOS")), 0, 0, 0);
	host_sdl_app_version(p->build, sizeof(p->build));
#else
	delta_platform_key(p->key, delta_platform_current(0, 0, 0), 0, 0, 0);
#ifdef HALO_BUILD_NUMBER
	snprintf(p->build, sizeof(p->build), "%d", HALO_BUILD_NUMBER);
#else
	snprintf(p->build, sizeof(p->build), "unknown");
#endif
#endif
	/* The wire caps build strings at 31 printable ASCII characters. */
	{
		unsigned i;
		p->build[31] = 0;
		for (i = 0; p->build[i]; i++)
			if ((unsigned char)p->build[i] < 32 || (unsigned char)p->build[i] > 126) p->build[i] = '?';
	}
}
static SOCKET socket_open(unsigned short port)
{
	SOCKET s = halo_ws_socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in address;
	u_long nonblocking = 1;
	if (s == INVALID_SOCKET) return s;
	memset(&address, 0, sizeof(address)); address.sin_family = AF_INET;
	address.sin_port = halo_ws_htons(port);
	if (halo_ws_bind(s, (const struct sockaddr *)&address, sizeof(address)) ||
		halo_ws_ioctlsocket(s, FIONBIO, &nonblocking))
	{
		halo_ws_closesocket(s); return INVALID_SOCKET;
	}
	return s;
}
static void packet_send(SOCKET s, const struct sockaddr_in *to, const struct delta_peer_packet *p)
{
	unsigned char bytes[DELTA_PEER_MAX_PACKET];
	int n = delta_peer_encode(bytes, sizeof(bytes), p);
	if (n) halo_ws_sendto(s, (const char *)bytes, n, 0, (const struct sockaddr *)to, sizeof(*to));
}
static void host_bye(int machine)
{
	struct delta_peer_packet p;
	if (host.peers[machine].state != DELTA_PEER_ACTIVE) return;
	memset(&p, 0, sizeof(p)); p.major = DELTA_MAJOR; p.type = DELTA_PEER_BYE;
	p.session = host.peers[machine].session; packet_send(host.socket, &host.endpoints[machine], &p);
}
void delta_peer_game_host_stop(void)
{
	int i;
	if (!host.attempted) return;
	__atomic_store_n(&host_open, 0, __ATOMIC_RELAXED);
	if (host.initialized)
	{
		for (i = 0; i < DELTA_PEER_MACHINES; i++) host_bye(i);
		halo_ws_closesocket(host.socket);
	}
	memset(&host, 0, sizeof(host));
}
static void roster_send(unsigned limit, unsigned now)
{
	struct delta_peer_packet p;
	unsigned room = DELTA_CAP_PLATFORM;
	int i, recipient;
	if (host.roster_sent && now - host.roster_time < 250) return;
	if (host.roster_sent && !host.dirty && now - host.roster_time < 5000) return;
	for (i = 0; i < DELTA_PEER_MACHINES; i++)
		if (host.present[i] && i != host.local)
			room &= host.peers[i].state == DELTA_PEER_ACTIVE ? host.peers[i].agreed : 0;
	for (recipient = 0; recipient < DELTA_PEER_MACHINES; recipient++)
	{
		struct delta_peer_session *target = &host.peers[recipient];
		if (target->state != DELTA_PEER_ACTIVE) continue;
		memset(&p, 0, sizeof(p)); p.major = DELTA_MAJOR; p.type = DELTA_PEER_ROSTER;
		p.session = target->session; p.room = room; p.limit = (unsigned char)limit;
		/* One pass covers the whole room in datagrams of at most 32 entries. */
		for (i = 0; i < DELTA_PEER_MACHINES; i++)
		{
			struct delta_peer_entry *e;
			unsigned agreed;
			const unsigned char *key;
			int active;
			if (!host.present[i]) continue;
			if (p.count == DELTA_PEER_MAX_ENTRIES)
			{ packet_send(host.socket, &host.endpoints[recipient], &p); p.count = 0; }
			e = &p.entries[p.count++]; memset(e, 0, sizeof(*e)); e->machine = (unsigned char)i;
			active = i == host.local || host.peers[i].state == DELTA_PEER_ACTIVE;
			agreed = i == host.local ? DELTA_CAP_PLATFORM : host.peers[i].agreed;
			key = i == host.local ? host.identity.key : host.peers[i].key;
			if (!active) continue;
			e->flags = 1; e->agreed = agreed;
			if (agreed & target->agreed & DELTA_CAP_PLATFORM)
			{ e->flags |= 2; memcpy(e->key, key, 8); }
		}
		if (p.count)
		{
			packet_send(host.socket, &host.endpoints[recipient], &p);
			if (host.dirty || !host.roster_sent)
				platform_log("Delta Peer: ROSTER sent to machine %d (%u entries, room 0x%x)", recipient, p.count, room);
		}
	}
	host.roster_sent = 1; host.roster_time = now; host.dirty = 0;
}
void delta_peer_game_host_idle(const unsigned *addresses, const unsigned char *present,
	int local_machine, int limit, unsigned now)
{
	unsigned char bytes[DELTA_PEER_MAX_PACKET + 1];
	int i;
	if (!config_boolean("network.delta_peer") || !config_boolean("network.delta_announce"))
	{ delta_peer_game_host_stop(); return; }
	if (!host.attempted)
	{
		host.attempted = 1; host.socket = socket_open(NETWORK_GAME_SERVER_PORT + DELTA_PEER_PORT_OFFSET);
		if (host.socket == INVALID_SOCKET) return;
		host.initialized = 1; identity_make(&host.identity);
		__atomic_store_n(&host_open, 1, __ATOMIC_RELAXED);
	}
	if (!host.initialized) return;
	if (host.local != local_machine || host.identity.limit != limit)
		host.dirty = 1;
	host.local = local_machine; host.identity.machine = (unsigned char)(local_machine < 0 ? 255 : local_machine);
	host.identity.limit = (unsigned char)limit;
	for (i = 0; i < DELTA_PEER_MACHINES; i++)
	{
		unsigned address = present[i] && i != local_machine ? addresses[i] : 0;
		if (host.addresses[i] != address || host.present[i] != present[i])
		{
			host_bye(i); memset(&host.peers[i], 0, sizeof(host.peers[i])); host.dirty = 1;
		}
		host.addresses[i] = address;
		host.present[i] = present[i];
	}
	for (i = 0; i < 64; i++)
	{
		struct sockaddr_in from;
		struct delta_peer_packet reply;
		int length = sizeof(from), n, machine, changed;
		unsigned sender;
		memset(&from, 0, sizeof(from));
		n = halo_ws_recvfrom(host.socket, (char *)bytes, sizeof(bytes), 0, (struct sockaddr *)&from, &length);
		if (n < 0) break;
		if (length != sizeof(from) || from.sin_family != AF_INET) continue;
		sender = halo_ws_ntohl(from.sin_addr.s_addr);
		/* delta_peer_host_receive checks membership before parsing any bytes. */
		machine = delta_peer_host_receive(host.peers, host.addresses, sender, bytes, (unsigned)n,
			now, &host.identity, &reply, &changed);
		if (machine < 0) continue;
		if (changed) host.dirty = 1;
		if (reply.type)
		{
			if (reply.type == DELTA_PEER_WELCOME)
				platform_log("Delta Peer: HELLO from machine %d, WELCOME sent", machine);
			host.endpoints[machine] = from;
			packet_send(host.socket, &host.endpoints[machine], &reply);
		}
	}
	roster_send((unsigned)limit, now);
}
void delta_peer_game_client_stop(void)
{
	if (client.initialized)
	{
		if (client.session.state == DELTA_PEER_ACTIVE)
		{
			struct delta_peer_packet p = client.identity;
			p.type = DELTA_PEER_BYE; p.session = client.session.session;
			packet_send(client.socket, &client.endpoint, &p);
		}
		halo_ws_closesocket(client.socket);
	}
	memset(&client, 0, sizeof(client));
}
void delta_peer_game_join(unsigned address, unsigned short game_port, int advertised)
{
	delta_peer_game_client_stop();
	client.wanted = advertised && config_boolean("network.delta_peer") && !delta_peer_host_open();
	memset(&client.endpoint, 0, sizeof(client.endpoint)); client.endpoint.sin_family = AF_INET;
	client.endpoint.sin_addr.s_addr = halo_ws_htonl(address);
	client.endpoint.sin_port = halo_ws_htons((unsigned short)(game_port + DELTA_PEER_PORT_OFFSET));
}
void delta_peer_game_client_idle(int machine, const unsigned char *has_players, unsigned now)
{
	unsigned char bytes[DELTA_PEER_MAX_PACKET + 1];
	int i;
	if (!client.wanted || machine < 0 || machine >= DELTA_PEER_MACHINES) return;
	if (!config_boolean("network.delta_peer")) { delta_peer_game_client_stop(); return; }
	for (i = 0; i < DELTA_PEER_MACHINES; i++)
		if (!has_players[i] || now - client.platform_seen[i] >= 12000) client.platforms[i] = 0;
	if (!client.initialized)
	{
		unsigned session = 0;
		client.socket = socket_open(0);
		if (client.socket == INVALID_SOCKET) { client.wanted = 0; return; }
		client.initialized = 1; identity_make(&client.identity);
		if (XNetRandom((BYTE *)&session, sizeof(session)) || !session)
		{ delta_peer_game_client_stop(); return; }
		delta_peer_client_start(&client.session, session, now);
	}
	if (delta_peer_client_tick(&client.session, now, machine))
	{
		struct delta_peer_packet p = client.identity;
		p.type = DELTA_PEER_HELLO; p.machine = (unsigned char)machine; p.session = client.session.session;
		packet_send(client.socket, &client.endpoint, &p);
		platform_log("Delta Peer: HELLO sent (machine %d)", machine);
	}
	if (client.session.state == DELTA_PEER_FALLBACK)
	{ platform_log("Delta Peer: no WELCOME, playing without it"); delta_peer_game_client_stop(); return; }
	for (i = 0; i < 64; i++)
	{
		struct sockaddr_in from;
		struct delta_peer_packet p;
		int length = sizeof(from), n;
		unsigned j;
		memset(&from, 0, sizeof(from));
		n = halo_ws_recvfrom(client.socket, (char *)bytes, sizeof(bytes), 0, (struct sockaddr *)&from, &length);
		if (n < 0) break;
		if (length != sizeof(from) || from.sin_family != AF_INET ||
			from.sin_addr.s_addr != client.endpoint.sin_addr.s_addr || from.sin_port != client.endpoint.sin_port ||
			!delta_peer_decode(&p, bytes, (unsigned)n) || !delta_peer_client_receive(&client.session, &p, now)) continue;
		if (client.session.state == DELTA_PEER_FALLBACK) { delta_peer_game_client_stop(); return; }
		if (p.type == DELTA_PEER_WELCOME)
			platform_log("Delta Peer: WELCOME received (host platform %u, agreed 0x%x)", p.key[0], client.session.agreed);
		if (p.type == DELTA_PEER_WELCOME && (client.session.agreed & DELTA_CAP_PLATFORM))
		{
			if (has_players[machine]) client.platforms[machine] = client.identity.key[0];
			client.platform_seen[machine] = now;
			if (p.machine < DELTA_PEER_MACHINES && has_players[p.machine])
			{ client.platforms[p.machine] = p.key[0]; client.platform_seen[p.machine] = now; }
		}
		if (p.type != DELTA_PEER_ROSTER) continue;
		for (j = 0; j < p.count; j++)
		{
			const struct delta_peer_entry *e = &p.entries[j];
			client.platform_seen[e->machine] = now;
			client.platforms[e->machine] = has_players[e->machine] && (e->flags & 3) == 3 &&
				(e->agreed & client.session.agreed & DELTA_CAP_PLATFORM) ? e->key[0] : 0;
			platform_log("Delta Peer: ROSTER entry machine %u platform %u", e->machine, client.platforms[e->machine]);
		}
	}
}
unsigned delta_peer_machine_platform(int machine)
{
	if (machine < 0 || machine >= DELTA_PEER_MACHINES) return 0;
	if (delta_peer_host_open())
	{
		if (!host.present[machine]) return 0;
		if (machine == host.local) return host.identity.key[0];
		return host.peers[machine].state == DELTA_PEER_ACTIVE &&
			(host.peers[machine].agreed & DELTA_CAP_PLATFORM) ? host.peers[machine].key[0] : 0;
	}
	return client.session.state == DELTA_PEER_ACTIVE ? client.platforms[machine] : 0;
}
