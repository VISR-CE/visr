/* Shared by the game and pure probes; publication never exposes a partial row. */
#include "../include/halo_delta_legacy.h"
#if defined(HALO_IOS) && defined(HALO_LINUX_PLATFORM_LAYER)
#include "posix.h"
#endif
#include "../include/halo_port_limits.h"
#include <pthread.h>
#include <string.h>
static pthread_mutex_t row_lock = PTHREAD_MUTEX_INITIALIZER;
static struct delta_legacy_row active;
void delta_legacy_runtime_set(const struct delta_legacy_row *row)
{
	pthread_mutex_lock(&row_lock);
	if (row) active = *row;
	else memset(&active, 0, sizeof(active));
#if defined(HALO_IOS) && defined(HALO_LINUX_PLATFORM_LAYER)
	/* Bonjour runs in the native host; mirror only these bounded scalar fields. */
	posix_delta_legacy_row(active.present ? active.minimum : 0, active.announce,
		active.maximum, active.proven_maximum, active.requires);
#endif
	pthread_mutex_unlock(&row_lock);
}
void delta_legacy_runtime_get(struct delta_legacy_row *row)
{
	pthread_mutex_lock(&row_lock);
	*row = active;
	pthread_mutex_unlock(&row_lock);
}
unsigned delta_legacy_announce_version(void)
{
	struct delta_legacy_row row;
	delta_legacy_runtime_get(&row);
	unsigned version;
	/* We implement no host protections, so any requires field keeps hosting at 38. */
	version = row.present && !(row.requires && row.announce > 38) ? row.announce : 38;
	/* Never below the base version, and never above the codec this build runs: a higher number makes
	   VISR clients add join-pass padding that a host on this codec does not expect. */
	if (version < 38) version = 38;
	if (version > HALO_PORT_NETWORK_VERSION) version = HALO_PORT_NETWORK_VERSION;
	return version;
}
int delta_legacy_joinable(unsigned version)
{
	struct delta_legacy_row row;
	delta_legacy_runtime_get(&row);
	return row.present && version > 41 && version >= row.minimum && version <= row.maximum;
}
int delta_legacy_presumed(unsigned version)
{
	struct delta_legacy_row row;
	delta_legacy_runtime_get(&row);
	return row.present && version > 41 && version >= row.minimum &&
		version > row.proven_maximum && version <= row.maximum;
}
int delta_legacy_fetch_due(uint32_t now, uint32_t next, int attempted)
{
	return !attempted || (int32_t)(now - next) >= 0;
}
uint32_t delta_legacy_fetch_next(uint32_t now, int success)
{
	return now + (success ? 4u * 60u * 60u * 1000u : 30u * 60u * 1000u);
}
