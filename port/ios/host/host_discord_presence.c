/* Apache-2.0. A small host snapshot avoids racing the 32-bit game's memory. */
#include "host_discord_presence.h"
#include <pthread.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static const char *active_map;
static unsigned game_kind, players, engine, capacity;
static int64_t started;
static int menu_map, map_loading = 1, movie_playing;
static const struct { const char *key, *title; } maps[] = {
    {"a10", "The Pillar of Autumn"}, {"a30", "Halo"},
    {"a50", "The Truth and Reconciliation"}, {"b30", "The Silent Cartographer"},
    {"b40", "Assault on the Control Room"}, {"c10", "343 Guilty Spark"},
    {"c20", "The Library"}, {"c40", "Two Betrayals"}, {"d20", "Keyes"}, {"d40", "The Maw"},
    {"beavercreek", "Battle Creek"}, {"bloodgulch", "Blood Gulch"},
    {"boardingaction", "Boarding Action"}, {"carousel", "Derelict"},
    {"chillout", "Chill Out"}, {"damnation", "Damnation"},
    {"dangercanyon", "Danger Canyon"}, {"deathisland", "Death Island"},
    {"gephyrophobia", "Gephyrophobia"}, {"hangemhigh", "Hang 'Em High"},
    {"icefields", "Ice Fields"}, {"infinity", "Infinity"}, {"longest", "Longest"},
    {"prisoner", "Prisoner"}, {"putput", "Chiron TL34"}, {"ratrace", "Rat Race"},
    {"sidewinder", "Sidewinder"}, {"timberland", "Timberland"}, {"wizard", "Wizard"}
};

void host_discord_presence_map(const char *name)
{
    char key[64];
    const char *title = NULL;
    int menu = 0;
    if (name && *name) {
        const char *base = name;
        for (const char *p = name; *p; ++p)
            if (*p == '/' || *p == '\\') base = p + 1;
        size_t length = strcspn(base, ".");
        if (length >= sizeof(key)) length = sizeof(key) - 1;
        for (size_t i = 0; i < length; ++i)
            key[i] = base[i] >= 'A' && base[i] <= 'Z' ? base[i] + ('a' - 'A') : base[i];
        key[length] = 0;
        menu = !strcmp(key, "ui");
        if (key[0] && !menu) {
            /* Custom cache names may contain private text; publish only known titles. */
            title = "Custom map";
            for (size_t i = 0; i < sizeof(maps) / sizeof(maps[0]); ++i)
                if (!strcmp(key, maps[i].key)) { title = maps[i].title; break; }
        }
    }
    pthread_mutex_lock(&lock);
    active_map = title;
    menu_map = menu;
    game_kind = players = engine = capacity = 0;
    started = title ? (int64_t)time(NULL) : 0;
    pthread_mutex_unlock(&lock);
}

void host_discord_presence_loading(int loading)
{
    pthread_mutex_lock(&lock);
    map_loading = loading != 0;
    pthread_mutex_unlock(&lock);
}

void host_discord_presence_movie(int playing)
{
    pthread_mutex_lock(&lock);
    movie_playing = playing != 0;
    pthread_mutex_unlock(&lock);
}

int host_discord_presence_menu_ready(void)
{
    pthread_mutex_lock(&lock);
    int ready = menu_map && !map_loading && !movie_playing;
    pthread_mutex_unlock(&lock);
    return ready;
}

void host_discord_presence_context(unsigned field, uint64_t value)
{
    if (field != 2 && field != 4 && field != 24 && field != 25) return;
    pthread_mutex_lock(&lock);
    if (field == 2) game_kind = value == 1 || value == 2 ? (unsigned)value : 0;
    if (field == 4) players = value <= 128 ? (unsigned)value : 0;
    if (field == 24) engine = value <= 7 ? (unsigned)value : 0;
    if (field == 25) capacity = value <= 128 ? (unsigned)value : 0;
    pthread_mutex_unlock(&lock);
}

void host_discord_presence_snapshot(struct discord_presence *out)
{
    pthread_mutex_lock(&lock);
    /* Numeric game_engine_type values come from the game thread; custom
     * variant names never cross into the public presence. */
    static const char *const modes[] = {
        "Multiplayer", "Capture the Flag", "Slayer", "Oddball", "King of the Hill", "Race"
    };
    const char *mode = game_kind == 2 ? modes[engine < 6 ? engine : 0] :
        game_kind == 1 ? (players > 1 ? "Co-op" : "Campaign") : "Loading";
    discord_presence_make(out, active_map, mode, (int)players, (int)capacity, started);
    pthread_mutex_unlock(&lock);
}
