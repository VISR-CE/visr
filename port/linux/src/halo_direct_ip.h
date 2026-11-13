/* Pure Direct IP parsing. Addresses are network byte order; ports are host
order, like transport_address.port. See port/ios/tests/direct_ip_probe.c. */
#ifndef HALO_DIRECT_IP_H
#define HALO_DIRECT_IP_H
#include <stdint.h>
#include <string.h>

#define HALO_DIRECT_IP_TIMEOUT 15000U

struct halo_direct_target
{
	uint32_t address;
	unsigned short port;
};

static inline int halo_direct_ip_space(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Empty or nonnumeric ports use the form's default. Explicit numeric ports
outside 1..65535 fail instead of wrapping to a different destination. */
static inline int halo_direct_ip_port(const char *text, const char *end, unsigned short *port)
{
	unsigned long value = 0;
	const char *p;
	while (text < end && halo_direct_ip_space(*text)) text++;
	while (end > text && halo_direct_ip_space(end[-1])) end--;
	*port = 5150;
	if (text == end) return 1;
	for (p = text; p < end; p++)
		if (*p < '0' || *p > '9') return 1;
	for (p = text; p < end; p++)
	{
		value = value * 10 + (unsigned long)(*p - '0');
		if (value > 65535) return 0;
	}
	if (!value) return 0;
	*port = (unsigned short)value;
	return 1;
}

static inline int halo_direct_ip_parse(const char *text, const char *port, struct halo_direct_target *target)
{
	struct halo_direct_target parsed;
	unsigned char octets[4];
	const char *end = text + strlen(text);
	int part;
	while (text < end && halo_direct_ip_space(*text)) text++;
	while (end > text && halo_direct_ip_space(end[-1])) end--;
	for (part = 0; part < 4; part++)
	{
		unsigned value = 0, digits = 0;
		while (text < end && *text >= '0' && *text <= '9' && digits < 3)
		{
			value = value * 10 + (unsigned)(*text++ - '0');
			digits++;
		}
		if (!digits || value > 255) return 0;
		octets[part] = (unsigned char)value;
		if (part < 3 && (text == end || *text++ != '.')) return 0;
	}
	/* Unspecified, multicast, and broadcast destinations are not hosts. */
	if (!octets[0] || octets[0] >= 224) return 0;
	if (text < end && *text == ':')
	{
		text++;
		if (!halo_direct_ip_port(text, end, &parsed.port)) return 0;
	}
	else if (text != end || !halo_direct_ip_port(port, port + strlen(port), &parsed.port)) return 0;
	memcpy(&parsed.address, octets, sizeof(octets));
	*target = parsed;
	return 1;
}

/* xnet.c adds this temporary unicast destination to system link searches.
NULL cancels it; it expires even if the player leaves the form. */
void halo_direct_ip_probe_set(const struct halo_direct_target *target);
#endif
