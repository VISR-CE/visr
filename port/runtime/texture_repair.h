/* Apache-2.0. Repairs banding that is baked into a texture's stored 5:6:5 values (spec
2026-11-23-halo-ce-ios-image-precision-design.md, "Part 1: repairing textures at import").

Lightmaps and many sky and fog textures were stored with 5 or 6 bits a channel, so a slow falloff became plateaus a step
apart. The filter rebuilds the smooth gradient those plateaus were rounded from, and never moves a texel farther than the
rounding could have: the result is one of the images the stored texture could have been rounded from, a repair and not a
repaint. Plain C with no dependencies, so the property tests run in public CI (tools/test_texture_repair.py).

Both entry points take one mip level and produce RGBA8 (alpha 255 except a DXT1 punch-through texel). Texels outside a
flagged area come out byte-identical to the plain decode (xbox_textures.c: bit replication, then the DXT1 palette's
integer mix). A caller repairs each level of a texture's stored mip tail on its own: every level has its own steps. */
#ifndef TEXTURE_REPAIR_H
#define TEXTURE_REPAIR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A texel is flagged when every channel of its (2r+1) x (2r+1) neighborhood spans at most two stored steps (detail and
edges span more). The smoothing never uses a neighbor that differs from the texel by more than two steps in any channel. */
#define TEXTURE_REPAIR_WINDOW_RADIUS 2
#define TEXTURE_REPAIR_PASSES 10

/* Raised whenever what this file and texture_repair.c produce for a texture changes (the filter, its limits, which
textures it applies to, the storing rule): the repair's own records and every upscaled texture are made again, since the
upscaler works from the repaired source (its global recipe carries this number). tools/test_texture_repair.py keeps the
digest of both files for each version, so a change to them fails until the version is raised. */
#define TEXTURE_REPAIR_VERSION 1u

/* the maps' bitmap formats the repair takes, and its largest texture */
#define TEXTURE_REPAIR_FORMAT_R5G6B5 6u
#define TEXTURE_REPAIR_FORMAT_DXT1 14u
#define TEXTURE_REPAIR_MAXIMUM_TEXELS (1024u * 1024u)

struct texture_repair_stats
{
	unsigned long texels;        /* opaque texels */
	unsigned long flagged;       /* texels the filter may move */
	unsigned long changed;       /* texels whose output differs from the plain decode */
	unsigned long stepped_pairs; /* adjacent flagged pairs one stored step apart in some channel */
};

/* a stored 5 or 6 bit value as the upload shows it (bit replication) */
unsigned texture_repair_expand(unsigned bits, unsigned stored);
/* the 8-bit values (low to high, inclusive) that round to the stored value at this depth, ties excluded */
void texture_repair_rounding_interval(unsigned bits, unsigned stored, int *low, int *high);

/* rgba: the level decoded the way the upload does (every texel an exact expansion of a stored R5G6B5 value), width x
height x 4 bytes. out gets the repaired level, same size. wrap: the level tiles (a sky or a detail texture; a lightmap
does not). Returns 0, or -1 when a texel is not an exact expansion (the caller's decode is not R5G6B5) or an argument is
bad. stats may be NULL. */
int texture_repair_r5g6b5(const uint8_t *rgba, unsigned width, unsigned height, int wrap, uint8_t *out,
	struct texture_repair_stats *stats);

/* blocks: the level's DXT1 blocks, 8 bytes each, rows of blocks top first, (width + 3) / 4 across. Limit: the larger of
half the spacing between a block's colors and half the endpoint step; a punch-through texel is never changed. */
int texture_repair_dxt1(const uint8_t *blocks, unsigned width, unsigned height, int wrap, uint8_t *out,
	struct texture_repair_stats *stats);

/* the plain decode of the same blocks (the reference the unflagged texels must equal) */
void texture_repair_dxt1_decode(const uint8_t *blocks, unsigned width, unsigned height, uint8_t *out);

/* Whether a texture's repair is worth storing: a lightmap whenever anything changed; anything else when large smooth
areas hold steps (flagged texels are at least a quarter of the opaque ones and a stepped pair comes at least every 32
flagged texels). */
int texture_repair_worth_storing(const struct texture_repair_stats *stats, int lightmap);

/* Whether the repair takes a texture of this map format and size. */
int texture_repair_applies(unsigned format, unsigned width, unsigned height);

/* A whole texture, every level: rgba is the upload's decode and stored the map's bytes (DXT1 reads its blocks), each
level at its offset. out gets the levels end to end, level i being max(1, width >> i) x max(1, height >> i) x 4 bytes
(a DXT1 level under 4 texels across is the decode as it is). Returns 1 when the repair changes the texture (what both
the repair's records and the upscaler's sources use), 0 when it leaves it as it is (out is then not meant to be used),
-1 when the repair does not apply or a level is refused. sum (may be NULL) gets every level's stats added. The
lightmap rule of texture_repair_worth_storing goes with R5G6B5. */
int texture_repair_texture(unsigned format, unsigned width, unsigned height, unsigned level_count, const uint8_t *rgba,
	const size_t *level_offsets, const uint8_t *stored, const size_t *level_stored_offsets, uint8_t *out,
	struct texture_repair_stats *sum);

/* The property tests, independent of the filter's tables. Each returns the number of violating texels (0: a repair, not a
repaint), or -1 for a bad argument. R5G6B5: every texel rounds back (exact rational rounding, no tie) to the stored value,
and every texel outside the flagged area is byte-identical. DXT1: every opaque texel is within the limit of the palette
color it had, measured against the block's original endpoints, and a punch-through texel is unchanged.
A texel outside the filter's flagged area must equal the plain decode. */
long texture_repair_verify_r5g6b5(const uint8_t *original_rgba, const uint8_t *repaired_rgba, unsigned width,
	unsigned height, int wrap);
long texture_repair_verify_dxt1(const uint8_t *blocks, const uint8_t *repaired_rgba, unsigned width, unsigned height,
	int wrap);

#ifdef __cplusplus
}
#endif

#endif
