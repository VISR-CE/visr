/* Apache-2.0. Backend-independent value producer; callers own the snapshot. */
#ifndef VISR_DISCORD_PRESENCE_H
#define VISR_DISCORD_PRESENCE_H
#include <stdint.h>
struct discord_presence {
    char name[32], details[256], state[64], image[16];
    int players, capacity;
    int64_t started;
};
#ifdef __cplusplus
extern "C" {
#endif
/* Feed public map/mode labels, never server addresses or player identities.
 * A missing map shows "In the menus" and clears the party and timestamp.
 * A zero capacity preserves a known player count without inventing a limit. */
void discord_presence_make(struct discord_presence *out, const char *map, const char *mode,
    int players, int capacity, int64_t started);
#ifdef __cplusplus
}
#endif
#endif
