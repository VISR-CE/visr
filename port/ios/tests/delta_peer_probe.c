#include "delta_peer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void wire(void)
{
	/* These bytes come from the wire table, independent of the encoder. */
	static const unsigned char hello[] = {
		0x44,0x50,1,1,21,0,0,0,0x78,0x56,0x34,0x12,
		1,0,0,0,38,0,7,1,0,0,0,0,20,1,0,128,128,128,11,0,'V'};
	static const unsigned char welcome[] = {
		0x44,0x50,1,2,25,0,0,0,0x78,0x56,0x34,0x12,
		1,0,0,0,1,0,0,0,38,0,255,1,0,0,0,0,20,1,0,128,128,128,11,0,'V'};
	static const unsigned char legacy[] = {0x44,0x50,1,3,0,0,0,0,0x78,0x56,0x34,0x12};
	static const unsigned char bye[] = {0x44,0x50,1,5,0,0,0,0,0x78,0x56,0x34,0x12};
	const unsigned char *fixtures[] = {hello, welcome, legacy, bye};
	unsigned sizes[] = {sizeof(hello), sizeof(welcome), sizeof(legacy), sizeof(bye)};
	unsigned char bytes[1201];
	struct delta_peer_packet p, q;
	unsigned i, j;
	for (i = 0; i < 4; i++)
	{
		assert(delta_peer_decode(&p, fixtures[i], sizes[i]));
		assert(delta_peer_encode(bytes, sizeof(bytes), &p) == (int)sizes[i]);
		assert(!memcmp(bytes, fixtures[i], sizes[i]));
		for (j = 0; j < sizes[i]; j++)
		{
			assert(!delta_peer_decode(&q, bytes, j));
			assert(!delta_peer_encode(bytes, j, &p));
		}
		if (i < 2)
		{
			for (j = 12; j < sizes[i]; j++)
			{
				memcpy(bytes, fixtures[i], sizes[i]); bytes[4] = (unsigned char)(j - 12);
				assert(!delta_peer_decode(&q, bytes, j));
			}
		}
		memcpy(bytes, fixtures[i], sizes[i]);
		bytes[2] = 0; assert(!delta_peer_decode(&q, bytes, sizes[i])); bytes[2] = 1;
		bytes[0] = 0; assert(!delta_peer_decode(&q, bytes, sizes[i])); bytes[0] = 'D';
		bytes[4]++; assert(!delta_peer_decode(&q, bytes, sizes[i])); bytes[4]--;
		memset(bytes + 8, 0, 4); assert(!delta_peer_decode(&q, bytes, sizes[i]));
	}
	memcpy(bytes, hello, sizeof(hello));
	bytes[19] = 32; assert(!delta_peer_decode(&q, bytes, sizeof(hello)));
	bytes[19] = 1; bytes[32] = 31; assert(!delta_peer_decode(&q, bytes, sizeof(hello)));
	bytes[32] = 127; assert(!delta_peer_decode(&q, bytes, sizeof(hello)));
	bytes[32] = 'V'; bytes[2] = 2;
	assert(delta_peer_decode(&q, bytes, sizeof(hello)) && q.major == 2);
	/* Appended fields are allowed, but the framing length must still match. */
	bytes[2] = 1; bytes[4]++; bytes[sizeof(hello)] = 99;
	assert(delta_peer_decode(&q, bytes, sizeof(hello) + 1));
	assert(!delta_peer_decode(&q, NULL, 12));
	assert(!delta_peer_decode(NULL, bytes, sizeof(hello)));
	memset(&p, 0, sizeof(p)); p.major = 1; p.type = 4; p.session = 1;
	p.limit = 128; p.count = 2; p.room = 1;
	p.entries[0].machine = 0; p.entries[0].flags = 3; p.entries[0].key[0] = 20;
	p.entries[1].machine = 127; p.entries[1].flags = 3; p.entries[1].key[0] = 22;
	assert(delta_peer_encode(bytes, sizeof(bytes), &p) == 96);
	assert(delta_peer_decode(&q, bytes, 96) && q.entries[1].key[0] == 22);
	for (i = 0; i < 96; i++) assert(!delta_peer_decode(&q, bytes, i));
	bytes[20] = 33; assert(!delta_peer_decode(&q, bytes, 96));
	p.count = 33; assert(!delta_peer_encode(bytes, sizeof(bytes), &p));
}

