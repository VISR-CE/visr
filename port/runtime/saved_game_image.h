/* The layout of a saved game's header and its checksum, for host code that reads or writes saved games:
   the cloud saves rules (cloud_saves.c) and the saved game writer (save_writer.c). The game's own are
   struct game_state_header (source/saved games/game_state.h) and source/memory/crc.c. */
#ifndef SAVED_GAME_IMAGE_H
#define SAVED_GAME_IMAGE_H

#include <stddef.h>
#include <stdint.h>

enum
{
	SAVED_GAME_HEADER_LAYOUT = 0,
	SAVED_GAME_HEADER_MAP = 4,
	SAVED_GAME_HEADER_MAP_SIZE = 256,
	SAVED_GAME_HEADER_BUILD = 0x104,
	SAVED_GAME_HEADER_BUILD_SIZE = 32,
	SAVED_GAME_HEADER_DIFFICULTY = 0x126,
	SAVED_GAME_HEADER_MAP_CHECKSUM = 0x128,
	SAVED_GAME_HEADER_CHECKSUM = 0x148,
	SAVED_GAME_HEADER_SIZE = 0x14c,
};

#define SAVED_GAME_CRC_START 0xFFFFFFFFu

/* source/memory/crc.c, four bits at a time from a constant table (no table to build, so any thread may call) */
static inline uint32_t saved_game_crc(uint32_t crc, const uint8_t *data, size_t size)
{
	static const uint32_t table[16] = {
		0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
		0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
	};
	size_t i;

	for (i = 0; i < size; i++)
	{
		crc ^= data[i];
		crc = (crc >> 4) ^ table[crc & 15];
		crc = (crc >> 4) ^ table[crc & 15];
	}
	return crc;
}

/* a run of size bytes that begins at offset in the image, added to the checksum: the image's own checksum
   field counts as zero, wherever in the run it falls */
static inline uint32_t saved_game_crc_run(uint32_t crc, const uint8_t *data, size_t offset, size_t size)
{
	static const uint8_t zero[4] = { 0 };
	size_t i = 0;

	while (i < size)
	{
		size_t at = offset + i;

		if (at >= SAVED_GAME_HEADER_CHECKSUM && at < SAVED_GAME_HEADER_CHECKSUM + 4)
		{
			crc = saved_game_crc(crc, zero, 1);
			i++;
		}
		else
		{
			size_t run = size - i;

			if (at < SAVED_GAME_HEADER_CHECKSUM && run > SAVED_GAME_HEADER_CHECKSUM - at)
				run = SAVED_GAME_HEADER_CHECKSUM - at;
			crc = saved_game_crc(crc, data + i, run);
			i += run;
		}
	}
	return crc;
}

/* the checksum a whole image's header holds */
static inline uint32_t saved_game_image_checksum(const uint8_t *data, size_t size)
{
	return saved_game_crc_run(SAVED_GAME_CRC_START, data, 0, size);
}

#endif
