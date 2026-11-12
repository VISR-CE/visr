#include "delta_peer.h"
#include <string.h>

static unsigned get16(const unsigned char *b)
{
	return b[0] | ((unsigned)b[1] << 8);
}
static unsigned get32(const unsigned char *b)
{
	return get16(b) | (get16(b + 2) << 16);
}
static void put16(unsigned char *b, unsigned n)
{
	b[0] = (unsigned char)n; b[1] = (unsigned char)(n >> 8);
}
static void put32(unsigned char *b, unsigned n)
{
	put16(b, n); put16(b + 2, n >> 16);
}
static int machine_valid(unsigned m)
{
	return m < DELTA_PEER_MACHINES;
}
static int build_valid(const char *b, unsigned n)
{
	unsigned i;
	if (n > 31) return 0;
	for (i = 0; i < n; i++)
		if ((unsigned char)b[i] < 32 || (unsigned char)b[i] > 126) return 0;
	return 1;
}
int delta_peer_decode(struct delta_peer_packet *p, const unsigned char *b, unsigned size)
{
	unsigned n, fixed, length, i;
	unsigned char seen[DELTA_PEER_MACHINES] = {0};
	if (!p || !b || size < 12 || size > DELTA_PEER_MAX_PACKET ||
		b[0] != 'D' || b[1] != 'P' || !b[2] || get16(b + 4) != size - 12 || !get32(b + 8))
		return 0;
	memset(p, 0, sizeof(*p));
	p->major = b[2]; p->type = b[3]; p->session = get32(b + 8);
	/* A foreign major shares only the header. Its payload may have changed. */
	if (p->major != DELTA_MAJOR) return 1;
	n = size - 12; b += 12;
	switch (p->type)
	{
	case DELTA_PEER_HELLO:
	case DELTA_PEER_WELCOME:
		fixed = p->type == DELTA_PEER_HELLO ? 20 : 24;
		if (n < fixed) return 0;
		p->capabilities = get32(b);
		if (p->type == DELTA_PEER_WELCOME) { p->agreed = get32(b + 4); b += 4; }
		p->legacy = (unsigned short)get16(b + 4); p->machine = b[6]; length = b[7];
		if ((!machine_valid(p->machine) && !(p->type == DELTA_PEER_WELCOME && p->machine == 255)) ||
			length > n - fixed || !build_valid((const char *)b + 20, length)) return 0;
		p->serial = get32(b + 8); memcpy(p->key, b + 12, 8);
		memcpy(p->build, b + 20, length);
		return 1;
	case DELTA_PEER_ROSTER:
		if (n < 12 || b[8] > DELTA_PEER_MAX_ENTRIES || n - 12 < 36U * b[8]) return 0;
		p->room = get32(b); p->limit = b[4]; p->count = b[8]; b += 12;
		for (i = 0; i < p->count; i++, b += 36)
		{
			struct delta_peer_entry *e = &p->entries[i];
			if (!machine_valid(b[0]) || seen[b[0]]) return 0;
			seen[b[0]] = 1; e->machine = b[0]; e->flags = b[1]; e->agreed = get32(b + 4);
			memcpy(e->key, b + 8, 8); e->revision = get32(b + 16); memcpy(e->player_id, b + 20, 16);
		}
		return 1;
	default:
		/* Unknown kinds and appended fields are ignored, as the wire requires. */
		return 1;
	}
}
int delta_peer_encode(unsigned char *b, unsigned capacity, const struct delta_peer_packet *p)
{
	unsigned n = 0, length = 0, i;
	unsigned char *out;
	if (!b || !p || !p->session || !p->major) return 0;
	switch (p->type)
	{
	case DELTA_PEER_HELLO:
	case DELTA_PEER_WELCOME:
		while (length < sizeof(p->build) && p->build[length]) length++;
		if (!build_valid(p->build, length) ||
			(!machine_valid(p->machine) && !(p->type == DELTA_PEER_WELCOME && p->machine == 255))) return 0;
		n = (p->type == DELTA_PEER_HELLO ? 20 : 24) + length;
		break;
	case DELTA_PEER_LEGACY:
	case DELTA_PEER_BYE: break;
	case DELTA_PEER_ROSTER:
		if (p->count > DELTA_PEER_MAX_ENTRIES) return 0;
		n = 12 + 36U * p->count;
		for (i = 0; i < p->count; i++)
		{
			unsigned j;
			if (!machine_valid(p->entries[i].machine)) return 0;
			for (j = 0; j < i; j++)
				if (p->entries[j].machine == p->entries[i].machine) return 0;
		}
		break;
	default: return 0;
	}
	if (capacity < 12 + n || 12 + n > DELTA_PEER_MAX_PACKET) return 0;
	memset(b, 0, 12 + n); b[0] = 'D'; b[1] = 'P'; b[2] = p->major; b[3] = p->type;
	put16(b + 4, n); put32(b + 8, p->session); out = b + 12;
	if (p->type == DELTA_PEER_HELLO || p->type == DELTA_PEER_WELCOME)
	{
		put32(out, p->capabilities);
		if (p->type == DELTA_PEER_WELCOME) { put32(out + 4, p->agreed); out += 4; }
		put16(out + 4, p->legacy); out[6] = p->machine; out[7] = (unsigned char)length;
		put32(out + 8, p->serial); memcpy(out + 12, p->key, 8); memcpy(out + 20, p->build, length);
	}
	else if (p->type == DELTA_PEER_ROSTER)
	{
		put32(out, p->room); out[4] = p->limit; out[8] = p->count; out += 12;
		for (i = 0; i < p->count; i++, out += 36)
		{
			const struct delta_peer_entry *e = &p->entries[i];
			out[0] = e->machine; out[1] = e->flags; put32(out + 4, e->agreed);
			memcpy(out + 8, e->key, 8); put32(out + 16, e->revision); memcpy(out + 20, e->player_id, 16);
		}
	}
	return (int)(12 + n);
}

