/* Join padding and refusal messages from the observed network frames. */
#include "../../linux/include/halo_network_join_wire.h"
#include "../../linux/include/halo_network_version.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	unsigned char encoded[114], padded[348], unchanged[114], in_place[348];
	unsigned char const refusal[] = { 0x01, 0x00, 0x09, 0x05 };
	unsigned code, index;
	size_t size;

	encoded[0] = 0x01;
	for (index = 1; index <= 112; index++)
		encoded[index] = (unsigned char)(index ^ 0xa5);
	encoded[113] = 0x0c;
	memset(padded, 0xff, sizeof(padded));
	size = halo_network_join_request_pad(encoded, sizeof(encoded), HALO_NETWORK_JOIN_PASS_FIELD_SIZE,
		padded, sizeof(padded));
	assert(size == 348);
	assert(!memcmp(encoded, padded, 113));
	for (index = 113; index <= 346; index++)
		assert(padded[index] == 0);
	assert(padded[347] == 0x0c);
	memcpy(in_place, encoded, sizeof(encoded));
	assert(halo_network_join_request_pad(in_place, sizeof(encoded), 234, in_place, sizeof(in_place)) == size);
	assert(!memcmp(in_place, padded, size));
	assert((uint16_t)(((size + 2) << 4) | 0xc) == 0x15ec);
	size = halo_network_join_request_pad(encoded, sizeof(encoded), 0, unchanged, sizeof(unchanged));
	assert(size == sizeof(encoded) && !memcmp(encoded, unchanged, size));
	assert((uint16_t)(((size + 2) << 4) | 0xc) == 0x074c);
	assert(halo_network_join_request_pad(encoded, sizeof(encoded), 234, padded, 347) == 0);
	assert(halo_network_join_request_pad(encoded, 0, 234, padded, sizeof(padded)) == 0);
	assert(halo_network_join_request_pad(encoded, 1, 234, padded, sizeof(padded)) == 0);
	assert(halo_network_join_request_pad(encoded, sizeof(encoded), SIZE_MAX, padded, sizeof(padded)) == 0);

	assert(!strcmp(halo_network_rejection_message(9), "This server doesn't allow VPN connections."));
	assert(strstr(halo_network_rejection_message(8), "linked Discord"));
	assert(!strcmp(halo_network_rejection_message(8),
		"This server requires a linked Discord account, which VISR does not support yet."));
	assert(!strcmp(halo_network_rejection_message(10),
		"This server rejected your Discord link, which VISR does not support yet."));
	for (code = 0; code <= 6; code++)
		assert(halo_network_rejection_message(code) == NULL);
	for (code = 7; code <= 13; code++)
		assert(halo_network_rejection_message(code) != NULL);
	assert(!strcmp(halo_network_rejection_message(7), "The server refused your join request."));
	for (code = 11; code <= 12; code++)
		assert(!strcmp(halo_network_rejection_message(code), "The server refused your join request."));
	assert(!strcmp(halo_network_rejection_message(13), "Too many players from your address are in this game."));
	assert(halo_network_rejection_message(14) == NULL);
	assert(halo_network_rejection_message(UINT32_MAX) == NULL);
	assert(refusal[0] == 1 && refusal[3] == 5);
	code = ((unsigned)refusal[1] << 8) | refusal[2];
	assert(code == 9);
	assert(!strcmp(halo_network_rejection_message(code), "This server doesn't allow VPN connections."));
	/* the wire size logged for a join: 116 to hosts below 40, 350 to 40 and 41, 352 from 42 */
	for (unsigned version = 29; version <= 44; version++)
		assert(halo_network_join_request_wire_size(halo_network_version_has_join_pass_field(version),
			halo_network_version_has_join_version_field(version)) ==
			(version >= 42 ? 352u : version >= 40 ? 350u : 116u));
	assert(halo_network_join_request_wire_size(0, 0) == 116 && halo_network_join_request_wire_size(1, 0) == 350);
	assert(halo_network_join_request_wire_size(1, 1) == 352);
	/* from 42: the 234 zeros, then the version as a big-endian word, then the trailer */
	{
		unsigned char versioned[350];

		size = halo_network_join_request_pad(encoded, sizeof(encoded), HALO_NETWORK_JOIN_PASS_FIELD_SIZE,
			versioned, sizeof(versioned));
		assert(size == 348);
		size = halo_network_join_request_append_version(versioned, size, sizeof(versioned), 43);
		assert(size == 350);
		assert(!memcmp(encoded, versioned, 113));
		for (index = 113; index <= 346; index++)
			assert(versioned[index] == 0);
		assert(versioned[347] == 0x00 && versioned[348] == 0x2b && versioned[349] == 0x0c);
		/* the frame adds the length word: 352 bytes */
		assert(size + 2 == 352);
		assert(halo_network_join_request_append_version(versioned, 348, 349, 43) == 0);
		assert(halo_network_join_request_append_version(versioned, 1, sizeof(versioned), 43) == 0);
		assert(halo_network_join_request_append_version(versioned, 348, sizeof(versioned), 0x10000) == 0);

		/* the host reads it back from the framed request (length word first) */
		{
			unsigned char framed[352];

			framed[0] = 0x60;
			framed[1] = 0x01;
			memcpy(framed + 2, versioned, sizeof(versioned));
			assert(halo_network_join_request_read_version(framed, sizeof(framed)) == 43);
			/* a client before 42 sends a shorter request: it reads as 0 */
			assert(halo_network_join_request_read_version(framed, 350) == 0);
			assert(halo_network_join_request_read_version(framed, 116) == 0);
			framed[349] = 0x01;
			framed[350] = 0x02;
			assert(halo_network_join_request_read_version(framed, sizeof(framed)) == 0x0102);
		}
	}
	puts("PASS: join padding, frame lengths, refusal messages");
	return 0;
}
