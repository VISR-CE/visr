/* Client join padding and player-facing refusals from observed wire formats. */
#ifndef __HALO_NETWORK_JOIN_WIRE_H
#define __HALO_NETWORK_JOIN_WIRE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define HALO_NETWORK_JOIN_PASS_FIELD_SIZE 234

/* Client-only codes: the hosting protocol's rejection range stays unchanged. */
enum
{
	HALO_NETWORK_REJECTION_LINK_REQUIRED = 8,
	HALO_NETWORK_REJECTION_VPN_BLOCKED = 9,
	HALO_NETWORK_REJECTION_LINK_INVALID = 10,
};

static inline size_t halo_network_join_request_pad(unsigned char const *encoded, size_t size,
	size_t extra, unsigned char *out, size_t out_capacity)
{
	unsigned char trailer;

	if (size < 2 || size > out_capacity || extra > out_capacity - size)
		return 0;
	/* Save the trailer so callers can pad the encoder's buffer in place. */
	trailer = encoded[size - 1];
	memmove(out, encoded, size - 1);
	memset(out + size - 1, 0, extra);
	out[size + extra - 1] = trailer;
	return size + extra;
}

static inline char const *halo_network_rejection_message(unsigned code)
{
	switch (code)
	{
	case HALO_NETWORK_REJECTION_LINK_REQUIRED:
		return "This server requires a linked Discord account, which VISR does not support yet.";
	case HALO_NETWORK_REJECTION_VPN_BLOCKED:
		return "This server doesn't allow VPN connections.";
	case HALO_NETWORK_REJECTION_LINK_INVALID:
		return "This server rejected your Discord link, which VISR does not support yet.";
	case 7:
	case 11:
	case 12:
	case 13:
		return "The server refused your join request.";
	default:
		return NULL;
	}
}

#endif
