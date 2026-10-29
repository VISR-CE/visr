/* Apache-2.0. Public gameplay values shared with the Discord backend. */
#ifndef VISR_HOST_DISCORD_PRESENCE_H
#define VISR_HOST_DISCORD_PRESENCE_H
#include "discord_presence.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Called at map and numeric context hooks on the game thread. Context fields
 * 2/4 are game kind/player count; phases 24/25 are engine type/session limit. */
void host_discord_presence_map(const char *name);
void host_discord_presence_context(unsigned field, uint64_t value);
/* Copies host-owned values; never reads guest memory on the caller's thread. */
void host_discord_presence_snapshot(struct discord_presence *out);
#ifdef __cplusplus
}
#endif
#endif
