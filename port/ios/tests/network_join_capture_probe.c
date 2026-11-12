/* A network version 43 join request as a version 43 client sent it to a
version 43 host (one TCP frame, observed on the wire), checked against this
build's join helpers: the host reads its version field, admits it only when
hosting 43, and the frame is exactly what this build sends for 43. */
#include "../../linux/include/halo_network_join_wire.h"
#include "../../linux/include/halo_network_version.h"
#include <assert.h>
#include <stdio.h>

/* the frame's first 115 bytes: length word, version byte, 112-byte body;
then 234 zero bytes, the version word 0x002b and the type trailer 0x0c.
The 32 hex digits of the key hash in the body are replaced by a placeholder. */
static unsigned char const captured_head[] = {
	0x16, 0x0c, 0x01, 0x00, 0x63, 0x00, 0x6c, 0x00, 0x69, 0x00, 0x65, 0x00,
	0x6e, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6d, 0x65, 0x73, 0x73, 0x61,
	0x67, 0x65, 0x20, 0x69, 0x6e, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x74, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x61, 0x62, 0x63,
	0x64, 0x65, 0x66, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
};

int main(void)
{
	unsigned char frame[352], built[350], body[114], old_client[350];
	unsigned index;
	size_t size;

	assert(sizeof(captured_head) == 115);
	memset(frame, 0, sizeof(frame));
	memcpy(frame, captured_head, sizeof(captured_head));
	frame[349] = 0x00;
	frame[350] = 0x2b;
	frame[351] = 0x0c;
	/* the length word: 352 bytes, type 12 (join request) */
	assert((((unsigned)frame[0] << 8 | frame[1]) >> 4) == sizeof(frame));
	assert((frame[1] & 0xf) == 0xc);

	/* the host reads 43 and admits it only when hosting 43 */
	assert(halo_network_join_request_read_version(frame, sizeof(frame)) == HALO_NETWORK_VERSION_RIDING);
	assert(halo_network_join_version_verdict(43, 43) == HALO_NETWORK_JOIN_VERSION_ADMIT);
	assert(halo_network_join_version_verdict(42, halo_network_join_request_read_version(frame, sizeof(frame))) ==
		HALO_NETWORK_JOIN_VERSION_TOO_NEW);
	assert(halo_network_join_version_verdict(44, halo_network_join_request_read_version(frame, sizeof(frame))) ==
		HALO_NETWORK_JOIN_VERSION_TOO_OLD);
	/* the same request with a 41 version word is too old for a 43 host */
	frame[350] = 41;
	assert(halo_network_join_version_verdict(43, halo_network_join_request_read_version(frame, sizeof(frame))) ==
		HALO_NETWORK_JOIN_VERSION_TOO_OLD);
	frame[350] = 0x2b;

	/* a client before 42 sends 350 bytes (pass field, no version): it reads
	as 0 and a 43 host refuses it as too old */
	memcpy(old_client, frame, 349);
	old_client[349] = 0x0c;
	old_client[0] = (unsigned char)((350 << 4) >> 8);
	old_client[1] = (unsigned char)(((350 << 4) & 0xf0) | 0xc);
	assert(halo_network_join_request_read_version(old_client, sizeof(old_client)) == 0);
	assert(halo_network_join_version_verdict(43, 0) == HALO_NETWORK_JOIN_VERSION_TOO_OLD);
	/* and a 116-byte request from a client before 40 */
	assert(halo_network_join_request_read_version(frame, HALO_NETWORK_JOIN_REQUEST_FRAME_SIZE) == 0);

	/* this build pads the same 114-byte encoded request (version byte, body,
	trailer) to the same bytes: the host decodes the body and trailer exactly
	as it does a 116-byte request */
	memcpy(body, frame + 2, 113);
	body[113] = 0x0c;
	size = halo_network_join_request_pad(body, sizeof(body), HALO_NETWORK_JOIN_PASS_FIELD_SIZE, built, sizeof(built));
	assert(size == 348);
	size = halo_network_join_request_append_version(built, size, sizeof(built), HALO_NETWORK_VERSION_RIDING);
	assert(size == 350);
	assert(size + 2 == halo_network_join_request_wire_size(halo_network_version_has_join_pass_field(43),
		halo_network_version_has_join_version_field(43)));
	for (index = 0; index < size; index++)
		assert(built[index] == frame[index + 2]);
	puts("PASS: captured 43 join request");
	return 0;
}
