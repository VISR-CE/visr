/* Content fields established by the version 38 black-box format description.
   These helpers do not implement the new game modes or shield presentations. */
#ifndef HALO_NETWORK_CONTENT_H
#define HALO_NETWORK_CONTENT_H

#include "halo_port_limits.h"

#define HALO_NETWORK_LEGACY_OPTIONS_SIZE 28
#define HALO_NETWORK_MODE_OPTIONS_SIZE 16
#define HALO_NETWORK_OPTIONS_SIZE 44
#define HALO_NETWORK_LEGACY_SETTINGS_SIZE (HALO_PORT_NETWORK_GAME_SIZE - HALO_NETWORK_MODE_OPTIONS_SIZE)

/* Each advertised kind's names in one row: the capitals the sort key and the HC text use, the case the menus show a
game's type in, and why this build cannot play it (NULL when it can). */
struct halo_network_kind_row
{
	char const *label, *title, *reason;
};

static inline struct halo_network_kind_row const *halo_network_kind_row_get(unsigned kind)
{
	static struct halo_network_kind_row const unsupported = { "UNSUPPORTED", "Unsupported", "This game has an unknown game kind." };
	static struct halo_network_kind_row const hardcore = { "HARDCORE", "Hardcore", "Hardcore rules are not supported by VISR." };
	static struct halo_network_kind_row const kinds[] = {
		{ "", "", 0 },
		{ "SWAT", "SWAT", "SWAT rules are not supported by VISR." },
		{ "INFECTION", "Infection", "Infection is not supported by VISR." },
		{ "GUN GAME", "Gun Game", "Gun Game is not supported by VISR." },
	};

	if (kind & 0x7C || kind > 0xFF) return &unsupported;
	if (kind & 0x80) return &hardcore;
	return kind < sizeof(kinds) / sizeof(kinds[0]) ? &kinds[kind] : &unsupported;
}

static inline char const *halo_network_kind_label(unsigned kind) { return halo_network_kind_row_get(kind)->label; }
static inline char const *halo_network_kind_title(unsigned kind) { return halo_network_kind_row_get(kind)->title; }
static inline char const *halo_network_kind_reason(unsigned kind) { return halo_network_kind_row_get(kind)->reason; }

static inline void halo_network_options_defaults(unsigned char *extension)
{
	static unsigned char const defaults[HALO_NETWORK_MODE_OPTIONS_SIZE] =
		{0, 0, 1, 150, 200, 5, 10, 1, 0, 1, 0, 0, 0, 0, 0, 0};
	unsigned i;
	for (i = 0; i < HALO_NETWORK_MODE_OPTIONS_SIZE; i++) extension[i] = defaults[i];
}

static inline char const *halo_network_options_reason(unsigned char const *extension)
{
	unsigned i;
	char const *reason = halo_network_kind_reason(extension[0]);
	if (reason) return reason;
	if (extension[1]) return "Hardcore rules are not supported by VISR.";
	/* Inactive Infection and Gun Game parameters are preserved, not enabled.
	   The six trailing bytes have no established meaning; refuse nonzero data. */
	for (i = 10; i < HALO_NETWORK_MODE_OPTIONS_SIZE; i++)
		if (extension[i]) return "This game uses options that VISR does not support.";
	return 0;
}

/* The 29–34 record has a 28-byte options block and the same four-byte
   local tail. Move that tail before filling the new extension. */
static inline void halo_network_settings_expand_legacy(unsigned char *settings)
{
	unsigned i;
	for (i = 0; i < 4; i++)
		settings[HALO_PORT_NETWORK_GAME_LOCAL_DATA_OFFSET + i] =
			settings[HALO_PORT_NETWORK_GAME_LOCAL_DATA_OFFSET - HALO_NETWORK_MODE_OPTIONS_SIZE + i];
	halo_network_options_defaults(settings + HALO_PORT_NETWORK_GAME_VARIANT_OPTIONS_OFFSET + HALO_NETWORK_LEGACY_OPTIONS_SIZE);
}

/* From network version 42 the settings record ends with two 32-byte map
   digests (contract by chester, GulchCE). They are zero for Xbox and
   Custom Edition maps; anything else is an MCC map. */
#define HALO_NETWORK_SETTINGS_DIGESTS_SIZE 64

static inline char const *halo_network_settings_digests_reason(unsigned char const *digests)
{
	unsigned i;
	for (i = 0; i < HALO_NETWORK_SETTINGS_DIGESTS_SIZE; i++)
		if (digests[i]) return "This game uses an MCC map, which VISR does not support.";
	return 0;
}

/* From network version 43 a host's vehicle riding switch (OpenCE PR #637)
   travels in bit 6 of the variant's universal flags, a bit the Xbox left
   spare: record offset 196, mask 0x40 (confirmed by capture of an OpenCE
   release dedicated server). VISR does not simulate riding yet. */
#define HALO_NETWORK_SETTINGS_RIDING_OFFSET 196
#define HALO_NETWORK_SETTINGS_RIDING_MASK 0x40

static inline char const *halo_network_settings_riding_reason(unsigned char const *settings)
{
	if (settings[HALO_NETWORK_SETTINGS_RIDING_OFFSET] & HALO_NETWORK_SETTINGS_RIDING_MASK)
		return "This game has vehicle riding on, which VISR does not support yet.";
	return 0;
}

#endif
