/* Signed legacy configuration; no game or platform headers are required. */
#ifndef HALO_DELTA_LEGACY_H
#define HALO_DELTA_LEGACY_H
#include <stddef.h>
#include <stdint.h>
#define DELTA_LEGACY_DOCUMENT_MAX 16384
#define DELTA_LEGACY_SIGNED_MAX (129 + DELTA_LEGACY_DOCUMENT_MAX)
#define DELTA_LEGACY_PRESUMED 1
struct delta_legacy_key { uint8_t public_key[32]; unsigned last_epoch; };
struct delta_legacy_row
{
	uint32_t serial;
	unsigned minimum, announce, maximum, proven_maximum;
	int present, requires;
	char follows[32];
};
struct delta_legacy_table
{
	struct delta_legacy_row row;
	unsigned disabled_count;
	char disabled[16][32];
};
int delta_legacy_verify(const void *bytes, size_t size, const struct delta_legacy_key *keys,
	size_t key_count, struct delta_legacy_table *table);
/* Configure before networking starts, or reset when the kill switch changes.
   NULL keys selects the published keys.
   A disabled table ignores both offers and the cache. Keys are copied. */
void delta_legacy_initialize(int enabled, const char *save_root,
	const struct delta_legacy_key *keys, size_t key_count);
int delta_legacy_offer(const void *bytes, size_t size);
uint32_t delta_legacy_serial(void);
/* Copy a coherent signed table; returns zero when absent or capacity is too small. */
size_t delta_legacy_signed(void *bytes, size_t capacity);
int delta_legacy_capability_disabled(const char *name);
int delta_legacy_cache_read(void);
int delta_legacy_cache_write(void);
/* Runtime row is separate so small probes need only delta_legacy_runtime.c. */
void delta_legacy_runtime_set(const struct delta_legacy_row *row);
void delta_legacy_runtime_get(struct delta_legacy_row *row);
unsigned delta_legacy_announce_version(void);
int delta_legacy_joinable(unsigned version);
int delta_legacy_presumed(unsigned version);
/* Milliseconds, wrap-safe for the network worker's 32-bit clock. */
int delta_legacy_fetch_due(uint32_t now, uint32_t next, int attempted);
uint32_t delta_legacy_fetch_next(uint32_t now, int success);
#endif
