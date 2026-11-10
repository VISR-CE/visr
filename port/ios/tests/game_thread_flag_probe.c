#include "host_config.h"
#include "host_game_thread.h"
#include <SDL3/SDL_events.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *configured;
static int config_reads, pumps, polls, peeps;
static int on_main = 1, peep_result = 1;

void host_config_string(const char *name, const char *fallback, char *out, size_t size) {
    assert(strcmp(name, "debug.game_thread") == 0);
    assert(strcmp(fallback, "false") == 0);
    config_reads++;
    snprintf(out, size, "%s", configured ? configured : fallback);
}

int host_on_main_thread(void) {
    return on_main;
}

void SDL_PumpEvents(void) {
    assert(on_main);
    pumps++;
}

bool SDL_PollEvent(SDL_Event *event) {
    polls++;
    event->type = SDL_EVENT_QUIT;
    return polls == 1;
}

int SDL_PeepEvents(SDL_Event *event, int count, SDL_EventAction action, Uint32 first, Uint32 last) {
    assert(count == 1);
    assert(action == SDL_GETEVENT);
    assert(first == SDL_EVENT_FIRST && last == SDL_EVENT_LAST);
    peeps++;
    event->type = SDL_EVENT_QUIT;
    return peep_result;
}

/* tools/ios_test.py extracts the production helper without the rest of host_sdl.c. */
#include "game_thread_next_event.inc"

int main(int argc, char **argv) {
    assert(argc == 1 || argc == 2);
    configured = argc == 2 ? argv[1] : NULL;
    int expected = configured && strcmp(configured, "true") == 0;
    assert(host_game_thread_enabled() == expected);
    assert(config_reads == 1);
    configured = expected ? "false" : "true";
    assert(host_game_thread_enabled() == expected);
    assert(host_game_thread_enabled() == expected);
    assert(config_reads == 1);
    assert(pumps == 0);
    host_event_pump();
    assert(pumps == 1);
    SDL_Event event;
    assert(next_event(&event));
    assert(event.type == SDL_EVENT_QUIT);
    peep_result = 0;
    assert(!next_event(&event));
    peep_result = -1;
    assert(!next_event(&event));
    assert(pumps == (expected ? 4 : 1));
    on_main = 0;
    peep_result = 1;
    assert(next_event(&event) == expected);
    assert(pumps == (expected ? 4 : 1));
    assert(polls == (expected ? 0 : 4));
    assert(peeps == (expected ? 4 : 0));
    assert(config_reads == 1);
    puts("PASS: cached game-thread flag; legacy poll or main pump and peep drain");
    return 0;
}