int delta_peer_rate_take(struct delta_peer_rate *rate, unsigned now, unsigned per_second, unsigned burst)
{
	unsigned elapsed, refill;
	if (!per_second || !burst) return 0;
	if (!rate->initialized)
	{
		rate->initialized = 1; rate->time = now; rate->tokens = burst;
	}
	elapsed = now - rate->time;
	/* Bound multiplication even after days without traffic or a clock wrap. */
	if (elapsed >= (burst * 1000U + per_second - 1) / per_second)
	{ rate->tokens = burst; rate->time = now; }
	else
	{
		refill = elapsed * per_second / 1000;
		if (refill)
		{
			rate->tokens = rate->tokens + refill > burst ? burst : rate->tokens + refill;
			rate->time += refill * 1000 / per_second;
		}
	}
	if (!rate->tokens) return 0;
	rate->tokens--; return 1;
}
void delta_peer_client_start(struct delta_peer_session *s, unsigned session, unsigned now)
{
	memset(s, 0, sizeof(*s)); s->session = session; s->started = now;
	s->state = session ? DELTA_PEER_WAITING : DELTA_PEER_FALLBACK;
}
int delta_peer_client_tick(struct delta_peer_session *s, unsigned now, int machine)
{
	if (s->state != DELTA_PEER_WAITING || machine < 0 || machine >= DELTA_PEER_MACHINES) return 0;
	/* The timeout starts when the machine index exists and HELLO can be sent. */
	if (!s->has_sent) s->started = now;
	if (now - s->started >= 4000) { s->state = DELTA_PEER_FALLBACK; return 0; }
	if (s->has_sent && now - s->sent < 500) return 0;
	s->sent = now; s->has_sent = 1; return 1;
}
int delta_peer_client_receive(struct delta_peer_session *s, const struct delta_peer_packet *p, unsigned now)
{
	if ((s->state != DELTA_PEER_WAITING && s->state != DELTA_PEER_ACTIVE) || p->session != s->session ||
		!delta_peer_rate_take(&s->rate, now, 50, 100)) return 0;
	if (s->state == DELTA_PEER_WAITING && s->has_sent && now - s->started >= 4000)
	{ s->state = DELTA_PEER_FALLBACK; return 0; }
	if (p->type == DELTA_PEER_LEGACY)
	{ s->state = DELTA_PEER_FALLBACK; return 1; }
	if (p->major != DELTA_MAJOR) return 0;
	if (p->type == DELTA_PEER_BYE) { s->state = DELTA_PEER_FALLBACK; return 1; }
	if (p->type == DELTA_PEER_WELCOME && s->state == DELTA_PEER_WAITING)
	{
		if ((p->agreed & ~DELTA_CAP_PLATFORM) || (p->agreed & ~p->capabilities)) return 0;
		s->state = DELTA_PEER_ACTIVE; s->agreed = p->agreed;
		memcpy(s->key, p->key, 8); return 1;
	}
	return p->type == DELTA_PEER_ROSTER && s->state == DELTA_PEER_ACTIVE;
}
int delta_peer_host_receive(struct delta_peer_session peers[DELTA_PEER_MACHINES],
	const unsigned addresses[DELTA_PEER_MACHINES], unsigned sender,
	const unsigned char *bytes, unsigned size, unsigned now,
	const struct delta_peer_packet *identity, struct delta_peer_packet *reply, int *changed)
{
	struct delta_peer_packet p;
	struct delta_peer_session *s;
	int i, machine = -1, members = 0;
	*changed = 0; reply->type = 0;
	if (!sender) return -1;
	for (i = 0; i < DELTA_PEER_MACHINES; i++)
		if (addresses[i] == sender) { machine = i; members++; }
	if (!members || !delta_peer_decode(&p, bytes, size)) return -1;
	/* A foreign major shares only the header, so its machine cannot be
	identified safely when several machines share the address. */
	if (p.type == DELTA_PEER_HELLO)
	{
		if (p.major == DELTA_MAJOR)
		{
			machine = p.machine;
			if (!machine_valid(machine) || addresses[machine] != sender) return -1;
		}
		else if (members != 1) return -1;
	}
	else
	{
		for (machine = 0; machine < DELTA_PEER_MACHINES; machine++)
			if (addresses[machine] == sender && peers[machine].session == p.session) break;
		if (machine == DELTA_PEER_MACHINES) return -1;
	}
	s = &peers[machine];
	if (!delta_peer_rate_take(&s->rate, now, 10, 20)) return -1;
	if (p.type == DELTA_PEER_BYE && p.major == DELTA_MAJOR && p.session == s->session)
	{
		*changed = s->state != DELTA_PEER_FALLBACK;
		s->state = DELTA_PEER_FALLBACK; return machine;
	}
	if (p.type != DELTA_PEER_HELLO) return -1;
	if (p.session != s->session)
	{
		if (s->has_changed && now - s->changed < 1000) return -1;
		s->changed = now; s->has_changed = 1; s->session = p.session;
	}
	*reply = *identity; reply->session = p.session;
	if (p.major != DELTA_MAJOR)
	{
		*changed = s->state != DELTA_PEER_FALLBACK;
		reply->type = DELTA_PEER_LEGACY; s->state = DELTA_PEER_FALLBACK; return machine;
	}
	*changed = s->state != DELTA_PEER_ACTIVE ||
		s->agreed != (p.capabilities & identity->capabilities) || memcmp(s->key, p.key, 8) != 0;
	s->state = DELTA_PEER_ACTIVE; s->agreed = p.capabilities & identity->capabilities;
	memcpy(s->key, p.key, 8); reply->type = DELTA_PEER_WELCOME; reply->agreed = s->agreed;
	return machine;
}
