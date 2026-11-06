/* Delta advertisement and platform identifiers, from the Delta protocol specification. */
#ifndef HALO_DELTA_H
#define HALO_DELTA_H

#include "halo_port_limits.h"

#define DELTA_MAJOR 1
/* Hosting stays at OpenCE 38: chupa-38a plays with chupa-28a and newer. */
#define DELTA_WIRE_NUMBER 38
#define DELTA_WIRE_LETTER 'a'
#define DELTA_WIRE_OLDEST_NUMBER 28
#define DELTA_WIRE_OLDEST_LETTER 'a'
#if HALO_PORT_NETWORK_VERSION != DELTA_WIRE_NUMBER
#error Update the Delta wire for the hosted network version
#endif
#define DELTA_ADVERTISED_TAG_OFFSET 17
#define DELTA_ADVERTISED_TAG_SIZE 9
#define DELTA_ADVERTISED_LAYOUT_VERSION 1
#define DELTA_ADVERTISED_CHECK_SEED 0xA5
#define DELTA_PLATFORM_KEY_SIZE 8
#define DELTA_PLATFORM_KEY_VERSION 1
#define DELTA_PLATFORM_OPTED_IN 0x01
#define DELTA_PLATFORM_DEDICATED 0x02
/* Byte 7 stays zero, reserved for Milenko's planned client ID; encoding is unspecified. */
#define DELTA_PLATFORM_CLIENT_ID_RESERVED_OFFSET 7

enum delta_platform
{
	_delta_platform_unknown = 0,
	_delta_platform_pc_windows = 1,
	_delta_platform_pc_macos = 2,
	_delta_platform_pc_linux = 3,
	_delta_platform_android = 4,
	_delta_platform_steam_deck = 5,
	_delta_platform_xbox = 6,
	_delta_platform_xbox360 = 7,
	_delta_platform_wiiu = 8,
	_delta_platform_switch = 9,
	_delta_platform_psp = 10,
	_delta_platform_vita = 11,
	_delta_platform_xbox_uwp = 12,
	_delta_platform_pc_freebsd = 13,
	_delta_platform_ps3 = 14,
	_delta_platform_ps4 = 15,
	_delta_platform_ps5 = 16,
	_delta_platform_n64 = 17,
	_delta_platform_pc_legacy = 18,
	_delta_platform_steamlink = 19,
	_delta_platform_ios = 20,
	_delta_platform_tvos = 21,
	_delta_platform_visionos = 22
};

struct delta_advertised_tag
{
	unsigned char major, wire_number, wire_letter, oldest_number, oldest_letter;
};

/* Only the tail belongs to Delta; the OpenCE version, flags and kind stay intact. */
static inline void delta_advertise(unsigned char *reserved)
{
	unsigned char *tag = reserved + DELTA_ADVERTISED_TAG_OFFSET;
	unsigned char check = DELTA_ADVERTISED_CHECK_SEED;
	int index;

	tag[0] = 'D'; tag[1] = 'L'; tag[2] = DELTA_MAJOR;
	tag[3] = DELTA_ADVERTISED_LAYOUT_VERSION;
	tag[4] = DELTA_WIRE_NUMBER; tag[5] = DELTA_WIRE_LETTER;
	tag[6] = DELTA_WIRE_OLDEST_NUMBER; tag[7] = DELTA_WIRE_OLDEST_LETTER;
	for (index = 0; index < DELTA_ADVERTISED_TAG_SIZE - 1; index++)
		check ^= tag[index];
	tag[8] = check;
}

/* Bounded independently of the advertisement struct for hostile or truncated input. */
static inline int delta_advertised_tag_parse(const unsigned char *reserved, unsigned size,
	struct delta_advertised_tag *result)
{
	const unsigned char *tag;
	unsigned char check = DELTA_ADVERTISED_CHECK_SEED;
	int index;

	if (!reserved || !result || size < DELTA_ADVERTISED_TAG_OFFSET + DELTA_ADVERTISED_TAG_SIZE)
		return 0;
	tag = reserved + DELTA_ADVERTISED_TAG_OFFSET;
	for (index = 0; index < DELTA_ADVERTISED_TAG_SIZE - 1; index++)
		check ^= tag[index];
	if (tag[0] != 'D' || tag[1] != 'L' || !tag[2] ||
		tag[3] != DELTA_ADVERTISED_LAYOUT_VERSION || tag[8] != check ||
		!tag[4] || !tag[6] || tag[5] < 'a' || tag[5] > 'z' || tag[7] < 'a' || tag[7] > 'z' ||
		tag[6] > tag[4] || (tag[6] == tag[4] && tag[7] > tag[5]))
		return 0;
	result->major = tag[2]; result->wire_number = tag[4]; result->wire_letter = tag[5];
	result->oldest_number = tag[6]; result->oldest_letter = tag[7];
	return 1;
}
#endif