static void sessions(void)
{
	struct delta_peer_session client, peers[128];
	struct delta_peer_packet hello, identity, reply;
	unsigned addresses[128] = {0};
	unsigned char bytes[1200];
	unsigned now;
	int n;
	memset(peers, 0, sizeof(peers)); memset(&identity, 0, sizeof(identity));
	memset(&hello, 0, sizeof(hello));
	identity.major = 1; identity.capabilities = 1; identity.key[0] = 2; identity.machine = 0;
	hello = identity; hello.type = 1; hello.machine = 7; hello.session = 123; hello.key[0] = 20;
	addresses[7] = 42;
	n = delta_peer_encode(bytes, sizeof(bytes), &hello);
	assert(n);
	assert(!delta_peer_host_receive(peers, addresses, 41, bytes, n, 0, &identity, &reply));
	assert(!peers[7].state);
	assert(delta_peer_host_receive(peers, addresses, 42, bytes, n, 0, &identity, &reply));
	assert(reply.type == 2 && reply.agreed == 1 && reply.session == 123);
	assert(peers[7].key[0] == 20);
	hello.session++; n = delta_peer_encode(bytes, sizeof(bytes), &hello);
	assert(!delta_peer_host_receive(peers, addresses, 42, bytes, n, 999, &identity, &reply));
	assert(delta_peer_host_receive(peers, addresses, 42, bytes, n, 1000, &identity, &reply));
	hello.major = 2; hello.session++; n = delta_peer_encode(bytes, sizeof(bytes), &hello);
	assert(delta_peer_host_receive(peers, addresses, 42, bytes, n, 2000, &identity, &reply));
	assert(reply.type == 3 && reply.major == 1);
	delta_peer_client_start(&client, 123, 0);
	assert(!delta_peer_client_tick(&client, 0, -1));
	for (now = 0; now < 4000; now += 500)
	{
		assert(delta_peer_client_tick(&client, now, 7));
		assert(!delta_peer_client_tick(&client, now + 499, 7));
	}
	assert(!delta_peer_client_tick(&client, 4000, 7) && client.state == DELTA_PEER_FALLBACK);
	reply.major = 1; reply.type = 2; reply.session = 123; reply.agreed = 1;
	assert(!delta_peer_client_receive(&client, &reply, 4000));
	delta_peer_client_start(&client, 123, 0);
	reply.session++; assert(!delta_peer_client_receive(&client, &reply, 1)); reply.session--;
	reply.major = 2; assert(!delta_peer_client_receive(&client, &reply, 1)); reply.major = 1;
	reply.capabilities = 0;
	assert(!delta_peer_client_receive(&client, &reply, 1));
	reply.capabilities = 1;
	assert(delta_peer_client_receive(&client, &reply, 1) && client.state == DELTA_PEER_ACTIVE);
	reply.type = 5; assert(delta_peer_client_receive(&client, &reply, 2));
	assert(client.state == DELTA_PEER_FALLBACK);
	delta_peer_client_start(&client, 123, 0); reply.type = 3; reply.major = 2;
	assert(delta_peer_client_receive(&client, &reply, 1) && client.state == DELTA_PEER_FALLBACK);
	/* A wrapping millisecond clock still has exactly the same timeout. */
	delta_peer_client_start(&client, 1, 0xfffffff0U);
	assert(delta_peer_client_tick(&client, 0xfffffff0U, 1));
	assert(!delta_peer_client_tick(&client, 3984, 1) && client.state == DELTA_PEER_FALLBACK);
}
static void rate_limits(void)
{
	struct delta_peer_rate rate = {0};
	unsigned i;
	for (i = 0; i < 20; i++) assert(delta_peer_rate_take(&rate, 0, 10, 20));
	assert(!delta_peer_rate_take(&rate, 0, 10, 20));
	assert(!delta_peer_rate_take(&rate, 99, 10, 20));
	assert(delta_peer_rate_take(&rate, 100, 10, 20));
	assert(!delta_peer_rate_take(&rate, 100, 10, 20));
	for (i = 0; i < 10; i++) assert(delta_peer_rate_take(&rate, 1100, 10, 20));
	assert(!delta_peer_rate_take(&rate, 1100, 10, 20));
}

static void advertisement(void)
{
	unsigned char bytes[26] = {38, 0, 4, 1};
	assert(delta_advertised_peer_major(bytes, sizeof(bytes)) == 1);
	bytes[0] = 39;
	assert(!delta_advertised_peer_major(bytes, sizeof(bytes)));
	memcpy(bytes + 4, "chupa-38a", 9);
	assert(delta_advertised_peer_major(bytes, sizeof(bytes)) == 1);
	bytes[3] = 0;
	assert(!delta_advertised_peer_major(bytes, sizeof(bytes)));
	delta_advertise(bytes);
	assert(delta_advertised_peer_major(bytes, sizeof(bytes)) == 1);
	bytes[2] = 0;
	assert(delta_advertised_peer_major(bytes, sizeof(bytes)) == 1);
	bytes[25] ^= 1;
	assert(!delta_advertised_peer_major(bytes, sizeof(bytes)));
}

static void hostile_input(void)
{
	unsigned char bytes[1201], encoded[1200];
	struct delta_peer_packet p;
	unsigned seed = 118, i, j, n;
	for (i = 0; i < 10000; i++)
	{
		seed = seed * 1664525U + 1013904223U; n = seed % sizeof(bytes);
		for (j = 0; j < n; j++)
		{
			seed = seed * 1664525U + 1013904223U; bytes[j] = (unsigned char)(seed >> 24);
		}
		/* Give half the noise a valid header, reaching the payload parsers. */
		if ((i & 1) && n >= 12)
		{
			bytes[0] = 'D'; bytes[1] = 'P'; bytes[2] = 1; bytes[3] = (unsigned char)(1 + i % 5);
			bytes[4] = (unsigned char)(n - 12); bytes[5] = (unsigned char)((n - 12) >> 8);
			bytes[6] = bytes[7] = 0; bytes[8] = 1;
		}
		if (delta_peer_decode(&p, bytes, n)) delta_peer_encode(encoded, sizeof(encoded), &p);
	}
}

int main(void)
{
	wire(); sessions(); rate_limits(); advertisement(); hostile_input(); puts("delta peer probe ok"); return 0;
}
