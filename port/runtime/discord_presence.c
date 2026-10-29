/* Apache-2.0. Shared by future SDK and IPC consumers. */
#include "discord_presence.h"
#include <stdio.h>
#include <string.h>

void discord_presence_make(struct discord_presence *out, const char *map, const char *mode,
    int players, int capacity, int64_t started)
{
    memset(out, 0, sizeof(*out));
    snprintf(out->name, sizeof(out->name), "VISR: CE");
    snprintf(out->image, sizeof(out->image), "visr");
    if (!map || !*map) {
        snprintf(out->details, sizeof(out->details), "In the menus");
        return;
    }
    /* Bound each field so truncation cannot split the separator's UTF-8 bytes. */
    snprintf(out->details, sizeof(out->details), "Halo: Combat Evolved — %.96s, %.96s",
        map, mode && *mode ? mode : "Unknown mode");
    out->capacity = capacity > 0 ? capacity : 0;
    out->players = players > 0 ? players : 0;
    if (out->capacity && out->players > out->capacity) out->players = out->capacity;
    if (out->capacity) snprintf(out->state, sizeof(out->state), "%d/%d players", out->players, out->capacity);
    else if (out->players) snprintf(out->state, sizeof(out->state), "%d players", out->players);
    out->started = started > 0 ? started : 0;
}
