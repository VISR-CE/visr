/* Client join padding and player-facing refusals from observed wire formats. */
#ifndef __HALO_NETWORK_JOIN_WIRE_H
#define __HALO_NETWORK_JOIN_WIRE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define HALO_NETWORK_JOIN_PASS_FIELD_SIZE 234
/* the client's network version after the pass field (from 42), big-endian */
#define HALO_NETWORK_JOIN_VERSION_FIELD_SIZE 2
/* the join request's TCP frame without the pass field: length word, version
byte, 112-byte body, type trailer */
#define HALO_NETWORK_JOIN_REQUEST_FRAME_SIZE 116

/* the join request's size on the wire to a host of this network version
(halo_network_version_has_join_pass_field: from 40;
halo_network_version_has_join_version_field: from 42) */
static inline unsigned halo_network_join_request_wire_size(int has_pass_field, int has_version_field)
{
	return HALO_NETWORK_JOIN_REQUEST_FRAME_SIZE + (has_pass_field ? HALO_NETWORK_JOIN_PASS_FIELD_SIZE : 0) +
		(has_version_field ? HALO_NETWORK_JOIN_VERSION_FIELD_SIZE : 0);
}

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

/* Inserts the client's network version, big-endian, before the type trailer
of a padded join request in place; returns the new size, or 0 when it does
not fit. */
static inline size_t halo_network_join_request_append_version(unsigned char *message, size_t size,
	size_t capacity, unsigned version)
{
	unsigned char trailer;

	if (size < 2 || size > capacity || capacity - size < HALO_NETWORK_JOIN_VERSION_FIELD_SIZE || version > 0xFFFFu)
		return 0;
	trailer = message[size - 1];
	message[size - 1] = (unsigned char)(version >> 8);
	message[size] = (unsigned char)(version & 0xFFu);
	message[size + 1] = trailer;
	return size + HALO_NETWORK_JOIN_VERSION_FIELD_SIZE;
}

/* Host side: reads the client's network version from a whole join request
(header word through type trailer). A request without the field (a client
before 42) reads as 0. Returns the version, or 0 when the size does not hold
the field. */
static inline unsigned halo_network_join_request_read_version(unsigned char const *message, size_t size)
{
	if (size != halo_network_join_request_wire_size(1, 1))
		return 0;
	return ((unsigned)message[size - 3] << 8) | message[size - 2];
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
	case 13:
		return "Too many players from your address are in this game.";
	case 7:
	case 11:
	case 12:
		return "The server refused your join request.";
	default:
		return NULL;
	}
}

#endif
